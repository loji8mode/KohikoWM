# Changelog

## Version 0.5.0

Release date: 2026-07-12

### Added
- EWMH/NetWM compliance: Kohiko now creates an invisible check window and publishes `_NET_SUPPORTING_WM_CHECK` and `_NET_SUPPORTED`, and keeps `_NET_CLIENT_LIST` (every managed window) and `_NET_ACTIVE_WINDOW` (the focused window) current for the session. This lets EWMH-aware tools that check for a compliant window manager before trusting window discovery (flameshot's screenshot overlay is the motivating example) work correctly under Kohiko.
- `Command::LauncherReload` and `Super+Shift+D` binding, plus `kohikoctl reloadlauncher`: rebuilds the launcher's cached application list and file index from disk immediately, without restarting Kohiko.

### Changed
- The launcher's application list and file index are now explicitly documented/exposed as an in-memory cache that does not automatically pick up newly installed applications or new/removed files until refreshed.

### Notes
- The application list and file index were already cached in memory as of 0.4.0; this release adds the explicit, on-demand refresh mechanism (`Super+Shift+D` / `kohikoctl reloadlauncher`) for that cache.
