#include "IPCServer.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>

namespace Kohiko
{

IPCServer::~IPCServer()
{
    Stop();
}

bool IPCServer::Start(
    const std::string& path,
    Handler handler)
{
    m_handler = std::move(handler);
    m_path = path;

    ::unlink(path.c_str()); // clear a stale socket from a previous run

    m_listenFd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (m_listenFd < 0)
        return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());

    if (bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    if (listen(m_listenFd, 8) < 0)
    {
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    return true;
}

void IPCServer::Stop()
{
    if (m_listenFd >= 0)
    {
        ::close(m_listenFd);
        m_listenFd = -1;
    }

    if (!m_path.empty())
    {
        ::unlink(m_path.c_str());
        m_path.clear();
    }
}

int IPCServer::ListenFd() const
{
    return m_listenFd;
}

void IPCServer::Poll()
{
    if (m_listenFd < 0)
        return;

    int client = accept(m_listenFd, nullptr, nullptr);

    if (client < 0)
        return;

    std::string request;
    char buffer[4096];

    ssize_t received = read(client, buffer, sizeof(buffer) - 1);

    if (received > 0)
    {
        request.assign(buffer, static_cast<std::size_t>(received));

        while (!request.empty() && (request.back() == '\n' || request.back() == '\r'))
            request.pop_back();
    }

    std::string response = m_handler ? m_handler(request) : std::string("error: no handler");
    response += '\n';

    ssize_t written = write(client, response.data(), response.size());
    (void)written;

    ::close(client);
}

}
