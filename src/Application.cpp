#include "Application.h"

#include "Logger.h"
#include "Version.h"

namespace Kohiko
{

int Application::Run()
{
    Logger::Info("Kohiko");
    Logger::Info(VERSION);

    if (!m_connection.Connect())
        return 1;

    if (!m_connection.BecomeWindowManager())
        return 1;

    m_windowManager =
        new WindowManager(
            m_connection
        );

    m_eventLoop =
        new EventLoop(
            m_connection,
            *m_windowManager
        );

    m_eventLoop->Run();

    delete m_eventLoop;

    delete m_windowManager;

    return 0;
}

}