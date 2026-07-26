# Changelog

## Version 0.13.2

Release date: 2026-07-20

### Changed
- A window's own declared `WM_NORMAL_HINTS` minimum size is no longer treated as a requirement for whether it can be tiled at all — only the configured `general.min_tile_width`/`general.min_tile_height` floor gates tiling placement now, unconditionally, for every window. Kohiko already force-resizes a tiled window to its assigned slot regardless of what the window itself asked for, so a client's declared preferred minimum was never a hard technical requirement, just an additional demand the capacity check made before agreeing to tile it. This previously caused real applications with a fairly large declared minimum (Discord, Telegram) to fall back to floating on a small or cramped monitor even when a smaller-than-preferred tile slot was actually available. A tiled window can now only become floating again through an explicit user action (`Super+Space`) or the separate tiling-misbehavior fallback for a window that keeps actively fighting its assigned tile.

### Removed
- `ManagedWindow::SetIgnoresOwnMinSizeForTiling()`/`IgnoresOwnMinSizeForTiling()` and the `windowrule=tile class:telegram` default-config workaround introduced in 0.13.1, both superseded by the more general fix above.

### Notes
- `tests/test_bsptree.cpp` updated to test capacity/placement purely against the configured floor, no longer against a window's own `SetMinSize()`.
