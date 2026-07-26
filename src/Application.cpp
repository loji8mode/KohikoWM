#include "Application.h"

#include "EventLoop.h"
#include "Logger.h"
#include "Version.h"
#include "WindowManager.h"

#include <csignal>

namespace Kohiko
{

int Application::Run(
    const std::string& configPath)
{
    // Reap Process::Spawn()'s launched children automatically instead
    // of letting them pile up as zombies, and don't die just because
    // a client vanished mid-write on some socket.
    std::signal(SIGCHLD, SIG_IGN);
    std::signal(SIGPIPE, SIG_IGN);

    EventLoop::InstallSignalHandlers();

    Logger::Info(std::string("kohiko ") + VERSION + " starting");

    if (!m_connection.Connect())
    {
        Logger::Fatal(m_connection.LastError());
        return 1;
    }

    if (!m_connection.BecomeWindowManager())
    {
        Logger::Fatal(m_connection.LastError());
        return 1;
    }

    if (!m_config.Load(configPath))
        Logger::Warning("could not read " + configPath + ", using built-in defaults");

    WindowManager windowManager(m_connection, m_config, configPath);
    windowManager.Initialize();

    Logger::Info("ready");

    EventLoop loop(m_connection, windowManager);
    loop.Run();

    windowManager.Shutdown();

    Logger::Info("shutting down");

    return 0;
}

}
