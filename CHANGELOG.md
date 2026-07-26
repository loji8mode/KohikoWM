# Changelog

## Version 0.11.0

Release date: 2026-07-18

### Added
- Real multi-monitor support: each XRandr-detected output now gets its own independent workspace and its own `BSPTree`, so switching workspace on one monitor never affects another, and fullscreen only covers the monitor it was toggled on. Falls back to treating the whole X display as one monitor when XRandr is unavailable.
- `monitor=<output>,workspace=<N>` repeatable config directive (`MonitorRule.h`/`.cpp`) pinning a specific XRandr output name to a starting workspace the next time it connects.
- `focusmonitor <left|right|up|down|N>` and `movetomonitor <left|right|up|down|N>` commands, bound by default to `Super+.`/`Super+,` and `Super+Shift+.`/`Super+Shift+,`. A moved window keeps its floating/tiled/fullscreen state; a floating window is re-homed into the destination's coordinate space at the same relative position.
- Automatic monitor hotplug handling: connecting or disconnecting a monitor re-detects the whole layout, assigns any newly-connected output its own workspace, and re-homes any floating window whose position no longer falls on any remaining monitor.
- Requesting a workspace that is already visible on a different monitor now swaps what the two monitors are showing, rather than refusing or duplicating the workspace.
- `kohikoctl monitors`: JSON dump of every detected monitor (id, XRandr output name, geometry, work area, active workspace, primary/focused flags). `kohikoctl dispatch focusmonitor <dir>` and `tree <id>` (for a specific workspace's tree) also added.
- `tests/test_monitormanager.cpp` and a corresponding `test-monitors` build target/CMake test, covering `MonitorManager`/`MonitorRule` (skips X-dependent checks gracefully when no `$DISPLAY` is set).
- Build files updated to require Imlib2 and GTK3 as proper pkg-config dependencies (previously implicit via the plain Makefile only).

### Changed
- `WorkspaceManager` no longer has a single notion of "the current workspace"; it owns every workspace (and its own `BSPTree`) for the process lifetime, with each monitor tracking its own active workspace.
- New windows now open on whichever monitor is currently focused, except a transient/dialog child, which follows its parent's monitor (in addition to its parent's workspace, per 0.10.0).

### Removed
- The "Multi-monitor tiling is basic... all tiling currently happens against the primary monitor" known limitation was removed, superseded by this release's actual multi-monitor support.
