# Changelog

## Version 0.14.1

Release date: 2026-07-21

### Added
- `XConnection::QueryPointer()`: a direct `XQueryPointer()` wrapper returning the pointer's current position in root coordinates.

### Fixed
- Fixed the focused monitor not being correctly detected right after startup, or right after a monitor topology change (hotplug), when the pointer was already sitting over a monitor that had nothing on it to generate a `MotionNotify`/`EnterNotify` event (most commonly, an empty monitor). Kohiko now directly queries the pointer position at the end of `Initialize()` and after handling a monitor topology change, instead of waiting for a pointer-motion event to establish which monitor is focused.
- Restored the `auto_start_programs=` config section, which had been inadvertently dropped from `config/default.conf` when `workspace<N>=` was introduced in 0.14.0.

### Changed
- Default config's example `workspace1=`/`workspace2=`/`workspace3=` lines are now commented out (they had been left active/uncommented in 0.14.0's default config).
