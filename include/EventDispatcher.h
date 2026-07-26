#pragma once

#include <X11/Xlib.h>

namespace Kohiko
{

class WindowManager;
class KeyboardManager;
class MouseManager;

// The single dispatch point for every X11 event Kohiko receives - one
// switch on event.type, exactly as the spec asks for. EventLoop just
// drains XPending() into this; nothing else decides where an event
// goes.
class EventDispatcher
{
public:

    EventDispatcher(
        WindowManager& windowManager,
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
