# Changelog

## Version 0.17.0

Release date: 2026-07-26

### Added
- Kohiko Settings (`kohiko-settings`): a native configuration GUI, built as its own standalone application (not part of the `kohiko` process) and installed automatically alongside `kohiko`/`kohikoctl`, with its own `.desktop` entry (`desktop/kohiko-settings.desktop`) and icon (`assets/icons/kohiko-settings.svg`) so it's discoverable through the launcher or any standards-compliant menu. It renders every setting described by `ConfigSchema` (previously dormant scaffolding, now actually used) grouped into a category sidebar with sub-headings, offers a search box filtering by key/label/description/group, an (i) info icon per setting showing its description/default/allowed values, inline validation of typed values (numbers, colors, percentages, and rule/keybinding syntax), and Apply/Save/Reset to Default actions.
- `ConfigWriter` (`include/ConfigWriter.h`/`.cpp`): the write path Kohiko Settings uses to save changes back into `kohiko.conf`, editing existing lines, comments, and ordering in place rather than regenerating the file, so hand-editing the config remains fully supported alongside the GUI. A new key with no existing line is appended under a `# --- Added by Kohiko Settings ---` marker instead of being inserted elsewhere in the file.
- `lockscreen.after` config key (`never`/`manual`/`suspend`/`always`), replacing the previous plain on/off `lockscreen.lock_on_suspend` toggle: `never` disables locking entirely, including the manual lock command; `manual` allows locking on demand only; `suspend` (the default) also locks automatically before Suspend; `always` additionally locks once at Kohiko startup.
- Lock screen clock and date display (`lockscreen.show_clock`, `show_date`, `clock_format`, `date_format`), and optional hostname/username display above the password field (`lockscreen.show_hostname`, `show_username`).
- `Utils::SecureErase()`: overwrites a string's buffer with zeros before clearing it; used by the lock screen to wipe the typed password from memory immediately after every authentication attempt (successful, failed, or cancelled via `Escape`), as a best-effort mitigation beyond what a plain `clear()` provides.

### Changed
- `ConfigSchema::ValueType::Bool` renamed to `Boolean`.
- Default config: `session.restore_priority` default changed from `session` back to `config`; `workspace1=`/`workspace2=`/`workspace3=` example autostart lines commented out again; `auto_start_programs` default restored to `Telegram discord zen-browser flameshot`.
- `scripts/install-arch.sh` updated to note that `kohiko-settings` is installed automatically alongside `kohiko`, with no additional dependency.

### Removed
- `lockscreen.lock_on_suspend` config key, replaced by `lockscreen.after` with no automatic migration; a config file still setting `lockscreen.lock_on_suspend` has no effect under this release.

### Notes
- `kohiko-settings` is built from a deliberately small, shared subset of the project's own source files (`Config`, `ConfigSchema`, `Utils`, `Font`, plus its own `SettingsWindow`/`ConfigWriter`) rather than linking against the full `kohiko` binary, so it carries none of the window-manager-only code (BSP layout, EWMH handling, XRandr monitor detection, and so on) and needs nothing beyond the X11/Xft dependencies `kohiko` itself already requires.
- The four repeatable config directives (`bind=`, `exec.<name>=`, `windowrule=`, `monitor=`) are edited in Kohiko Settings as raw, one-line-per-entry text blocks rather than individually-typed fields; `workspace<N>=` autostart is the exception, shown as one plain text field per workspace since each is a distinct key.
