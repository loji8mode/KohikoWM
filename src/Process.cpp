#include "Process.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace Kohiko
{

pid_t Process::Spawn(
    const std::string& command,
    const std::string& display)
{
    if (command.empty())
        return -1;

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
    // `pid` is -1 here only if fork() itself failed.
    return pid;
}

bool Process::IsDescendantOf(
    long pid,
    pid_t ancestor,
    int maxHops)
{
    if (pid <= 0 || ancestor <= 0)
        return false;

    long current = pid;

    for (int hop = 0; hop < maxHops; ++hop)
    {
        if (current == static_cast<long>(ancestor))
            return true;

        // pid 1 (init, or its userspace replacement) is as far up as
        // any process tree goes - nothing left to walk past that.
        if (current <= 1)
            return false;

        std::ifstream stat("/proc/" + std::to_string(current) + "/stat");

        if (!stat.is_open())
            return false;

        std::string line;
        std::getline(stat, line);

        // Fields are `pid (comm) state ppid ...` - `comm` (the
        // executable name) can itself contain spaces or parentheses,
        // so find the *last* ')' and split what follows on whitespace
        // rather than tokenizing the whole line from the start.
        auto close = line.rfind(')');

        if (close == std::string::npos || close + 2 >= line.size())
            return false;

        std::istringstream rest(line.substr(close + 2));

        char state = 0;
        long ppid = 0;

        rest >> state >> ppid;

        if (!rest)
            return false;

        current = ppid;
    }

    return current == static_cast<long>(ancestor);
}

}
