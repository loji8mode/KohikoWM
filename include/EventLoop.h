#pragma once

namespace Kohiko
{

class XConnection;
class WindowManager;

class EventLoop
{
public:

    EventLoop(
        XConnection& connection,
        WindowManager& wm
    );

    void Run();

    void Stop();

private:

    XConnection& m_connection;

    WindowManager& m_windowManager;

    bool m_running = true;

};

}