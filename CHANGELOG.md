# Changelog

## Version 0.10.0

Release date: 2026-07-17

### Added
- `XConnection::IsFloatingWindowType()` (replacing `IsDialog()`): recognizes any of `_NET_WM_WINDOW_TYPE` DIALOG, UTILITY, SPLASH, TOOLBAR, POPUP_MENU, DROPDOWN_MENU, or MENU as a type that should always float rather than tile — previously only DIALOG was checked.
- Transient windows (any window with `WM_TRANSIENT_FOR` set) now always open on whichever workspace their parent window currently occupies (switching to it if needed) and are centered over the parent's own window rather than the middle of the screen, so dialogs/pickers/prompts always appear attached to the window that spawned them.
- `XAtoms`: interned atoms for `NET_WM_WINDOW_TYPE_NORMAL`, `_UTILITY`, `_SPLASH`, `_TOOLBAR`, `_POPUP_MENU`, `_DROPDOWN_MENU`, `_MENU`.

### Changed
- `WindowManager::CenteredFloatingRectForWindow()` now accepts an optional parent `ManagedWindow*` and centers the result over the parent's geometry when given, clamped to stay within the monitor.

### Removed
- The "No system tray in the bar" known-limitation note was removed from the README (the system tray was already implemented in 0.4.0).
