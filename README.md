# SC4-Region-Update

A research and tooling project addressing a known limitation in SimCity 4 (2003):
within a region, a city tile's cross-border data (RCI demand, commuter flow,
utility import/export) only refreshes when that tile is manually loaded,
simulated, and saved. Neighbouring tiles drift out of sync, and a tile never
reopened is never updated at all.

This repository documents the investigation into whether that refresh can be
automated safely, and the tool built to do it.

**Status:** research and design phase complete; implementation not yet started.

## Why this project exists

This is a learning-and-portfolio project, not a commercial product. SimCity 4
simulates exactly one city at a time; every other tile in the region is inert
data on disk. The game reads a neighbour's state only at load time, so a change
in one city is not reflected in its neighbours until each neighbour is reopened,
given simulation time, and saved. The community treats this as a permanent engine
limitation and works around it by hand, tile by tile.

The value of an automated tool is precisely the tiles you would otherwise never
revisit: a region played without ever reopening a given tile drifts permanently
wrong, because that tile's saved file never updates for its neighbours to
reconcile against.

## Fast path for a newcomer

- `docs/project-summary.md` — scope, the approaches considered, what was ruled
  out and why, and the final design. Read this first.
- `docs/experiment-log.md` — the empirical tests run against a vanilla install,
  and what each one established. This is the evidence base.
- `docs/decision-log.md` — chronological record of every notable decision and
  its rationale, including approaches investigated and abandoned.

## The design in brief

A two-component design:

- **Planner** (read-only): derives the region's tile-adjacency graph from
  `config.bmp` and per-city save data, and produces an ordered list of tiles to
  process. Pure and inspectable; touches no save.
- **Executor** (in-game DLL plugin): consumes the list and, for each active tile,
  loads it, runs the simulation for a configured duration, saves, and moves on —
  locking out user input for the duration of a pass.

The tool runs each active tile for **Y** in-game months, repeated over **Z**
passes, in a user-chosen traversal order. It makes no attempt to compute an
"optimal" order or to detect convergence; it does a bounded, deterministic amount
of work and stops.

## Dependencies

Standalone; requires no other mods to function on a clean base-game install
(version 641). For much faster processing, install the **Disable FPS Limits** mod
(recommended, not required). Compatible with NAM, CAM, and other gameplay mods,
which change how cities simulate but not how this tool operates.

## Build

32-bit (x86) C++, built against the gzcom-dll framework with the SC4Fix loader.
See `docs/project-summary.md` for the toolchain and the first build milestone.

## Attribution

See `ATTRIBUTIONS.md`. This project builds on the work of the SC4 DLL modding
community, in particular the gzcom-dll framework (nsgomez) and the reference
plugins by 0xC0000054, Null 45, memo, and caspervg.
