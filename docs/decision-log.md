# Decision Log

Chronological record of notable decisions and their rationale. Newest last.
Grouped by working session.

## Session 1 — framing and approach
- **D1.** Treat the limitation as engine-level, not a bug to patch out. Target a
  workaround around the load/simulate/save cycle.
- **D2.** Rule out true background simulation of unloaded tiles (would require
  reimplementing the simulator).
- **D3.** Rule out direct savegame overwriting of sync values (Approach C): the
  values are simulator outputs; deriving them correctly ≈ rebuilding the
  simulator, errors corrupt saves. Offline *reading* retained as safe.
- **D4.** Deprioritise parallel game instances (Approach D): shared region folder,
  order-dependent propagation, unverified multi-instance support.
- **D5.** Target the in-game DLL plugin (Approach B). Macro (A) retained only as a
  measurement harness / fallback for the tile-load step.
- **D6.** Adopt a Planner + Executor architecture — isolate all write risk in a
  small component driven by a read-only, inspectable plan.
- **D7.** Use edge-adjacency (v2) as the default refresh rule; corner contact
  excluded; connection-aware (v3) deferred.

## Session 2 — signalling and passes
- **D8.** No modal dialog as the machine signal (human signal, poor for
  automation). Prefer a window-title status token backed by a status file; a
  confirm-each-step dialog is an opt-in debug aid only.
- **D9.** Prefer an in-process state machine over macro timing.

## Mechanism findings
- **D10.** Utility neighbour data updates at load time, zero ticks (a2). Utility
  resync needs no simulate phase.
- **D11.** Measure metrics, not advisor/icon cues (they lag) (a2).
- **D12.** Import/export asymmetry (a2b): consumer self-corrects on load; producer
  retains stale outgoing state.
- **D13.** Refresh order matters; producers reconcile at month-end against the
  neighbour's SAVED file (a2c). Update/save the changed side before reconciling the
  other; run producer tiles to at least the next month-end; multi-hop changes may
  need ordered/repeated passes.
- **D14.** The tool targets ordered reconciliation, not provable correctness.
  Linear dependencies solved by one ordered pass; cycles handled by bounded
  iteration, not solved. Edge direction can flip between passes.

## Architecture decisions
- **D15.** Traversal order is a user option, positional not dependency-based:
  forward / reverse / circular-from-selected via `.ini`. No graph required.
- **D16.** Refresh model is fixed "Y months × Z passes", user-overridable. Reject
  "repeat-until-stable": no principled stability metric; oscillating regions never
  settle; more passes is not strictly safer.
- **D17.** Dependency graph and cycle detection: considered, DEFERRED (not built).
  Cost exceeds benefit; bounded iteration absorbs cycles without detecting them.

## Speed and config
- **D18 / D18a.** Simulation speed is the pivot for how crude the algorithm can be.
  Revised: sim speed is tick-locked, not render-bound; the lever is the FPS/
  tick-rate limit, not render suppression.
- **D19.** Region geometry from `config.bmp`. External planner needs the region
  name/path; in-process executor asks the game for the active region.
- **D20.** Custom in-game UI is feasible (precedent: interactive controls, region-
  view panels, new menus in existing plugins) but DEFERRED to v2; v1 ships with
  `.ini`.

## Self-sufficiency and correctness
- **D21.** (Superseded by D30.) Originally: build a dual vanilla/fast speed mode.
- **D22.** Licensing/attribution discipline from day one. Prefer implementing
  mechanisms ourselves over copying third-party code; note gzcom-dll is LGPL v2.1+,
  reference plugins mostly MIT; maintain an ATTRIBUTIONS file.
- **D23.** Simulation phase terminates on in-game date, not wall-clock —
  machine- and speed-independent.
- **D24.** Commute/RCI needs YEARS of simulation (a2-commute/a3); sets Y large.
  Minimise tiles per pass, keep Z low, treat speed as important.
- **D25.** Neighbour = shared edge AND a connection across it (a7, a8). Corner
  contact excluded; v2 default, v3 upgraded to "worth building" since skipping
  unconnected neighbours provably wastes nothing and tiles are expensive.
- **D26.** No global resource arbiter; in scarcity, order decides allocation (a6).
  Traversal order is an outcome-determining setting. The tool promises *a*
  consistent region under a chosen order, not THE correct one.

## Pre-build consolidation
- **D27.** Speed lever confirmed as the FPS cap (validates D18a). Fast simulation
  via lifting the cap; render suppression then helps; open MIT-friendly reference
  exists (caspervg/sc4-disable-fps-limits).
