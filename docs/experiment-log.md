# Experiment Log

Empirical tests run against a vanilla, unmodded SimCity 4 (Steam, v641).
Purpose: verify the assumptions behind the region-refresh design before
committing to an implementation route.

## Test region

Canonical 15x15 region, built in config.bmp, verified in-game. Renders as
2+2 large, 3 medium, 2 empty cells, remainder small. ASCII map and tile
labels: see below.

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

A=large(0-3,0-3)   B=med(4-5,0-1)   C=med(4-5,2-3)   D=med(0-1,4-5)
F=large(4-7,4-7)   G=med(0-1,8-9)   H=med(13-14,8-9)
J=large(7-10,11-14)   K=large(11-14,11-14)
W1=gap(2,6)   W2=gap(14,10)
S=small (remainder)

Primary propagation pair: J and K (large-large, full shared edge).
Mixed-size hub: A (medium and small neighbours).
Corner-only control: A and F.

## Finding 0: config.bmp tile placement is not lattice-aligned

Assumption going in: large tiles must originate on a 4-cell sub-grid, medium
on a 2-cell sub-grid. **Disproven in-game.** A large tile starting at column 7
renders correctly. The actual rule: each tile only needs a self-consistent
block of its colour (4x4 blue, 2x2 green, 1x1 red); blocks may sit at any
offset provided they do not conflict. Recorded because it corrected a wrong
design assumption early.

**User Note**: this was the AI's assumption and required multiple pushbacks to accept on how it works and loads, where this limitations does not exist.

## Phase A: mechanism tests

### a2-zero-time-propagation
Question: does a pure load-and-save (no time elapsed) move any neighbour data?
Signal: power import (utility), chosen for legibility and low sim lag.

Fixture: J self-sufficient with power surplus, selling ~3k to K (K uses ~2.1k,
no local generation). Power-only cross-border link; no road/water/other.

Method: baseline K; in J, delete the border power connection and cancel the
resulting neighbour deal; save J; load K paused with zero ticks; observe, then
save/exit; reload K.

Result: **Positive. Neighbour utility data updates at load time, zero game time
elapsed.** On loading K (date unchanged, 10/2/06, no tick), power import had
already dropped to 0, the power-supply graph had collapsed, the budget line for
the power payment had updated, and the power advisor was already flagging the
shortfall. A same-date reload showed identical metrics, confirming persistence.

Notes:
- The neighbour DEAL did not dissolve when the physical connection was deleted
  on J's side; it required manual cancellation. Connection geometry and deal
  contract are separate state. Flagged for a5-deal and for tool design (risk of
  orphaned deals after an automated border change).
- Advisor/icon cues lag the metrics: electricity icons appeared on 10/8/06 with
  no underlying metric change. **Measurement must read metrics, not visual cues.**
- Tested as supply-removed (3k -> 0). A supply-changed case (e.g. 3k -> 1k) not
  yet tested; would confirm load-time ingestion reads the actual value, not just
  deal presence.

Design impact: for utilities, a load/save touch suffices; no simulation phase
required to ingest neighbour utility state. Does not yet generalise to
commute/RCI (see a3, a5).

### a2b-import-export-asymmetry (follow-up to a2)
Question: is neighbour data updated symmetrically on both sides?

Method: restored J->K 3k power export, both saved healthy at differing dates.
In J, deleted the border connection (paused, then with months passing); neighbour
deal persisted despite no connection. Saved J. Loaded K: on load, zero ticks,
K showed power missing, import 0, deal 0, power icons present. Saved/exited K
with no time passed. Reloaded J: J still showed the deal existing and exporting,
after both cities saved with no connection.

Result: **Asymmetric.** The CONSUMING side (K) recomputes its intake from the
neighbour's saved file on load and self-corrects to reality with zero ticks. The
PRODUCING side (J) carries its saved export/deal record forward and does NOT
re-validate it against the consumer on load; it remained stale.

Interpretation: each city, on load, recalculates what flows IN across its borders
by reading neighbours' files, but trusts its own saved record of what it sends
OUT. Import figures self-heal; export/deal records do not, absent manual action.

Open questions (untested):
- Does the producer self-correct if allowed to run N months (vs zero ticks)?
- Is the persistent deal a by-design contract object (dissolves only on manual
  cancel or expiry), independent of physical flow?

Design impact: refreshing only a changed tile's NEIGHBOURS is likely
insufficient; the changed (producing) tile may retain stale outgoing deals.
Refresh design must account for the producer side, order, or deal handling.
Elevates a4 (does the source tile need processing) from optional to important.

### a2c-producer-reconciliation (follow-up to a2b)
Question: under what condition does the producing tile (J) correct its stale
outgoing export/deal?

