#include "EventLoop.h"

#include "EventDispatcher.h"
#include "IPCServer.h"
#include "WindowManager.h"
#include "XConnection.h"

#include <X11/Xlib.h>

#include <cerrno>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

namespace Kohiko
{

EventLoop::EventLoop(
    XConnection& connection,
    WindowManager& windowManager)
    :
    m_connection(connection),
    m_windowManager(windowManager)
{
}

void EventLoop::Run()
{
    m_running = true;

    Display* display = m_connection.GetDisplay();
    int xfd = m_connection.ConnectionFd();

    while (m_running && m_windowManager.IsRunning())
    {
        // Drain everything Xlib already has buffered before going
        // back to select() - XPending() does a non-blocking read
        // itself if the fd looks readable but the buffer is empty.
        while (XPending(display) > 0)
        {
            XEvent event;
            XNextEvent(display, &event);
            m_windowManager.Dispatcher().Dispatch(event);

            if (!m_windowManager.IsRunning())
                break;
        }

        if (!m_windowManager.IsRunning())
            break;

        int ipcFd = m_windowManager.Ipc().ListenFd();

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(xfd, &readSet);

        int maxFd = xfd;

        if (ipcFd >= 0)
        {
            FD_SET(ipcFd, &readSet);

            if (ipcFd > maxFd)
                maxFd = ipcFd;
        }

        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(maxFd + 1, &readSet, nullptr, nullptr, &timeout);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;

            break;
        }

        if (ready == 0)
        {
            m_windowManager.Tick();
            continue;
        }

        if (ipcFd >= 0 && FD_ISSET(ipcFd, &readSet))
            m_windowManager.Ipc().Poll();
    }
}

void EventLoop::Stop()
{
    m_running = false;
}

}
