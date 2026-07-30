# SC4-Region-Update

A research and tooling project addressing a known limitation in SimCity 4:
within a region, a city tile's cross-border data (RCI demand, commute flow,
utility import/export) only refreshes when that tile is manually loaded,
simulated, and saved. Neighbouring tiles therefore drift out of sync.

This repository documents the investigation into whether that refresh can be
automated, and the tooling built to do so.

**Status:** research and design phase. No release yet.

## Requirements

This mod is standalone and requires no other mods to function. It works on a clean base-game install (version 641). It is compatible with NAM, CAM, and other gameplay mods, which change how cities simulate but do not affect how this tool operates. For faster processing, the mod includes an optional built-in speed boost (fast mode); users who prefer a separate solution can also use the standalone Disable FPS Limits mod, with which this tool composes naturally.

## Why this project exists

This is a learning-and-portfolio project, not a commercial product. SimCity 4
(2003) simulates exactly one city at a time; every other tile in the region is
inert data on disk. The game reads neighbour state only at load time, so a
change in one city is not reflected in its neighbours until each neighbour is
reopened. The community treats this as a permanent engine limitation and works
around it by hand.

The goal here is to establish, from first principles and by measurement,
whether a reliable automated workaround is possible, and to build one if so.

## Fast path for a newcomer

- `docs/project-summary.md`: the scope, the approaches considered, what was
  ruled out and why, and the chosen direction. Read this first.
- `docs/decision-log.md`: chronological record of decisions and their rationale,
  including approaches that were investigated and abandoned.
- `docs/experiment-log.md`: the empirical tests run against a vanilla install,
  and what they showed.

## Current direction (subject to test results)

A two-component design:

- **Planner** (read-only): derives the region's tile-adjacency graph from
  `config.bmp` and per-city save data, and emits an ordered refresh queue.
- **Executor** (in-game DLL plugin): consumes the queue, loading, simulating,
  and saving each tile, with player input locked out during a pass.

The single load-bearing unknown is whether a plugin can command the game to load
a named city tile programmatically. Confirming that decides whether the Executor
can be a pure in-game mod or must fall back to an external input macro for that
one step.
