#pragma once

#include <functional>
#include <string>

namespace Kohiko
{

class IPCServer
{
public:

    using CommandHandler =
        std::function<void(
            const std::string&
        )>;

    IPCServer();

    ~IPCServer();

    bool Start(
        const std::string& socketPath
    );

    void Stop();

    void Poll();

    void SetHandler(
        CommandHandler handler
    );

private:

    int m_socket = -1;

    int m_client = -1;

    CommandHandler m_handler;

};

}