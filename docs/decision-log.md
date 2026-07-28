# Decision Log

Chronological record of notable decisions and their rationale, newest last.
Entries are dated by working session, not calendar precision.

## Session 1: framing and approach

**D1. Treat the limitation as engine-level, not a bug to patch out.**
SC4 simulates one city context at a time by design. Accepted as a fixed
constraint; the project targets a workaround around the load/simulate/save
cycle, not a change to the simulator.

**D2. Rule out true background simulation of unloaded tiles.**
Would require reimplementing the simulator. Out of scope.

**D3. Rule out direct savegame overwriting of sync values (Approach C).**
The values are simulator outputs; deriving them correctly is equivalent to
rebuilding the simulator, and errors corrupt saves silently. Offline *reading*
of saves retained as a safe, useful technique for the Planner.

**D4. Deprioritise parallel game instances (Approach D).**
Shared region folder, order-dependent propagation, and unverified multi-instance
support. Cost exceeds benefit.

**D5. Target the in-game DLL plugin (Approach B) as the primary route.**
Robustness from the game's own load/save callbacks outweighs the C++ and
reverse-engineering cost. External macro (Approach A) retained only as a
measurement harness and as a fallback for the single tile-load step.

**D6. Adopt a Planner + Executor architecture.**
Isolates all write risk in a small component driven by a read-only, inspectable
plan. Chosen specifically because the failure mode is region corruption.

**D7. Use edge-adjacency (v2) as the default refresh rule.**
Corner-only contact excluded. Connection-aware filtering (v3) deferred as a
performance optimisation, since its errors fall on the safe side.

**D8. Signal design: no modal dialog as the machine signal.**
A blocking popup is a human signal, poor for automation and worse for unattended
runs. Preferred signals: a status token in the window title, backed by a status
file. A confirm-each-step dialog is retained only as an opt-in debug aid.

**D9. Prefer an in-process state machine over macro timing.**
The plugin counts in-game months itself and drops sim speed to normal before
saving, rather than the macro sleeping for a guessed duration.

**D10. Utility neighbour data confirmed to update at load time (a2).**
Zero-tick load of a neighbour ingests updated cross-border utility state.
Utility resync therefore needs no simulate phase. Commute/RCI unproven; assume
they may still require ticks until a3/a5 show otherwise.

**D11. Measure metrics, not advisor/icon cues.** Visual notifications lag or
diverge from underlying metric changes (observed in a2). All subsequent tests
read numeric metrics directly.

**D12. Import/export asymmetry confirmed (a2b).** Consumer side self-corrects on
zero-tick load; producer side retains stale export/deal records. Tool cannot
assume refreshing neighbours alone yields a globally consistent region; the
producing tile's outgoing state needs explicit handling. Deal-vs-physical-flow
distinction still to be resolved.

**D13. Refresh order matters; producer tiles need a month-end (a2c).** Consumers
self-heal on load (zero ticks); producers reconcile outgoing deals at month-end
against the neighbour's SAVED file. Therefore: (a) update/save the changed side
before reconciling the other side; (b) run producer tiles to at least the next
month-end rather than zero-tick save; (c) multi-hop changes may need ordered or
repeated passes to converge. Exact trigger (month-end vs periodic checkpoint) to
be confirmed.

**D14. The tool targets ordered reconciliation, not provable correctness.**
Linear (acyclic) dependencies are solved by one topologically-ordered pass.
Resource cycles (e.g. A<->B power coupling, C coupled to both) have no single
correct order and may converge, oscillate, or diverge. Approach: topologically
order the acyclic part; handle cyclic components by bounded repeat-until-stable
iteration with a change threshold and a max-pass cap; report if unsettled. Edge
direction can flip between passes (importer becomes exporter), so static
single-pass ordering is insufficient for coupled resources. Justification: manual
play never solves cycles either; the honest, achievable promise is "every tile
reopened and saved in a defensible order, cascades propagated within N passes".

**D15. Traversal order is a user option, positional not dependency-based.**
Planner already yields an ordered tile list from config.bmp. Offer
forward (0->X), reverse (X->0), and circular-from-selected as an .ini setting
(TraversalOrder). Circular uses the city-exit/selection hook. No graph required.
Lets a knowledgeable user hint propagation direction cheaply.

