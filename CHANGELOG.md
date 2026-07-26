# Changelog

## Version 0.2.0

Release date: 2026-07-09

### Added
- `Command` / `CommandType`: a parsed, ready-to-run action type. Config binds and IPC `dispatch` text are now parsed once into a `Command` instead of being tokenized separately by each consumer.
- `KeyCombo` / `ParseCombo`: shared parsing of combo strings such as `SUPER+SHIFT+Q` or `SUPER+BTN1` into a modifier mask and trailing token, used by both `KeyboardManager` and `MouseManager`.
- `IpcPath` / `IpcSocketPath()`: shared, `$DISPLAY`-scoped Unix socket path used by both `IPCServer` and the new `kohikoctl` client.
- `Json` namespace: minimal helpers (`Escape`, `String`, `Boolean`) for producing the JSON text returned over IPC.
- `tools/kohikoctl.cpp`: a command-line IPC client (`dispatch`, `clients`, `monitors`, `activewindow`, `tree`, `reload`, `quit`).
- `tests/test_bsptree.cpp`: unit tests for the BSP tree, runnable without an X server.
- A plain `Makefile` build (`make`, `make test`, `make install`) alongside the existing CMake build.
- `BSPTree`: `Resize` (direction-aware ratio adjustment), `FindNeighbor` (directional focus search), `Rotate`, `Flip`, `HitTest`, and `Serialize` (JSON dump used by `kohikoctl tree`).
- `ManagedWindow`: floating geometry, window class/instance name, role, PID, monitor index, urgent flag, previous state (for restoring from fullscreen), border width, and ignored-unmap bookkeeping (to distinguish a WM-initiated unmap from a client closing).
- `WindowRepository`: `Focused()`, `Floating()`, `Scratchpad()`, and `Visible(workspace)` query helpers.
- `XAtoms`: `WM_PROTOCOLS`, `NET_WM_NAME`, `NET_WM_WINDOW_TYPE` (+`_DIALOG`), `NET_WM_PID`, `WM_WINDOW_ROLE`.
- `XConnection`: graceful window close (`WM_DELETE_WINDOW` with `XKillClient` fallback), border color, raise/lower, key/button grab helpers with NumLock/CapsLock lock-variant handling, and window property queries (title, class, role, PID, transient-for, dialog check).
- `LayoutEngine::Params` (inner gap, outer gap, border width, smart gaps, smart borders) replacing hardcoded layout constants.
- `Utils::ParsePercent` for parsing `"70%"`-style config values.
- New config keys: `general.border_color_active`/`_inactive`, `bar.background`/`foreground`/`active`, directional focus binds (`Super+H/J/K/L`), rotate/flip binds, move-to-workspace binds (`Super+Shift+1..0`), reload/quit binds.

### Changed
- `Config` rewritten from an `unordered_map` to an ordered list of key/value pairs, so repeatable directives (`bind=`, `exec.<n>=`) are preserved instead of only keeping the last value; added `GetFloat`, `GetBool`, `GetPercent`, and `GetAll` accessors.
- `IPCServer` reworked from a fire-and-forget `CommandHandler` to a synchronous request/response `Handler` that returns a string.
- `Bar` now reads its configuration from `Config`, tracks a scratchpad indicator, and draws using a real font/graphics context instead of placeholder drawing.
- `Scratchpad` reworked into an explicit state machine (`Assign`/`Forget`/`Toggle`) that no longer owns a `WindowRepository` reference directly.
- `WorkspaceManager` takes a configurable workspace count, tracks the previous workspace, and `Switch()` now returns whether the switch happened.
- `CursorManager` now connects to `XConnection` and switches to a resize cursor during a drag-resize.
- `README.md` replaced with a full project description (see Notes on 0.1.0 for the prior content mix-up); `CMakeLists.txt` now contains the actual build configuration.
- `config/default.conf` reorganized with section headers and comments; added bar colors and additional keybindings.

### Removed
- `InputManager`: its key/mouse grabbing responsibilities were absorbed directly into `KeyboardManager` and `MouseManager`, each of which now owns a `Configure(config)` method that grabs its own bindings.
- `Renderer`: removed; it was not referenced by `WindowManager` or any other component in 0.1.0.
- `scratchpad.center` config key: removed from `config/default.conf`; scratchpad centering is not exposed as a separate setting in this release.

### Notes
- The project's top-level folder name changed from `Kohiko` to `Kohiko2` in this release's archive.