Method: after K had been saved reflecting the severed connection (0 import),
loaded J and let it reach a month-end with no manual action.

Result: **J self-corrected at the first month-end**, nullifying the phantom
export, no user action. Contrast with a2b, where J was loaded BEFORE K had been
saved with the connection gone, and did NOT self-correct.

Interpretation: the producer reconciles its outflows at month-end by reading the
NEIGHBOUR'S SAVED FILE, not physical reality. Correction therefore depends on the
consumer having been saved-with-truth first. Inferred: had K never been loaded
and saved post-change, J would export to a phantom indefinitely.

Refined model:
- Consumer recomputes inflows ON LOAD (instant, zero ticks).
- Producer reconciles outflows AT MONTH-END, against the neighbour's saved file.
Different triggers, different latencies; both read neighbour files.

Open question: is the trigger specifically the month-end rollover, or just the
next periodic checkpoint after some ticks? Affects how long a producer tile must
run. (Loaded position within the month not controlled in this test.)

Design impact (significant):
- Refresh is ORDER-DEPENDENT: the changed/consumer side must be saved before the
  producer is reconciled. Processing a producer against a stale neighbour file
  leaves it wrong.
- A single pass may not converge for multi-hop ripples; supports topological
  ordering or repeat-until-stable.
- Producer tiles are NOT zero-tick: they must run to at least the next month-end
  to reconcile outgoing state. First hard evidence that some tiles need sim time.
  Cost is small (<= ~1 in-game month).

  ### Finding: cross-border reconciliation model (consolidated from a2/a2b/a2c)

- Consumer recomputes inflows ON LOAD (instant, zero ticks).
- Producer reconciles outflows AT EACH MONTH-END checkpoint, against the
  neighbour's SAVED file (worst case ~31 in-game days to next checkpoint).
- Correction depends on the neighbour having been saved-with-truth first =>
  refresh is order-dependent.

Hypothesis (inferred, not isolated): volatile intra-city quantities update on
frequent internal ticks; cross-border / lower-volatility quantities reconcile at
month-end. Predicts which subsystems need runtime vs a load touch.

Thesis support: a region played without ever reopening a given tile drifts wrong,
because the un-opened tile's saved file never updates for others to reconcile
against. The tool's value is exhaustive, ordered reopen+save of exactly those
tiles.

### a2-commute / a3-time-sweep (combined)
Question: does commute/RCI update at load-time (like utilities) or require
simulation months? What is Y?

Fixture: J worker-heavy (residential > local jobs, jobless icons), K job-heavy
(Ind/Com > local residential). Highway + rail (passenger + cargo) across border,
power still sold. Both established/stable/saved.

Metric note/confound: no clean "commuters in" figure exists. Used Jobs&Pop graphs
(Ind-Dirty, Ind-Mfg, Ind-HT, Ind-Ag, Com) as proxies. The graphs do NOT
distinguish internal vs commuted-in workers, and a single facility may count
toward both tiles. Absolute values therefore partly contaminated; conclusions
drawn from direction/magnitude of change and its timing, not exact accounting.

Baseline sensitivity check (disconnect test):
- Disconnecting J dropped J's Ind-M 5.5k->3.9k, Ind-D 16k->13k, Com$$ 3.6k->3.2k.
  Reconnected/unsaved, J recovered to ~16k I-D, ~5.5k I-M over a year.
- Disconnecting K (~6 mo) dropped K's I-HT 800->250, I-Ag 2900->2600; I-M and I-D
  ~unchanged. Asymmetry consistent with J being the worker source.

Test:
2. Change J: added large residential (77k->88k pop) + bus. Stabilised 5 yrs.
   J I-D ~16k, I-M ~5.5k stable. Saved 10/3/48.
3. Load K (7/9/26), PAUSED: I-HT 800, I-M 5k, I-D 0, I-Ag 2900. **No change on
   load/pause** (contrast utilities, which changed instantly on load).
4. Run K Cheetah to 1/3/30 (~4 yrs): I-HT 800->2100, I-M 5k->5100, I-Ag
   2900->3200, commercial sharp rises (Cs$$ 3.2k->5.8k). I-D stayed 0.

Result: **Commute/RCI is a SIMULATED equilibrium, NOT a load-time read.** Changes
appear only through ticks, spread over YEARS, in steps, with some metrics not yet
plateaued at 4 yrs. I-HT notably FELL for several years before rising sharply
(inferred: an internal gate, e.g. education/staffing, indirectly delays the
commute response).

Y (commute) ~= 3-5 in-game YEARS to capture the bulk of re-allocation; full
plateau may not exist where settling depends on user zoning decisions the tool
can't make.

