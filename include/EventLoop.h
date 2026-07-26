#pragma once

namespace Kohiko
{

class XConnection;
class WindowManager;

// Multiplexes the X11 connection and the IPC socket with select().
// Every pass drains whatever X events are already pending through
// WindowManager's EventDispatcher, then polls IPCServer if it has a
// waiting connection. A short timeout keeps the bar's clock ticking
// even when nothing else happens - no threads anywhere in Kohiko.
class EventLoop
{
public:

    EventLoop(
        XConnection& connection,
        WindowManager& windowManager
    );

    void Run();

    void Stop();

private:

    XConnection& m_connection;
    WindowManager& m_windowManager;

    bool m_running = true;

};

}
