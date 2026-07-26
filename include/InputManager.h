#pragma once

#include <X11/Xlib.h>

namespace Kohiko
{

class WindowManager;
class XConnection;

class InputManager
{
public:

    InputManager(
        XConnection& connection,
        WindowManager& windowManager
    );

    void Initialize();

private:

    void GrabKeys();

    void GrabMouse();

private:

    XConnection& m_connection;

    WindowManager& m_windowManager;

};

}