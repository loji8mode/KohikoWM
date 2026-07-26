# Changelog

## Version 0.15.0

Release date: 2026-07-23

### Added
- Launcher internals split out of the previously-monolithic `Launcher.cpp` into dedicated modules: `AppIndex` (builds/caches the scanned application list), `DesktopEntry` (parses `.desktop` files, including filtering of hidden/helper-style entries not meant to appear in a launcher), `IconResolver` (a from-scratch implementation of the freedesktop Icon Theme Specification lookup algorithm), `IniFile` (generic INI/desktop-entry-file parsing), `HistoryStore` (launch-history persistence), `LauncherScoring` (fuzzy match/ranking logic), and `Xdg` (XDG base-directory helpers).
- Web search fallback in the launcher: when no local application or file matches the typed query, Kohiko can offer to search the web, using a configurable engine (`google`, `duckduckgo`, `brave`, `wikipedia`, `archwiki`, `github`, `youtube`, or a `custom` URL template). Smart prefixes (e.g. `yt `, `wiki `, `arch `, `gh `) route directly to a specific engine regardless of the default.
- `launcher.show_hidden`, `launcher.strict_filtering`, `launcher.internet_search`, `launcher.search_when_no_results`, `launcher.default_search_engine`, `launcher.fallback_search_engine`, `launcher.custom_search_url` config keys.
- `tests/test_launcherscoring.cpp`: unit tests for the extracted `LauncherScoring` matching/ranking logic.

### Changed
- Icon-theme lookup no longer depends on GTK3 (`GtkIconTheme`); it is now implemented directly against the freedesktop Icon Theme Specification in `IconResolver`. GTK3 is dropped from the project's dependencies entirely (Imlib2 remains, for icon loading/rendering).
- Build files and `scripts/install-arch.sh` updated to drop the GTK3 dependency and package list entry.

### Notes
- This release substantially reduces Kohiko's dependency footprint by removing the GTK3 dependency introduced in 0.4.0, while keeping (and extending) the launcher's icon-rendering capability.
