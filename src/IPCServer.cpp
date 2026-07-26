#include "IPCServer.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace Kohiko
{

IPCServer::IPCServer()
{
}

IPCServer::~IPCServer()
{
    Stop();
}

bool IPCServer::Start(
    const std::string& path)
{
    m_socket =
        socket(
            AF_UNIX,
            SOCK_STREAM,
            0);

    if(m_socket < 0)
        return false;

    sockaddr_un addr{};

    addr.sun_family = AF_UNIX;

    std::snprintf(
        addr.sun_path,
        sizeof(addr.sun_path),
        "%s",
        path.c_str());

    unlink(path.c_str());

    if(bind(
        m_socket,
        reinterpret_cast<
            sockaddr*>(&addr),
        sizeof(addr)) < 0)
        return false;

    listen(
        m_socket,
        5);

    fcntl(
        m_socket,
        F_SETFL,
        O_NONBLOCK);

    return true;
}

void IPCServer::Stop()
{
    if(m_client >= 0)
    {
        close(m_client);

        m_client = -1;
    }

    if(m_socket >= 0)
    {
        close(m_socket);

        m_socket = -1;
    }
}

void IPCServer::Poll()
{
    if(m_socket < 0)
        return;

    if(m_client < 0)
    {
        m_client =
            accept(
                m_socket,
                nullptr,
                nullptr);

        return;
    }

    char buffer[1024];

    ssize_t len =
        read(
            m_client,
            buffer,
            sizeof(buffer)-1);

    if(len <= 0)
        return;

    buffer[len] = '\0';

    if(m_handler)
        m_handler(buffer);
}

void IPCServer::SetHandler(
    CommandHandler handler)
{
    m_handler =
        std::move(handler);
}

}