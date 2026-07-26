# Changelog

## Version 0.3.0

Release date: 2026-07-11

### Added
- Native launcher (`Super+D`): a small centered input box that runs a typed command via `/bin/sh -c` on `Enter`, replacing the previous default of shelling out to `dmenu_run`. `Escape` or a click outside the box dismisses it without running anything. Implemented in new `Launcher.h`/`Launcher.cpp`.
- Native notepad (`Super+N`): a small scratch-notes box with free-form multi-line text (typing, `Enter`, `Backspace`/`Delete`, arrow keys, `Home`/`End`), auto-saved to and restored from `~/.config/kohiko/notepad.txt`. Implemented in new `Notepad.h`/`Notepad.cpp`. The bar shows a `[N]` indicator when the notepad has saved content or is open, matching the existing `[S]` scratchpad indicator style.
- `Animator`: a small rect-tweening stepper that animates a swapped window sliding into its new tile over a short duration, rather than snapping instantly. Used exclusively by the `Super+LMB` swap gesture.
- `BSPSplit::Subdivide()`: shared math for splitting a rect into two child rects, used by both `LayoutEngine` (real layout) and the new capacity check below.
- `BSPTree::HasSpaceForAnotherWindow()`: a non-mutating check for whether inserting a new window would shrink any resulting tile below `general.min_tile_width`/`general.min_tile_height`.
- New config keys: `general.min_tile_width`, `general.min_tile_height`, `notepad.width`, `notepad.height`.

### Changed
- The `Super+LMB` swap gesture no longer swaps live on hover: a dragged window now detaches and follows the cursor exactly, with the tiled window currently under the cursor highlighted as a swap preview; releasing over a window swaps the two (animated via `Animator`), and releasing over empty space (or the original window) slides the dragged window back to its original tile.
- `Super+RMB` resize now collapses a backlog of queued mouse-motion events down to the latest one per update, instead of processing every queued motion event, to avoid lag under a fast or laggy pointer.
- When there is no room to tile a new window without violating `general.min_tile_width`/`general.min_tile_height`, Kohiko now falls back to opening it on another workspace, or floating if no workspace has room, instead of forcing an undersized tile.
