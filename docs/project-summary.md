# Project Summary

## The problem, briefly

SimCity 4 holds one city in memory at a time. A region is a grid of city tiles,
but only the open tile is simulated; the rest are files on disk. Cross-border
figures (residential/commercial/industrial demand, commuter flow, and shared
power and water) are read from a neighbour's saved state at the moment a city
loads. Change city A and the effect does not appear in adjacent city B until B is
reopened, given simulation time, and saved.

For a technical reader: there is no background simulation and no message-passing
between tiles. The simulator (`cISC4Simulator`) runs against a single
`cISC4City` context. Neighbour data is a load-time snapshot, not a live feed.

## Scope

In scope: automating the load / simulate / save cycle across a region's active
tiles so that neighbour data resynchronises without manual play, safely and
without corrupting saves.

Out of scope: true concurrent simulation of unloaded tiles (a "living region").
That would require reimplementing the simulator and is not feasible.

## What the investigation established

The mechanism was characterised empirically (see `experiment-log.md`). Two
subsystems bracket the problem:

- **Utilities (power, water)** update at **load time**, instantly, with zero
  elapsed game time. A consuming tile recomputes its intake from a neighbour's
  saved file the moment it opens. A *producing* tile, however, reconciles its
  outgoing deals only at a **month-end checkpoint**, and only against the
  neighbour's *already-saved* state.
- **Commute and RCI demand** are a **simulated equilibrium**, re-derived over
  **years** of in-game time, not read at load. Changes appear gradually, in
  steps, and some coupled figures may never fully plateau.

Two consequences shaped the design:

1. **Order matters.** Because a producer reconciles against a neighbour's saved
   file, the changed/consuming side must be saved before the other side is
   processed. And where a shared source serves competing consumers, the game has
   no global arbiter — scarce supply is allocated first-come, by load order. So
   traversal order affects not just efficiency but outcomes.
2. **There is no reliable "done" signal.** Metrics settle at different rates,
   some transiently move the wrong way before settling, and the simulation never
   fully stops changing. Convergence detection is therefore not viable.

## Approaches considered

### A. External input automation (macro)
A script drives the region view: click tile, wait, save, exit, repeat.
- Buildable immediately, but no reliable signal for load/save completion, so
  timing is guessed and an interrupted save risks corruption. Retained only as a
  possible fallback for the single "load a tile" step, and as a measurement idea.

### B. In-game DLL plugin (chosen)
A plugin on the gzcom-dll framework, driving the cycle from inside the process.
- Knows exactly when load and save complete via the game's callbacks; can control
  speed; can enumerate tiles from region data; crash-safe by construction. Costs:
  32-bit C++, and identifying the "load a named tile" call, which no published
  plugin currently uses.

### C. Direct savegame editing
Write resynchronised values straight into neighbouring saves offline.
- Ruled out. The needed values are simulator *outputs*; deriving them correctly
  means reimplementing the simulator, and deriving them wrongly corrupts saves
  silently. Read-only save parsing is retained as safe; write-back is not.

### D. Parallel game instances
Run several game copies to process tiles concurrently.
- Deprioritised. Shared region folder (corruption risk), order-dependent
  propagation (one step per pass, not convergence), and unverified multi-instance
  support. Cost far exceeds benefit.

## Final design

### Architecture: Planner + Executor
- **Planner** (read-only): parses `config.bmp` for the region grid (each pixel a
  1 km cell; red/green/blue = small/medium/large tile), determines which cells
  hold established cities, computes edge-adjacency, and emits an ordered tile
  list. Pure and inspectable before anything touches a save.
- **Executor** (in-game DLL): consumes the list and performs the cycle. Detects
  the active region and its tiles by querying the game in-process (the same data
  the region census plugin reads), not by parsing filenames — filenames encode
  the city name, not the cell position, which lives inside each save.

### The refresh loop
Configuration lives in an `.ini` (traversal order; Y; Z), user-overridable.
On run:
1. Back up the whole region folder as a new timestamped copy; keep the newest
   five, deleting the oldest when making a sixth (never overwrite the most
   recent).
2. Build the ordered list of active tiles (a tile is active if an established
   save exists for its cell; never-used empty cells are excluded).
3. Take control: lock user input; show a non-blocking progress panel.
4. For Z passes, over the tiles in the chosen order, for each active tile:
   load it; run the simulation until the in-game date advances by Y; save; exit.
   Terminating on in-game date (not a real-time timer) is correct regardless of
   machine speed or FPS.
5. Update a plain-text progress marker after each tile (for crash recovery).
6. On completion: release control, return to region view, mark the run finished,
   and show a terminal notification.

