# Changelog

## Version 0.15.2

Release date: 2026-07-24

### Fixed
- Bumped the on-disk application-index cache format version string (`KOHIKO-APPINDEX 2` -> `KOHIKO-APPINDEX 3`) so that a cache file written by a version prior to 0.15.1's schema changes is no longer loaded as if it matched the current field layout.

### Notes
- This is a narrow, single-line follow-up to 0.15.1's launcher-index changes.
