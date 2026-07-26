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
    static void Spawn(
        const std::string& command
    );

};

}
