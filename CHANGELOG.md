# Changelog

## Version 0.13.1

Release date: 2026-07-19

### Added
- `windowrule=tile` now caps what a window's own declared `WM_NORMAL_HINTS` minimum size demands of the tiling capacity check, falling back to the plain `general.min_tile_width`/`general.min_tile_height` floor for that window instead. Previously, a window matching `windowrule=tile` (documented as "always tile it, strictly") could still be bounced to floating for lack of room if its own declared minimum size was larger than any available tile slot.
- `ManagedWindow::SetIgnoresOwnMinSizeForTiling()`/`IgnoresOwnMinSizeForTiling()`, set from `windowrule=tile`'s effect and consulted by `BSPTree::EffectiveMinSize()`.
- Default config adds `windowrule=tile class:telegram`, to keep Telegram's main window sharing the screen rather than opening floating on a small/cramped monitor.

### Notes
- This targeted fix is superseded one release later (0.13.2), which generalizes the underlying behavior to every window rather than only ones matching `windowrule=tile`.
