# Changelog

## Version 0.16.0

Release date: 2026-07-26

### Added
- Native lock screen (`Super+Shift+L`, `kohikoctl dispatch lock`, or automatically before Suspend): a full-screen, per-monitor overlay with a hidden-input password field, authenticated via PAM (new `Authenticator.h`/`.cpp`, using a `pam/kohiko` service file that must be installed as `/etc/pam.d/kohiko`) and implemented with a real keyboard/pointer grab so Kohiko's own global hotkeys and any client attempting to steal focus cannot receive input while locked. If the current account has no password configured, locking unlocks immediately (detected by attempting authentication with an empty password). Configurable via `lockscreen.*` keys (colors, optional background image and logo, font, and `lockscreen.lock_on_suspend`).
- Power menu: every bar gets a `[Power]` button opening a popup with Shutdown/Restart/Suspend, each running a configurable shell command (`power.shutdown_command`/`_restart_command`/`_suspend_command`, defaulting to `systemctl poweroff`/`reboot`/`suspend`).
- Session restore (`SessionStore.h`/`.cpp`): every managed window's workspace, tiled/floating state, floating geometry, monitor, and fullscreen state are saved on shutdown (including on `SIGTERM`/`SIGINT`, not just a clean `kohikoctl quit`) and reapplied on the next startup, keyed by X11 window ID. `session.restore_priority` (`config` or `session`) decides whether a conflicting `windowrule=` or the saved session wins for a given window.
- Adoption of already-mapped windows at startup (`WindowManager::AdoptExistingWindows()`): restarting Kohiko, or starting it into a session where other windows are already open, now manages every pre-existing top-level window exactly as if it had just opened (tiled or floated per the same window rules), skipping override-redirect windows and dock-type panels.
- `ConfigSchema.h`/`.cpp`: a metadata registry (category, type, default, description, allowed values) describing every config setting, intended as groundwork for a possible future configuration GUI. Not read by anything at runtime in this release.
- Notepad: `Ctrl+Backspace`/`Ctrl+Delete` for whole-word deletion; the caret is now drawn as its own overlay rather than an inline character, so it no longer reflows the line or disappears depending on font.
- The Launcher and Notepad now open centered on whichever monitor currently has the mouse cursor, falling back to the focused monitor and then the primary monitor if the cursor is outside every monitor.

### Changed
- Default config: `auto_start_programs` reduced to `flameshot`; `workspace1=`/`workspace2=`/`workspace3=` (commented out since 0.14.1) are active again, now launching `zen-browser`, `discord`/`telegram-desktop`, and `steam` respectively.
- README's "Known limitations" section restructured into three sections: "Done" (implemented feature summary), "Planned" (a configuration GUI, backed by `ConfigSchema`), and "Intentionally unsupported" (design decisions not expected to change, such as the single-slot scratchpad and minimal notepad).
- Build files updated to require libpam; `scripts/install-arch.sh` and the README's build instructions updated accordingly, including installing the new `pam/kohiko` service file.

### Removed
- The "Kohiko doesn't adopt windows that were already mapped by a previous window manager" and "the launcher and notepad are single, global panels, always on the primary monitor" known limitations were removed, both resolved by this release.

### Notes
- `Authenticator::Authenticate()` runs the actual PAM conversation in a short-lived forked child process rather than in Kohiko's own process, after testing found that `pam_authenticate()`'s own internal fork (via `unix_chkpwd`) could hang when run directly inside a larger multi-threaded process.
- Locking the screen before issuing the configured suspend command (rather than trying to detect resume afterward) means the lock screen is already active the instant the machine wakes, without needing any `logind`/DBus sleep-signal integration.
