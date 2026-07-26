#include "Process.h"

#include <cstdlib>
#include <unistd.h>

namespace Kohiko
{

void Process::Spawn(
    const std::string& command,
    const std::string& display)
{
    if (command.empty())
        return;

    pid_t pid = fork();

    if (pid == 0)
    {
        // Child: start a new session so the launched program isn't
        // tied to Kohiko's controlling terminal, then hand off to a
        // shell so config entries can use ordinary shell syntax
        // (arguments, quoting, `&&`, etc).
        setsid();

        // Pin this process to Kohiko's own X11 session explicitly,
        // rather than trusting whatever DISPLAY was inherited. setenv's
        // overwrite=1 makes sure a stale/foreign value already present
        // in the environment loses to the real one.
        if (!display.empty())
            setenv("DISPLAY", display.c_str(), 1);

        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));

        _exit(127); // execl() only returns on failure
    }

    // Parent: nothing to wait for - SIGCHLD is SIG_IGN (see
    // Application::Run()), so the kernel reaps `pid` on its own.
}

}
