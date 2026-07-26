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
