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

A=large(0-3,0-3)  B=med(4-5,0-3)  D=med(0-1,4-5)  F=large(4-7,4-7)
G=med(0-1,8-9)  H=med(13-14,8-9)  J=large(7-10,11-14)  K=large(11-14,11-14)
W1=gap(2,6)  W2=gap(14,10)

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

## Phase A: mechanism tests

### a2-zero-time-propagation
Question: does a pure load-and-save (no time elapsed) move any neighbour data?
Method: establish J and K with a cross-border connection; change J; save/exit;
load K, immediately save/exit without unpausing; reload K and compare.
Result: [pending]

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