Design impact:
- Y must be sized for the SLOWEST subsystem (commute), so Y is large (years),
  vs utilities' month-end. A single per-tile Y is wasteful for utility-only
  relationships / insufficient for commute-heavy ones.
- Confirms rejection of "repeat-until-stable" (D16): I-HT's transient decline
  before rising would fool a naive stability check into stopping early.
- Large Y raises per-pass cost sharply -> favours minimising tiles touched
  (edge-adjacency v2) and low Z; weakens "crude whole-region multi-pass".
- Elevates fast-speed mode (D21) from optional toward necessary: 4 yrs at vanilla
  Cheetah ~= 144 s/tile before I/O; unmanageable region-wide without tick unlock.

Design idea (deferred): split resync into a fast utility-only sweep (zero-tick
load/save) vs a slow full resync (Y years). Maps to dual-mode thinking.

### a4-source-side-requirement
Question: does the changed tile also need simulation time, or only the receiver?
Result: [pending]

### a5-what-propagates
Question: which subsystems go stale (power, water, commute, network, deals)?
Result: [pending]

### a6-two-hop-propagation
Question: does a change propagate A -> D -> G across two hops, and do repeated
passes converge?
Result: [pending]

### a7-corner-contact
Question: does corner-only (diagonal) contact transmit anything?
Method: A with power plant + edge connections; F built residential needing power,
corner-only contact with A. Ran a few months to confirm.
Result: **Nothing. The game offered no power-import option to F at all** - no
connectable border exists across a corner. Corner/diagonal adjacency is
functionally not adjacency. Confirms Planner rule: corner-only pairs are NOT
neighbours.

### a8-adjacent-but-unconnected
Question: does a shared edge with NO connection transmit anything?
Method: D edge-adjacent to A but with no road/rail/power/water across the edge.
D needed power; no import option appeared. Cross-check: changed J, saw J's RCI
move, returned to D - D unchanged. Ran a few months to confirm.
Result: **Nothing crosses an unconnected edge.** No utility import possible; a
neighbour's RCI change does not reach an unconnected tile. Edge-adjacency alone
is inert without a connection.

Regional-RCI note (out of scope): tiles carry a baseline regional demand that
exists even in isolation (observed on D). There are also region-wide effects
(airports; certain hardcoded caps, possibly tile-scoped). These are ambient/
hardcoded, present regardless of connection, and are NOT neighbour-to-neighbour
flows that go stale - so not something a refresh changes. Noted, not targeted.

Design impact: confirms "connection present" as the exact discriminator between
propagating and inert neighbours. Validates v3 (connection-aware refresh) as a
REAL saving, not a marginal one, because a8 proves unconnected neighbours are
pure wasted work.
### a9-date-drift
Question: does divergent in-game date between tiles cause problems?
Result: [pending]

### d3-render-cost / sim-speed-limiter (vanilla)
Question: does reducing render load speed up the simulation? Is Cheetah
render-bound or tick-locked?

Method: 62k-pop city, max zoom out, paused baseline. Measured FPS per speed and
approx month duration at Cheetah, high detail vs all-low detail (restarted to
apply).

Data:
- FPS is fixed per SIM speed, not per load: Normal ~30, Rhino ~20, Cheetah ~15,
  stable regardless of city size or detail. Uncapped moments (menus/idle) spike
  to 400-1000+ fps, proving the renderer has large spare capacity.
- Cheetah ~= 3 s/month at high detail; ~2.7 s/month (marginal, within eyeball
  noise) at low detail. FPS averages unchanged by detail.

Result: **Simulation is TICK-LOCKED, not render-bound.** The renderer runs free
(hundreds of fps available) while sim speed is paced to a fixed internal tick
rate per speed setting. Low detail / small window / zoom-out do NOT meaningfully
speed the sim because the sim never waits on rendering.

Correction: earlier assumption (render suppression as the primary speed lever)
is DISPROVEN for simulation speed. Render suppression may still help if a future
faster tick rate becomes render-bound, but at vanilla speeds it does not.

Implication: going faster requires changing the tick-rate data (per Super-Speed
Mod, values in SimCity_1.dat; 2x/4x/8x Cheetah variants exist), not relieving
hardware load. Expect a stability cost at higher tick rates (mod author warns to
reduce speed before saving; consistent with our existing save-at-normal-speed
requirement from a2c).

Side observation: a ~30fps floor engages during active simulation (likely a
frame cap / vsync), disengaging during idle/menu (the spikes). Separate from
tick rate; noted, not tested.

Open: confirm detail-independence with stopwatch-timed 12-month runs (high vs
low) to convert "feels same" into a measured yes/no.
