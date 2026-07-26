#include "EventLoop.h"

#include "WindowManager.h"
#include "XConnection.h"

#include <X11/Xlib.h>

namespace Kohiko
{

EventLoop::EventLoop(
    XConnection& connection,
    WindowManager& wm)
    :
    m_connection(connection),
    m_windowManager(wm)
{
}

void EventLoop::Run()
{
    while (m_running)
    {
        XEvent event;

        XNextEvent(
            m_connection.Display(),
            &event
        );

        switch (event.type)
        {
            case MapRequest:

                m_windowManager.HandleMapRequest(
                    event.xmaprequest
                );

                break;

            case ConfigureRequest:

                m_windowManager.HandleConfigureRequest(
                    event.xconfigurerequest
                );

                break;

            case DestroyNotify:

                m_windowManager.HandleDestroyNotify(
                    event.xdestroywindow
                );

                break;

            case UnmapNotify:

                m_windowManager.HandleUnmapNotify(
                    event.xunmap
                );

                break;

            case EnterNotify:

                m_windowManager.HandleEnterNotify(
                    event.xcrossing
                );

                break;

            default:
                break;
        }
    }
}

void EventLoop::Stop()
{
    m_running = false;
}

}