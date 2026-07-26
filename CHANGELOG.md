# Changelog

## Version 0.8.0

Release date: 2026-07-14

### Added
- Window rules (`windowrule=` config directive, new `WindowRule.h`/`.cpp`): lets specific applications' tiling behavior be overridden by `class:`/`instance:`/`title:` selector, with actions `float`, `tile`, `fullscreen`, `nofullscreen`, and `workspace:N`. `windowrule=` is now a third repeatable config key alongside `bind=` and `exec.<name>=`.
- EWMH `_NET_WM_STATE_FULLSCREEN` support: Kohiko now honours a client's request for real fullscreen, whether made via a `_NET_WM_STATE` `ClientMessage` after mapping or by setting the property before being mapped, and keeps the property in sync with actual state either way. `windowrule=fullscreen`/`nofullscreen` build on this same mechanism.
- Floating windows (automatic or via `windowrule=float`) now open centered at their own natural size (from `WM_NORMAL_HINTS`, or their size at map time) instead of a flat fraction of the screen.
- The Launcher now stays raised above all other windows for as long as it is open, including over a window that opens while it is up, since it depends on holding real X input focus rather than an active keyboard grab.

### Notes
- Default config includes example `windowrule=` entries: `windowrule=fullscreen class:flameshot` and `windowrule=tile class:tlauncher`.
