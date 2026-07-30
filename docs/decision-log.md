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
