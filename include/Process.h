#pragma once

#include <string>

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
    static void Spawn(
        const std::string& command,
        const std::string& display = std::string()
    );

};

}
