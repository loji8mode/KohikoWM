# Changelog

## Version 0.8.2

Release date: 2026-07-15

### Fixed
- Fixed a rendering glitch (most reliably reproduced with Java/Swing applications such as TLauncher) where a window mapped at its own small requested size and then immediately resized to its actual tile slot could end up with its content stuck in a corner, because the resize was applied after the window was mapped rather than before. `WindowManager::Manage()` and the workspace-switch path now call `Arrange()` to size a window into its real tile geometry before mapping it, instead of after.
