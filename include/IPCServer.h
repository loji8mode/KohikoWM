#pragma once

#include <functional>
#include <string>

namespace Kohiko
{

// One-shot request/response IPC over a Unix domain socket, in the
// spirit of hyprctl: kohikoctl connects, sends one line, reads one
// response, disconnects. Simple enough to run out of the same
// select() loop as the X11 connection - no threads needed, and every
// request is answered on the main thread so there is never a data
// race with the rest of Kohiko's state.
class IPCServer
{
public:

    using Handler = std::function<std::string(const std::string&)>;

    ~IPCServer();

    // Creates the listening socket at `path` (removing any stale
    // socket file left over from a previous run first).
    bool Start(
        const std::string& path,
        Handler handler
    );

    void Stop();

    int ListenFd() const;

    // Call when ListenFd() is readable: accepts one connection, reads
    // its request, runs the handler, writes back the response, and
    // closes it - all synchronously (fine: it's a local socket and
    // requests/responses are tiny).
    void Poll();

private:

    int m_listenFd = -1;
    std::string m_path;
    Handler m_handler;

};

}
