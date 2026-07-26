#include "Process.h"

#include <unistd.h>

namespace Kohiko
{

void Process::Spawn(
    const std::string& command)
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

        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));

        _exit(127); // execl() only returns on failure
    }

    // Parent: nothing to wait for - SIGCHLD is SIG_IGN (see
    // Application::Run()), so the kernel reaps `pid` on its own.
}

}
