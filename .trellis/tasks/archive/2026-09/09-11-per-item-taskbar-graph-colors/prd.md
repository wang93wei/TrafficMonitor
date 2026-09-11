# Add per-item taskbar graph colors

## Goal

Port the per-item taskbar graph color feature from the upstream fork into this
fork, so the taskbar bar/plot graph color can be configured per display item
instead of only globally.

## Background

The taskbar window already supports a global usage-graph color (either a fixed
color or the system theme accent) plus an optional per-item text color. Graph
colors remained global, so every item's bar/plot was drawn in one color. This
task brings the per-item capability to graphs and keeps the fork's existing
changes intact.

Source of the feature: upstream original-project commits `6b46016` ("Add
per-item taskbar graph colors") and `061b67b` ("Fix per-item graph color
checkbox layout").

## Requirements

### R1 — Per-item graph color setting

- Add a taskbar setting that enables specifying a graph color per display item,
  independent of the existing per-item text color setting.
- When disabled, every item keeps using the global graph color.

### R2 — Per-item color editing UI

- The per-item color dialog gains a way to edit the graph color for each item
  alongside the existing text color.
- The edit entry point is available only while per-item graph colors are
  enabled and graphs are actually shown.
- Built-in items and plugin items are both listed.

### R3 — Rendering

- Both graph display modes — the bar/status-bar mode and the plot/history mode —
  use the item's configured graph color.
- The dashed-box outline, when enabled, uses the same per-item color.
- Items without a configured color fall back to the global graph color.

### R4 — Persistence

- The enable flag and per-item graph colors persist in the configuration and
  restore on restart.
- The taskbar style presets carry graph colors and the enable flag, so applying
  a preset restores them.

### R5 — Localization

- New user-visible strings go through the existing string table with entries in
  the shipped language INIs; no hardcoded literals.

### R6 — Fork compatibility (constraint)

- The fork's fork-specific work must be preserved: GPU VRAM items, Win11
  multi-monitor taskbar behavior, DPI/display-change handling, and the PDH/DXGI
  hardware query paths.
- No second settings, drawing, plugin, or display-item abstraction may be
  introduced (see `.trellis/spec/cpp-mfc/`).

## Non-goals

- Adding graph rendering to items that do not already draw graphs (including the
  fork's built-in VRAM item).
- Changing the global graph color behavior or the system-theme-follow option.
- Adding new display items or plugin API surface.

## Acceptance Criteria

- [x] Per-item graph colors can be enabled/disabled independently of per-item
      text colors.
- [x] Each built-in and plugin display item can hold its own graph color.
- [x] Bar mode and plot mode both honor the per-item color; the dashed outline
      matches.
- [x] Unset items fall back to the global graph color.
- [x] Enable flag and per-item colors survive a save/reload cycle and are
      carried by taskbar style presets.
- [x] New strings resolve from the language INIs in English and Simplified
      Chinese.
- [x] The fork's existing items and behavior are unaffected.
- [x] Full (`TrafficMonitor.sln`) and Lite Release builds succeed for x64 and
      x86.
- [x] Manual verification on Windows: settings UI, color dialog, and taskbar
      rendering behave as expected.

## Known limitation (carried over from the source feature)

Persistence only upserts graph-color INI keys and never removes stale ones. After
saving custom colors, applying an untouched preset, saving and restarting,
re-enabling per-item graph colors can restore older overrides. This was accepted
as faithful-port behavior rather than fixed here, to keep the port reviewable
against the source commits.
