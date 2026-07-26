# Changelog

## Version 0.15.1

Release date: 2026-07-24

### Changed
- Launcher file-search performance: word-splitting for each `$HOME` file entry (used by the fuzzy match/ranking logic) is now precomputed once when the file index is scanned, instead of being recomputed on every keystroke while searching. Profiling identified this recomputation as the dominant per-query cost. `LauncherScoring::WordPrefixMatch()` and `BestFieldMatch()` gain overloads that accept a precomputed word list.

### Fixed
- `IconResolver` theme index parsing: `Directories=`/`Inherits=` lists in an icon theme's `index.theme` file are comma-separated per the freedesktop Icon Theme Specification, but were being read using `IniFile::GetList()`'s default separator (`;`, the desktop-entry list convention). This is corrected by passing the comma delimiter explicitly, so multi-directory and theme-inheritance icon themes are now parsed correctly.

### Notes
- `Launcher.h`'s internal `FileEntry` struct gains precomputed `nameLower`/`nameWords` fields to support the above.