**D16. Refresh model is fixed "Y months x Z passes", user-overridable.**
Y = in-game months run per tile per pass (floor: cross at least one month-end,
per a2c producer reconciliation). Z = number of full region passes. Defaults TBD
once commute N is known. Rejected "repeat-until-stable": no principled stability
metric across subsystems; oscillating regions never settle; and more passes is
not strictly safer (propagates real failures further). Bounded, deterministic,
user-controlled is the honest and safer design.

**D17. Dependency graph and cycle detection: considered, DEFERRED (not built).**
A directed tile-dependency graph would give optimal acyclic ordering via
topological sort, but requires parsing network/power/water/deal subfiles per save
and resolving state-dependent, reversible flow directions - a reverse-engineering
cost exceeding the rest of the tool. Payoff over positional traversal + Z passes
is marginal, and it does not help the coupled-cycle case at all. Bounded
iteration (D16) absorbs cycles without needing to detect them. Documented as a
conscious scope decision.

**D18a. Revised: sim speed is tick-locked, not render-bound (d3).** The speed
lever is the tick-rate data in SimCity_1.dat (third-party Super-Speed Mod
precedent), not render suppression. This raises the risk/cost of the
"crude multi-pass" strategy: extra speed is available only via a data mod with a
stability cost, not for free via low detail. Reinforces keeping Y/Z
user-bounded and saving at normal speed.

**D19. Region geometry comes from config.bmp; region identity differs by
component.** Planner parses config.bmp (pixel->cell, colour->size, contiguous
blocks->tiles, "other"->empty) to reconstruct the tile inventory. An EXTERNAL
planner needs the region name/path (via .ini). An IN-PROCESS executor does NOT:
it asks the game for the active region (same interfaces sc4-region-census uses).
Reinforces Planner/Executor separability.

**D20. Custom in-game UI is feasible (precedent found), but deferred to v2.**
DLL plugins already add interactive controls to SC4 UI (sc4-more-building-styles:
checkboxes/radio buttons via UI template + memory patch; sc4-query-ui-hooks:
dialog hooks; sc4-region-census: region-view panel; submenus-dll: new menus).
UI is authored in the game's resource format (iLive's Reader/TGI), a distinct
toolchain. v1 ships with .ini config; in-game panel is a real v2 target. Third
path noted: a browser-served settings page via a plugin TCP server (precedent:
sc4 web-interface plugin), avoiding native UI authoring.

**D21. Dual speed mode, user-selectable, self-contained.** .ini SpeedMode=
vanilla|fast. Vanilla drives tiles at the game's Cheetah, never altering tick
rate. Fast applies an elevated tick rate for the simulate phase only, dropping to
normal before each save (required anyway per a2c/save-stability). Shared
load/simulate/save skeleton; modes differ by one speed call. Default: vanilla
(fast carries the tick-rate stability risk from D18a). The tick-rate logic is to
be implemented within this mod, NOT as a dependency on the third-party
Super-Speed Mod.

**D22. Licensing/attribution discipline from day one.** Prefer reverse-
engineering the tick-rate mechanism ourselves (lean on documentation of WHICH
values, write our own code) over copying third-party implementation: cleaner
licensing, stronger as original work. Note gzcom-dll is LGPL v2.1+ (dynamic
linking with SC4 permitted; modifications to gzcom-dll must be shared under LGPL);
several reference plugins are MIT. Maintain an ATTRIBUTIONS/LICENSES file listing
every framework/header used and its licence. Do not copy Super-Speed Mod code
without honouring its licence.

**D23. Simulation phase terminates on in-game date, not wall-clock.** Executor
runs each tile until game date advances by Y months, using the game's own date as
loop condition. Machine- and tick-rate-independent; correct in both speed modes.
Supersedes any real-time-timer approach (removes the old macro sleep-timing
weakness). Stopwatch real-time measurement therefore unnecessary (per this
session).

**D24. Commute/RCI needs YEARS of simulation; sets Y large.** Unlike utilities
(load-time), commute is a re-derived equilibrium requiring ~3-5 in-game years,
possibly never fully plateauing. Y is sized for this worst case. Consequences:
minimise tiles per pass, keep Z low, and treat fast-speed mode as important not
optional. Consider (later) separate fast utility-only vs slow full-resync modes.
