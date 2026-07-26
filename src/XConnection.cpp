#include "XConnection.h"

#include <X11/Xutil.h>

namespace Kohiko
{

XConnection* XConnection::s_instance = nullptr;

XConnection::XConnection()
{
}

XConnection::~XConnection()
{
    Disconnect();
}

bool XConnection::Connect()
{
    m_display = XOpenDisplay(nullptr);

    if (!m_display)
    {
        m_lastError = "Cannot open display.";
        return false;
    }

    s_instance = this;

    m_screen =
        DefaultScreen(m_display);

    m_root =
        RootWindow(
            m_display,
            m_screen
        );

    return true;
}

bool XConnection::BecomeWindowManager()
{
    XSetErrorHandler(
        ErrorHandler
    );

    XSelectInput(
        m_display,
        m_root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        StructureNotifyMask |
        PropertyChangeMask |
        EnterWindowMask |
        LeaveWindowMask
    );

    XSync(
        m_display,
        False
    );

    if (m_wmExists)
    {
        m_lastError =
            "Window manager already running.";

        return false;
    }

    return true;
}

void XConnection::Disconnect()
{
    if (!m_display)
        return;

    XCloseDisplay(m_display);

    m_display = nullptr;
}

Display* XConnection::Display() const
{
    return m_display;
}

::Window XConnection::Root() const
{
    return m_root;
}

int XConnection::Screen() const
{
    return m_screen;
}

void XConnection::Flush()
{
    XFlush(m_display);
}

void XConnection::Sync()
{
    XSync(
        m_display,
        False
    );
}

void XConnection::MapWindow(::Window window)
{
    XMapWindow(
        m_display,
        window
    );
}

void XConnection::UnmapWindow(::Window window)
{
    XUnmapWindow(
        m_display,
        window
    );
}

void XConnection::DestroyWindow(::Window window)
{
    XDestroyWindow(
        m_display,
        window
    );
}

void XConnection::MoveResizeWindow(
    ::Window window,
    const Rect& rect)
{
    XMoveResizeWindow(
        m_display,
        window,
        rect.x,
        rect.y,
        rect.width,
        rect.height
    );
}

void XConnection::SetInputFocus(
    ::Window window)
{
    XSetInputFocus(
        m_display,
        window,
        RevertToPointerRoot,
        CurrentTime
    );
}

std::string XConnection::LastError() const
{
    return m_lastError;
}

int XConnection::ErrorHandler(
    Display*,
    XErrorEvent* event)
{
    if (!s_instance)
        return 0;

    if (event->error_code == BadAccess)
        s_instance->m_wmExists = true;

    return 0;
}

}