#pragma once

#include <string>
#include <sys/types.h>

namespace Kohiko
{

class Process
{
public:

    // Runs `command` through /bin/sh -c in a detached child process.
    // Relies on SIGCHLD being set to SIG_IGN at startup (see
    // Application::Run()) so children are auto-reaped by the kernel
    // and never pile up as zombies.
    //
    // `display`, when non-empty, is forced into the child's DISPLAY
    // environment variable before exec. This is what keeps every
    // spawned program on *this* Kohiko's X11 session instead of
    // whatever DISPLAY (a different, outer session; none at all)
    // happened to be sitting in the environment Kohiko itself was
    // started from - previously the only thing that decided where a
    // launched app appeared was environment inheritance, which is why
    // some apps could end up opening in an outer/different session
    // (e.g. Openbox) instead of the nested Kohiko one.
    //
    // Returns the immediate child's pid (-1 if fork() itself failed,
    // or `command` was empty) - the pid of the `/bin/sh -c` process
    // itself, not necessarily the launched program's own pid: `sh -c`
    // commonly forks again internally to actually run `command` rather
    // than execing it in place, so the program's real pid is normally
    // one level *below* what's returned here. That's exactly what
    // IsDescendantOf() exists to bridge - a caller that wants to line
    // this pid up against a window that shows up later (via its own
    // _NET_WM_PID) needs to walk up from the window's pid, not compare
    // it directly, e.g. to place it on a particular workspace
    // (`workspace<N>=` - see WindowManager::RunAutostart()).
    static pid_t Spawn(
        const std::string& command,
        const std::string& display = std::string()
    );

    // True if `pid` either *is* `ancestor`, or descends from it within
    // the local process tree - walked via /proc/<pid>/stat's own
    // parent pid, at most `maxHops` times. This is the normal way to
    // resolve a Spawn()'d pid against a window that shows up later, not
    // just a rare fallback: `sh -c` alone already puts one fork between
    // the two (see Spawn()), before any launcher script the program
    // itself re-forks through (Steam's own launch script is a common
    // example) adds more.
    static bool IsDescendantOf(
        long pid,
        pid_t ancestor,
        int maxHops = 10
    );

};

}
