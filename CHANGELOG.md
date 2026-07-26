# Changelog

## Version 0.7.0

Release date: 2026-07-14

### Added
- Xft/fontconfig-based text rendering (`Font.h`/`Font.cpp`, `TextColor`): replaces the previous plain X11 core-font (`XCreateFontSet`) rendering used by the Bar, Launcher, and Notepad. `general.font=` now takes a fontconfig pattern (e.g. `monospace:pixelsize=14`) rather than an XLFD name.
- Per-character font fallback: any character not covered by `general.font=`'s named font is automatically looked up against every other installed font via fontconfig and drawn with whichever one has that glyph (the same technique used by dwm's Xft patch), so scripts such as Cyrillic or CJK render correctly instead of as missing-glyph boxes, without any per-language configuration.
- `Font.h` new header/class; used by `Bar`, `Launcher`, and `Notepad`.

### Changed
- Build files (`CMakeLists.txt`/`Makefile`) now require and link Xft and fontconfig (via pkg-config), in addition to the existing X11, GTK3, and Imlib2 dependencies.
- Default config: `general.focus_follows_mouse` default changed from `false` to `true`; `exec.terminal` changed from `xterm` to `kitty`; `exec.browser` changed from `firefox` to `zen-browser`; `keyboard.layouts` default changed to `us, ua`; `auto_start_programs` default updated to `Telegram discord zen-browser flameshot`.
- New config key `general.font=` (default `monospace:pixelsize=14`).

### Notes
- This release changes the "no toolkit, no Xft" framing that appeared in earlier README text — the project explicitly adopts Xft/fontconfig as its one rendering dependency beyond libX11, citing the lack of non-Latin glyph coverage in classic X11 core fonts as the reason.
