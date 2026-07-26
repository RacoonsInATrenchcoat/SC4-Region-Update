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

### a3-time-sweep
Question: how many in-game months must a neighbour run before its figures
reflect a change next door? (The unknown "N".)
Method: repeat a2, letting K run 1/3/6/12/24 months before saving; restore from
backup between trials. Record where figures stop changing.
Result: [pending]

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
Question: does corner-only contact (A/F) transmit any neighbour data?
Result: [pending]

### a8-adjacent-unconnected
Question: does edge adjacency alone matter, or only actual connections?
Result: [pending]

### a9-date-drift
Question: does divergent in-game date between tiles cause problems?
Result: [pending]
