# Changelog

## Version 0.13.0

Release date: 2026-07-19

### Added
- `Super+RMB` drag-resize now works on floating windows, not just tiled ones: whichever edge(s) of the floating window are nearest to where it was grabbed grow or shrink live with the cursor, while the opposite edge(s) stay anchored — grabbing near a corner resizes both axes at once. The result is clamped to the window's own minimum size and to whichever monitor it is currently on.
- `WindowManager::BeginFloatingResize()`/`UpdateFloatingResize()`/`EndFloatingResize()` and a new `MouseManager::DragMode::FloatingResize` mode implementing the above.

### Notes
- Previously, `Super+RMB` on a floating window had no effect (resize only applied to tiled windows via BSP divider adjustment).
