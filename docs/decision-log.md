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