### Traversal order
A user `.ini` setting: forward (grid start → end), reverse, or circular from the
last-selected tile. Positional, not dependency-based. It is both a propagation
hint and — in scarcity — the user's lever over which tile wins contention.

### Y and Z
- **Y**: in-game months run per tile per pass. Default ~1 year, because commute/
  RCI settling dominates and needs years; floor is one month-end so utility
  producers reconcile. A low Y acts as a fast utility-oriented pass; a high Y as
  a full resync. One knob spans both.
- **Z**: number of full passes. Default ≥ 2, because a producer left stale in one
  pass (its consumer not yet saved) is reconciled in a later pass. This is how the
  order-blind design handles the producer/consumer asymmetry without tracking it.

### What the tool deliberately does NOT do
- No dependency graph or cycle detection. A directed tile-dependency graph would
  give optimal ordering but requires parsing network/power/water/deal subfiles and
  resolving state-dependent, reversible flow directions — a reverse-engineering
  cost exceeding the rest of the tool, and it does not help the coupled-cycle case
  at all. Bounded Y×Z passes absorb cycles without needing to detect them.
- No convergence detection (no reliable stability metric; the sim never settles).
- No claim of a uniquely-correct region. In contention cases there isn't one; the
  tool produces *a* consistent region under a chosen deterministic order, which is
  what manual play order does anyway.

## Adjacency model

Two tiles are neighbours if their rectangles share at least one cell-length of
common edge AND a connection (road, rail, power, or water) crosses that edge.
- Corner-only contact is not adjacency — the game offers no connectable border
  across a corner (confirmed empirically), and corner connections are associated
  with a known commuter bug.
- A shared edge with no connection transmits nothing — no utility, no RCI
  propagation (confirmed empirically).

Refresh selection:
- v2 (default): every active edge-neighbour. Never wrong, sometimes wastes work.
- v3 (later, optional): only tiles with an actual connection across the edge.
  A real saving on sparse regions — because skipping an unconnected neighbour
  wastes nothing, and each tile costs years to process — but requires
  connection-detection code. Correctness never depends on it.

## Speed

The simulation is tick-locked, not render-bound: SC4 caps frames at 30/20/15 FPS
for Turtle/Rhino/Cheetah and advances one sim step per frame. Reducing render load
does nothing while the cap is in place. Lifting the cap (the Disable FPS Limits
mod raises it up to 255) speeds the simulation directly; once lifted, rendering
becomes the bottleneck, so low detail / small window / zoom-out then help.

The tool does not implement its own cap-lift, to avoid a memory-write conflict
with the standalone FPS mod and to avoid maintaining a moving memory target. It
depends only on the FPS mod's *effect* (Cheetah runs faster), not its
implementation — no coupling, no version dependency. Absent the FPS mod, the tool
degrades gracefully to vanilla Cheetah speed (slower, fully functional).

## Dependencies and compatibility

- **Framework** (gzcom-dll headers, SC4Fix loader): required at *build* time,
  bundled/transparent at install (the user drops one DLL into the Plugins folder).
- **Disable FPS Limits**: recommended, not required. Big speed gain; composes
  with no coordination.
- **NAM / CAM / other gameplay mods**: optional, compatible not dependent. They
  change the magnitudes and timing of the slow subsystems (NAM the traffic
  simulator, CAM the growth stages and demand caps) but not the mechanisms, which
  the variable Y absorbs. Build-phase calibration: tune default Y against NAM/CAM.

## Build

- Language: 32-bit (x86) C++, matching SC4's process, via the gzcom-dll framework.
- Reference plugins share a skeleton: a COM director class, `PostCityInit` /
  `PreCityShutdown` hooks, mINI for `.ini` reading, spdlog for logging. Adapt this
  template rather than starting from scratch.
- **First milestone:** compile and run the gzcom-dll "city loaded" popup demo,
  and confirm it runs in-game. This proves the entire toolchain before any of the
  tool's own logic is written.
- Distribution: target sc4pac compatibility. DLL and `.ini` go in the top-level
  Plugins folder (DLLs cannot live in subfolders). Keep versioning and folder
  layout clean from the first commit.
- **Director ID: 0xE04809A9** (SC4RegionUpdate's unique plugin identifier).
Randomly generated; needed to remain unique among co-installed SC4 DLL plugins.

## Known unknowns (build-phase, not design-phase)

1. The "load a named city tile from code" call — the one capability no published
   plugin uses. The game's region view does it on every click, so the path exists
   in-process; confirming the interface is the first real build risk.
2. Whether the simulation continues while the game window is unfocused (audio
   stops on tab-out; sim continuation must be verified).
3. Calibration of default Y against vanilla, NAM, and CAM.
4. Confirming CAM's demand-cap changes don't disturb the connection-gated vs
   ambient-regional-demand distinction.
