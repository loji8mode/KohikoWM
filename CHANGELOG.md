# Changelog

## Version 0.8.1

Release date: 2026-07-14

### Fixed
- Fixed a bug where a tiled window that went fullscreen (either via `Super+F`, a client's own EWMH fullscreen request, or `windowrule=fullscreen`) and was then closed *while still fullscreen* left its BSP tree slot permanently reserved but empty, since the close path only checked `IsTiled()` — which is false for a fullscreen window — instead of also accounting for windows that logically still occupy a tree slot pending restoration. This produced a permanent dead/black area on screen that only a WM restart would clear. The same bug affected moving such a window to another workspace.
- Added `ManagedWindow::OccupiesTreeSlot()` (true if a window is tiled, or is fullscreen with a previous state of tiled) and updated window-close and move-to-workspace logic to use it instead of `IsTiled()` where appropriate. A window moved to another workspace while mid-fullscreen now reserves a fresh tiled slot on the destination workspace (falling back to floating if none fits) so it can still restore correctly when un-fullscreened.

### Notes
- A regression test covering this exact scenario (two ordinary tiled windows plus a third that tiles, goes fullscreen, and is closed without un-fullscreening first) was added to `tests/test_bsptree.cpp`.
