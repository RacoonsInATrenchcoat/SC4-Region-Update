# Experiment Log

Empirical tests run against a vanilla, unmodded SimCity 4 (Steam, v641), to
verify the assumptions behind the design before committing to implementation.
Results are the evidence base for the decisions in `decision-log.md`.

## Test region

A purpose-built 15x15 region (`config.bmp`), verified in-game.

    LLLLMMSSSSSSSSS
    LLLLMMSSSSSSSSS
    LLLLMMSSSSSSSSS
    LLLLMMSSSSSSSSS
    MMSSLLLLSSSSSSS
    MMSSLLLLSSSSSSS
    SS_SLLLLSSSSSSS
    SSSSLLLLSSSSSSS
    MMSSSSSSSSSSSMM
    MMSSSSSSSSSSSMM
    SSSSSSSSSSSSSS_
    SSSSSSSLLLLLLLL
    SSSSSSSLLLLLLLL
    SSSSSSSLLLLLLLL
    SSSSSSSLLLLLLLL

L=large (4x4), M=medium (2x2), S=small (1x1), _=empty cell.
Key tiles: A=large(0-3,0-3); B=med(4-5,0-1); C=med(4-5,2-3); D=med(0-1,4-5);
F=large(4-7,4-7); J/K=adjacent large pair (bottom); plus deliberate empty cells.
A has neighbours of every size; A and F meet at a corner only; J and K share a
full edge.

## Finding 0: config.bmp tile placement is not lattice-aligned

Assumption: large tiles must originate on a 4-cell sub-grid, medium on a 2-cell
sub-grid. **Disproven in-game.** A large tile starting at an odd column renders
correctly. The real rule: each tile needs only a self-consistent block of its
colour (4x4 blue, 2x2 green, 1x1 red); blocks may sit at any offset provided they
do not conflict. (Corrected a wrong early assumption; empirical trumps theory.)

## a2 — zero-time propagation (utilities)

Question: does a pure load-and-save (no time elapsed) move neighbour data?
Signal: power import, chosen for legibility and low simulation lag.

Fixture: J self-sufficient with a power surplus, selling ~3k to K (K uses ~2.1k,
no local generation); power-only cross-border link.
Method: baseline K; in J, delete the border connection and cancel the resulting
deal; save J; load K paused with zero ticks; observe; save/exit; reload.

Result: **Positive.** On loading K (date unchanged, no tick elapsed), power import
had already dropped to 0, the supply graph had collapsed, the budget line had
updated, and the advisor already flagged the shortfall. A same-date reload was
identical.

Notes:
- The neighbour **deal did not dissolve** when the physical connection was deleted;
  it required manual cancellation. Connection geometry and deal contract are
  separate state.
- Advisor/icon cues **lag** the metrics (electricity icons appeared days later with
  no metric change). Measurement must read metrics, not visual cues.

## a2b — import/export asymmetry

Question: is neighbour data updated symmetrically on both sides?
Method: restored J→K power; deleted the connection in J (deal persisted); saved J;
loaded K (self-corrected to 0 on load, zero ticks); saved K; reloaded J.

Result: **Asymmetric.** The consuming side (K) recomputes intake on load and
self-corrects with zero ticks. The producing side (J) carried its stale export/
deal forward and did not re-validate against the consumer on load. Model: each
city recomputes what flows IN on load, but trusts its own saved record of what it
sends OUT.

## a2c — producer reconciliation

Question: under what condition does the producer (J) correct its stale export?
Method: after K had been saved reflecting the severed connection, loaded J and let
it reach a month-end with no manual action.

Result: **J self-corrected at the first month-end**, nullifying the phantom
export. Contrast a2b, where J was loaded *before* K had been saved with the change
and did not self-correct. Interpretation: the producer reconciles outflows at
month-end **against the neighbour's saved file**. Correction depends on the
consumer having been saved-with-truth first. Inferred: had K never been reopened
and saved, J would export to a phantom indefinitely — this is the core case the
tool exists to fix.

## a2-commute / a3 — commute time-sweep

Question: does commute/RCI update at load-time like utilities, or need simulation
months? What is Y?

Fixture: J worker-heavy (residential > local jobs); K job-heavy (Ind/Com > local
residential); highway + rail across the border.
Measurement caveat: no clean "commuters in" figure exists; used Jobs & Pop graphs
(Ind-Dirty/Mfg/HT/Ag, Com) as proxies. These do not distinguish internal from
commuted-in workers and a facility may count toward both tiles, so absolute values
are partly contaminated; conclusions drawn from direction, magnitude, and timing.

Method: changed J's worker supply (added a large residential district, 77k→88k);
saved. Loaded K (paused): **no change on load**. Ran K to +~4 years.

Result: **Commute/RCI is a SIMULATED equilibrium, not a load-time read.** Changes
appeared only through ticks, over years, in steps (e.g. Ind-HT 800→2100, Com$$
3.2k→5.8k), some not plateaued at 4 years. Ind-HT notably *fell* for several years
before rising — an internal gate (inferred: education/staffing) delays the commute
response.

Y (commute) ≈ 3–5 in-game years for the bulk of re-allocation; full plateau may
not exist where settling depends on user zoning decisions the tool can't make.
The transient wrong-direction move is direct evidence that convergence detection
would stop too early — it must be rejected.

## a7 — corner contact

Method: A with a power plant; F built to need power, corner-only contact with A.
Result: **Nothing.** The game offered no power-import option to F — no connectable
border exists across a corner. Corner/diagonal contact is not adjacency.

## a8 — adjacent but unconnected

Method: D edge-adjacent to A with no connection across the edge. D needed power; no
import option appeared. Cross-check: changed J, saw J's RCI move, returned to D —
unchanged.
Result: **Nothing crosses an unconnected edge.** Edge-adjacency alone is inert.
Note (out of scope): tiles carry a baseline regional demand present even in
isolation; there are also region-wide effects (airports, some hardcoded caps).
These are ambient, not neighbour-to-neighbour flows, so not something a refresh
changes.

## a6 — cascade contention (shared source)

Question: how does a shared source resolve competing demand across order?
Fixture: C exports water to both A and F (capacity 40k, base use <1k).
Method: C selling ~2k (to A), not saved; built F to also draw ~2k; saved F;
reopened C.
Result: C on reload showed the **combined** export — a producer aggregates
multiple consumers' demand from their saved files. Consequence: if supply were
insufficient, neither consumer learns of the shortfall until C is saved with it,
and the reduced supply is then allocated to consumers **in load order** — first
loaded takes its share, the next gets the remainder. No global arbiter; contention
is resolved first-come by load order. Therefore traversal order affects outcomes,
not just speed.

## Speed — sim-limiter characterisation

Method: measured FPS per speed and month duration at Cheetah, high vs low detail.
Data: FPS is fixed per sim speed (Normal ~30, Rhino ~20, Cheetah ~15), stable
regardless of city size or detail; idle/menu moments spike to 400–1000+ FPS.
Cheetah ≈ 3 s/month; low detail gave no meaningful change.
Result: **Simulation is tick-locked, not render-bound.** Render suppression does
not speed the sim while the FPS cap is in place. (Earlier assumption that render
suppression was the primary lever: disproven here.)
Corroboration: the Disable FPS Limits mod confirms the 30/20/15 caps and lifts
them (up to 255), speeding the simulation directly. Once lifted, rendering becomes
the bottleneck, so render suppression then helps. The two levers stack, in that
order.
