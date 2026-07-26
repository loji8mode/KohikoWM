# Changelog

## Version 0.6.0

Release date: 2026-07-13

### Added
- Autostart: `auto_start_programs=` config key takes a space-separated list of commands launched once, right after the bar/tray/launcher finish starting up. Implemented via `WindowManager::RunAutostart()`, called only at startup (not on `kohikoctl reload`, so reloading the config never relaunches autostart programs).
- Keyboard layout support: `keyboard.layouts=` (comma-separated XKB layout list) and `keyboard.layout_toggle=` (a `setxkbmap` `-option` value, e.g. `grp:alt_shift_toggle`) applied via `setxkbmap` at startup and again on config reload. Kohiko's own keybindings are grabbed by physical keycode and are unaffected by which layout is active.
- `Utils::SplitWhitespace()`: splits a string on runs of whitespace, dropping empty tokens; used to parse `auto_start_programs=` and the keyboard layout list.

### Notes
- Default config now ships `auto_start_programs=telegram-desktop discord zen-browser` and `keyboard.layouts=us,ua` as examples.
