# Changelog

## Version 0.1.0

Release date: 2026-07-07

### Added
- Initial release of Kohiko, a tiling window manager for X11 written in C++20, using a BSP (binary space partitioning) layout.
- Core BSP tree implementation (`BSPTree`, `BSPNode`, `BSPLeaf`, `BSPSplit`) for tiled window layout.
- `LayoutEngine` to translate the BSP tree into on-screen pixel geometry.
- `WindowManager` coordinating X11 event handling, window lifecycle (map/unmap/destroy/configure), focus, and workspace switching.
- `WorkspaceManager` / `Workspace` for workspace bookkeeping.
- `MonitorManager` / `Monitor` for monitor geometry detection.
- `WindowRepository` / `ManagedWindow` for tracking managed windows and their state (geometry, title, workspace, focus, floating/fullscreen/scratchpad state).
- `KeyboardManager` and `MouseManager` for key bindings and Super+drag mouse gestures (swap and resize).
- `InputManager` for grabbing keyboard/mouse input.
- `CursorManager` for root window cursor setup.
- `EventLoop` / `EventDispatcher` for the main X11 event loop.
- `XConnection` and `XAtoms` as the Xlib access layer.
- `IPCServer` for a Unix-socket control protocol.
- `Scratchpad` for a single toggleable hidden/shown window.
- `Bar` for a minimal always-on-top status bar.
- `Renderer`, `Config`, `ConfigParser`, `Process`, `Logger`, `Utils` supporting classes.
- `config/default.conf` defining initial configuration keys (gaps, border size, bar height, focus-follows-mouse, scratchpad size, workspace count) and keybindings (launch terminal/launcher, close, toggle floating, fullscreen, scratchpad toggle, workspace switching 1-10).
- Build scripts (`scripts/build.sh`, `scripts/run.sh`, `scripts/run-xephyr.sh`) and an MIT `LICENSE`.

### Notes
- In this release, `README.md` contains what appears to be CMake project configuration text, while `CMakeLists.txt` itself is empty — the content of the two files appears to have been swapped. This is corrected in version 0.2.0, where `README.md` first contains a full project description and `CMakeLists.txt` contains a working build configuration.
- The `LICENSE` file in this release contains an abbreviated MIT license text missing the standard "subject to the following conditions" and liability disclaimer clauses; the full standard text appears starting in version 0.4.0.
- `include/Version.h` reports `"0.1.0"` in this release; this string is not updated in any subsequent release covered by this history, so it does not track the actual project version past this point.
