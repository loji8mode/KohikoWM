# Changelog

## Version 0.14.0

Release date: 2026-07-20

### Added
- `workspace<N>=` repeatable config directive (N from 1 to `workspace.count`): launches programs at startup exactly like `auto_start_programs=`, but pins each one to workspace N once its window actually appears, rather than wherever happened to be focused when Kohiko started.
- `Process::IsDescendantOf()`: walks `/proc/<pid>/stat` parent-pid links to determine whether a window's `_NET_WM_PID` descends from a process Kohiko spawned, since the immediate child of `Process::Spawn()` (a `/bin/sh -c` process) is often an ancestor of, rather than identical to, the eventual application's own pid (more so for programs with their own launcher script, such as Steam).
- `WindowManager::ResolveWorkspaceAutostart()`: matches a newly-managed window's pid against pending `workspace<N>=` launches, within a 60-second eligibility window so a long-running process opening an unrelated window later in the session is not redirected.

### Changed
- `Process::Spawn()` now returns the spawned process's pid (`-1` on failure) instead of returning nothing.
- A `workspace<N>=` placement takes precedence over a window's parent-attachment (from 0.10.0/0.11.0), but an explicit `windowrule=workspace:N` still wins over both.
