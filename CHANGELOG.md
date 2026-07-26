# Changelog

## Version 0.5.1

Release date: 2026-07-13

### Fixed
- `Super+RMB` resize on a tiled window: fixed a sign error where dragging the *second* child of a split resized it opposite to the direction it was dragged (grabbing the right/bottom window and dragging right/down could shrink it). The divider between the two children now consistently tracks the mouse direction regardless of which child was grabbed.
- Focus-stealing while the Launcher or Notepad is open: added a `FocusIn` event handler (`WindowManager::HandleFocusIn`) that immediately hands X input focus back to the open modal if any window (including one that calls `XSetInputFocus` on itself directly, as some Electron/GTK applications do) ends up focused while a modal should hold it. This is a second line of defense alongside not calling `Focus()` on newly-mapped windows while a modal is open.
- System tray icon background: tray icons previously sat on a hardcoded black background rectangle that didn't match the bar's configured (non-black) background color; the tray container and docked icons now use `ParentRelative` background painting, which tracks the bar's actual background color, including across a config reload.

### Notes
- The resize-direction fix is covered by an updated regression test in `tests/test_bsptree.cpp`.
