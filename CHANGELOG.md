# Changelog

## Version 0.12.0

Release date: 2026-07-19

### Added
- One `Bar` instance per monitor, each showing that monitor's own active workspace and highlight; only the currently-focused monitor's bar shows a window title, while the clock and scratchpad/notepad indicators appear on every bar. The system tray, being a single X11-wide selection, stays on whichever monitor is primary.
- Ambient focus-follows-mouse across monitors: whichever monitor the cursor is currently over becomes "the focused monitor" (where new windows open, which monitor workspace commands act on), independent of `general.focus_follows_mouse` (which only governs whether hovering a specific window steals its focus). No keybinding is required; `MouseManager` now also routes ambient pointer motion (when no drag is active) to this monitor-focus tracking.
- Dragging a floating window (`Super+LMB`) across a monitor boundary now transfers it live to the destination monitor's workspace mid-drag; releasing clamps its final position to whichever monitor it ends up over.
- Requesting a workspace already visible on a different monitor now does nothing to either monitor except show a transient notification on the requesting monitor's bar explaining the conflict, replacing the previous behavior (introduced in 0.11.0) of swapping the two monitors' workspaces.

### Changed
- `Super+LMB` drag is now a single shared gesture: it swaps tiled windows (as before) or moves a floating window 1:1 with the cursor, depending on which kind of window is under the cursor when the drag starts, rather than only ever meaning tiled-swap.
- Monitor hotplug handling now also rebuilds each monitor's own bar to match the new topology.

### Removed
- The "Focus doesn't follow the mouse onto empty space on another monitor" known limitation was removed, superseded by this release's ambient cross-monitor focus-follow.

### Notes
- The Launcher and Notepad remain single, global panels pinned to the primary monitor in this release (addressed later, in 0.16.0).