- **D28.** NAM/CAM don't break findings; they shift magnitudes/timing of the slow
  subsystems (NAM: traffic sim; CAM: growth stages/demand caps). Variable-Y absorbs
  this. Build-phase: calibrate default Y against NAM/CAM; verify CAM's cap changes
  don't disturb the connection-gated vs ambient split.
- **D29.** Build/packaging: 32-bit C++ via gzcom-dll; first milestone is the
  city-loaded popup demo to prove the toolchain; adapt the reference-plugin
  skeleton (COM director, PostCityInit/PreCityShutdown, mINI, spdlog); target
  sc4pac compatibility, DLL + `.ini` in top-level Plugins.
- **D30.** Dependency posture: standalone at runtime; framework-only at build. Do
  NOT build our own FPS-unlock — recommend the standalone Disable FPS Limits mod
  ("dependency-lite") to avoid a memory-write conflict and to avoid maintaining a
  moving target. The tool depends only on the FPS mod's effect, not its
  implementation; absent it, degrades gracefully to vanilla Cheetah. This
  supersedes D21's dual-mode: the tool always sets Cheetah and runs to a date, and
  speed is determined by the user's setup. Simpler, fewer conflict surfaces.
- **D31. Prior-art confirmed absent; one hard build requirement surfaced.** No tool
  does region-wide neighbour resync; the project is novel (community works around it
  manually or via multiplayer sync). Hard requirement: backups go OUTSIDE the live
  region folder — the game deletes/relocates duplicate cities at startup, keeping
  only the alphabetically-first. Note: richer commute metrics available via Null45's
  RCI DLL if the graph-based measurement confound needs resolving.
**D32. Backup location: under the mod's own folder tree, outside Regions/.**
Backups written to e.g. Documents/SimCity 4/Region-Updater-Mod/Backup/<RegionName>/
— a sibling of Regions/, never inside it, so the game's duplicate-city
deletion/relocation at startup (D31) cannot affect them. Tool creates the backup;
the user decides when to restore/overwrite. Confirms D31's hard requirement with a
concrete path.

**Decision (signal choice): prefer semantic alignment over coincidental timing.**
kMessageSetRadioStation fires reliably at load and was proposed as the "city
ready" trigger. Rejected as PRIMARY despite working: it is an audio-setup signal
that only coincides with load timing, making it fragile (audio mods/patches could
move it) and semantically confusing. Chosen instead:
- kSC4MessagePostCityInitComplete (0xEA8AE29A): city fully initialised.
- kSC4MessageConnectionsReady (0x6AC284F3): neighbour connections live - the
  signal whose MEANING matches this tool's purpose (neighbour sync).
Also clarified: the hello-world box appearing "on the loading screen" is an
artifact of the modal MessageBox blocking the thread mid-transition, NOT evidence
that PostCityInitComplete is too early. The real tool's non-blocking actions will
not have this artifact.

**Decision (two-phase load handling): prime early, act after settle.**
Split the tool's per-tile reaction into two phases:
- Prime at kSC4MessagePostCityInitComplete: set internal state, mark the tile as
  in-progress. Cheap and safe.
- Act at kSC4MessageConnectionsReady (after the game's own load-time hidden-pause
  settles): change speed, run the simulate phase. Deferring the SPEED change
  avoids fighting the game's load-time pause/speed management (SimHiddenPauseChange
  fires during load). Rationale from the load message sequence.

  **Build finding: ConnectionsReady fires on EVERY city load, even isolated tiles.**
Tested a tile with zero neighbour connections (island/solitary case). 
kSC4MessageConnectionsReady still fired on load, both entries. Conclusion: it
signals "connection-loading step complete", NOT "connections exist". Therefore
safe as the universal "act" trigger, an isolated tile will not hang the tool
waiting for a signal that never comes. The milestone sequence
(PostCityInitComplete -> SimHiddenPauseChange -> ConnectionsReady -> SimNewDay)
is stable across connected and isolated tiles.

**Decision: verbose enumeration logging is a Debug-level, settings-controlled option.**
The per-tile enumeration output logs at LogLevel::Debug (summary counts at Info).
The .ini exposes a log-level setting (default Error/Info). Users troubleshooting
set it to Debug to get full tile output. Reuses the existing Logger levels rather
than bespoke on/off plumbing. Implement when Settings handling is built.
