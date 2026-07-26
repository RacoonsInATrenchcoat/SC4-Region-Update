# Project Summary

## The problem, briefly

SimCity 4 holds one city in memory at a time. A region is a grid of city tiles,
but only the open tile is simulated; the rest are files on disk. Cross-border
figures (residential/commercial/industrial demand, commuter flow between tiles,
and shared power and water) are read from a neighbour's saved state at the moment
a city loads. Change city A and the effect does not appear in adjacent city B
until B is reopened, given time to simulate, and saved again.

For a technical reader: there is no background simulation and no message-passing
between tiles. The simulator (`cISC4Simulator`) runs against a single
`cISC4City` context. Neighbour data is a load-time snapshot, not a live feed.

## Scope

In scope: automating the load / simulate / save cycle across a region's tiles so
that neighbour data resynchronises without manual play, safely and without
corrupting saves.

Out of scope: true concurrent simulation of unloaded tiles (a "living region").
That would require reimplementing the simulator and is not feasible.

## Approaches considered

### A. External input automation (macro)
A script (e.g. AutoHotkey) drives the region view: click tile, wait, set speed,
wait, save, exit, repeat.
- Pros: buildable immediately, no reverse engineering.
- Cons: no reliable signal for "load finished" or "save finished", so timing is
  guessed; an interrupted save risks a corrupted tile. Suitable as a measurement
  harness, not as the final product.

### B. In-game DLL plugin (chosen target)
A plugin built on the gzcom-dll framework, exposed as a region-view control or
cheat, that drives the same cycle from inside the process.
- Pros: uses the game's own callbacks to know exactly when load and save
  complete; can adjust sim speed and rendering for the pass and restore them; can
  enumerate tiles from region data; crash-safe by construction.
- Cons: C++, x86, and requires identifying the city-load entry point, which no
  published plugin currently uses.

### C. Direct savegame editing
Write resynchronised values straight into neighbouring `.sc4` files offline.
- Ruled out. The values needed (demand caps, commute allocations, RCI figures)
  are simulator outputs. Producing them correctly means reimplementing the
  simulator; producing them incorrectly means silent, delayed save corruption.
  Reading save data offline is safe and useful; writing derived state is not.

### D. Parallel game instances
Run several copies of the game to refresh tiles concurrently.
- Deprioritised. All instances share one region folder (corruption risk on
  region-level writes); neighbour sync is order-dependent, not embarrassingly
  parallel, so concurrent passes give one propagation step, not convergence; and
  multi-instance support is unverified. Complexity far exceeds the benefit.

## Chosen direction

A **Planner + Executor** split:

- The Planner is pure and read-only. It reads `config.bmp` for the region grid
  and each save's region-view data for tile position and size, computes
  edge-adjacency, and emits an ordered, inspectable refresh queue. It can be
  validated by eye before anything touches a save.
- The Executor is the Route B DLL. It consumes the queue and performs the
  cycle, locking out input and updating a manifest of what has been synced.

This isolates all risk in a small, dumb component fed by a verifiable plan, which
matters because the failure mode is a corrupted region.

## Adjacency model

The region is a grid of 1 km cells (`config.bmp`). Tiles are 1x1 (small), 2x2
(medium), or 4x4 (large). Two tiles are neighbours if their rectangles share at
least one cell-length of common edge. Corner-only contact is not adjacency, and
is in fact associated with a known commuter bug, so treating it as adjacency
would be wrong.

Refresh selection, in increasing sophistication:
- v1: every established tile in the region.
- v2 (default): every tile sharing an edge with a changed tile.
- v3 (later, optional): only tiles with an actual connection crossing the shared
  edge. This is a performance optimisation; correctness never depends on it,
  because skipping a truly connected tile is the original bug, whereas refreshing
  an unconnected one is a harmless no-op.

## Key unknown

Whether a plugin can programmatically load a named city tile. The game's own
region view does this on every click, so the code path exists in-process; the
question is whether it sits on a reachable interface. This is the one thing that
decides pure-mod versus mod-plus-macro, and it is the priority to resolve.

## Evidence base

Existing published plugins already prove most of the required capabilities
individually: programmatic saving including a fast-save path, hooking the city
load pipeline, persisting state across a city exit within a region, and
enumerating every tile's statistics from region view without loading them. The
gzcom-dll framework and the SC4Fix loader make DLL plugins viable on the current
Steam build. What is unproven is only the city-load command above.
