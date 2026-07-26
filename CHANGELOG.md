# Changelog

## Version 0.9.0

Release date: 2026-07-17

### Added
- `BSPTree::Insert()` placement-aware overload: when inserting a new window, the tree now tries, in order: (1) the anchor's natural split direction, (2) the opposite split direction (a wide-but-shallow or tall-but-narrow anchor can fit one way and not the other), and (3) shrinking other tiles nearest the anchor first, walking outward — but never below `general.min_tile_width`/`general.min_tile_height`, or a window's own declared minimum if larger. `HasSpaceForAnotherWindow()` is updated to probe the same escalating logic without mutating the tree.
- `BSPTree::PlanReclaim()`: works out, via bisection over ancestor split ratios, whether shrinking existing tiles (without violating any of their own minimum sizes) can free enough room for a new window, without applying anything until a full plan succeeds.
- `Rect::ClampedTo()`: defensively clamps a rect to stay within given bounds, applied as a final safety check everywhere a computed rect is handed to X11.
- Tiling misbehavior detection and fallback: a window that repeatedly sends `ConfigureRequest`s asking for something other than the tile it was actually given (tracked via `ManagedWindow::RegisterTilingMisbehavior()`/`ResetTilingMisbehavior()`/`TilingMisbehaviorCount()`) is, after `general.tiling_misbehavior_threshold` conflicting requests in a row, pulled out of the tree and switched to floating or moved to the workspace with the fewest windows, per `general.tiling_misbehavior_fallback` (`floating` or `new_workspace`).
- New config keys: `general.tiling_misbehavior_threshold` (default 3), `general.tiling_misbehavior_fallback` (default `floating`).

### Changed
- `general.min_tile_width`/`general.min_tile_height` documentation updated to reflect that a new or existing tile now tries an alternate split direction and shrinking other tiles before falling back to another workspace or floating.
