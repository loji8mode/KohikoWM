#pragma once

#include "Types.h"

#include <X11/Xlib.h>

#include <string>

namespace Kohiko
{

class XConnection
{
public:

    XConnection();

    ~XConnection();

    bool Connect();

    bool BecomeWindowManager();

    void Disconnect();

    Display* Display() const;

    ::Window Root() const;

    int Screen() const;

    void Flush();

    void Sync();

    void MapWindow(::Window window);

    void UnmapWindow(::Window window);

    void DestroyWindow(::Window window);

    void MoveResizeWindow(
        ::Window window,
        const Rect& rect
    );

    void SetInputFocus(::Window window);

    std::string LastError() const;

private:

    static int ErrorHandler(
        Display*,
        XErrorEvent*
    );

private:

    Display* m_display = nullptr;

    ::Window m_root = 0;

    int m_screen = 0;

    bool m_wmExists = false;

    std::string m_lastError;

    static XConnection* s_instance;

};

}