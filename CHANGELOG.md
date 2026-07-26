# Changelog

## Version 0.8.3

Release date: 2026-07-15

### Added
- `ManagedWindow::MinWidth()`/`MinHeight()` (from `WM_NORMAL_HINTS`'s `PMinSize`), read once when a window is managed via new `XConnection::GetMinSize()`.
- `XConnection::ClearArea()`: forces an `Expose` event over a window's whole area without changing its geometry.

### Changed
- Tiling capacity checks (`TryTile`, `FindWorkspaceWithRoom`) now take a window's own declared minimum size into account in addition to the configured `general.min_tile_width`/`general.min_tile_height`, using whichever is larger.
- When a client's `ConfigureRequest` asking for a different geometry than its actual tile is denied, Kohiko now forces an `Expose` (`XConnection::ClearArea`) if the request actually asked for something different from the window's current geometry, so a client that already repainted itself at its requested (denied) size gets prompted to redraw correctly. This fixes windows (again, most reliably TLauncher) visually "opening crooked" after re-asserting their own preferred size mid-session.

### Notes
- This release's approach to a window's own minimum size is superseded in 0.13.2, which stops treating a client's declared minimum as a tiling requirement at all.
