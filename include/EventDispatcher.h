#pragma once

#include <X11/Xlib.h>

namespace Kohiko
{

class WindowManager;
class KeyboardManager;
class MouseManager;

class EventDispatcher
{
public:

    EventDispatcher(
        WindowManager& wm,
        KeyboardManager& keyboard,
        MouseManager& mouse
    );

    void Dispatch(
        const XEvent& event
    );

private:

    WindowManager& m_windowManager;

    KeyboardManager& m_keyboard;

    MouseManager& m_mouse;

};

}