# Changelog

## Version 0.4.0

Release date: 2026-07-12

### Added
- Launcher (`Super+D`) rewritten into a full application launcher: scans `.desktop` entries and builds a file index of `$HOME`, renders icons via Imlib2 (with GTK used for icon-theme lookup), fuzzy-matches typed text against application names with a subsequence matcher, and ranks results using a new hardcoded application popularity/penalty table (`AppRatings.h`). Adds launch history tracking (persisted to `/tmp/kohiko_launcher_history`) so frequently-used entries rank higher. Text entry now goes through X Input Method (XIM/XIC) with a multi-byte font set, replacing plain ASCII key handling, and gains a results list navigable by keyboard or mouse, with icons and scrolling.
- System tray support in the bar, implementing the freedesktop System Tray Protocol (`SystemTray.h`/`.cpp`): Kohiko takes ownership of the `_NET_SYSTEM_TRAY_S<screen>` selection and docks icon-bearing applets (NetworkManager, Bluetooth, volume, etc.) left of the clock.
- `scripts/install-arch.sh`: one-command Arch Linux install script — installs pacman dependencies, builds, installs, and registers Kohiko as a session (`.desktop` entry and `~/.xinitrc`, without overwriting an existing setup).
- `Print` key binding to run `exec.screenshot` (defaults to `flameshot gui`).
- `Utils::Utf8PrevBoundary`, `Utf8NextBoundary`, `Utf8ClampToBoundary`: UTF-8-aware cursor movement helpers, supporting proper multi-byte text editing in the new IME-based launcher input.

### Changed
- `LICENSE` updated to the full standard MIT license text (previously missing the "subject to the following conditions" and liability disclaimer clauses).
- Build files (`CMakeLists.txt`/`Makefile`) updated for the new GTK3 and Imlib2 dependencies.

### Notes
- This release significantly increases Kohiko's dependency footprint (GTK3, Imlib2) purely for the launcher's icon handling; later releases (0.15.0) replace the GTK3 dependency with a custom icon-theme lookup implementation.
