#include "EventDispatcher.h"

#include "KeyboardManager.h"
#include "MouseManager.h"
#include "WindowManager.h"

namespace Kohiko
{

EventDispatcher::EventDispatcher(
    WindowManager& wm,
    KeyboardManager& keyboard,
    MouseManager& mouse)
    :
    m_windowManager(wm),
    m_keyboard(keyboard),
    m_mouse(mouse)
{
}

void EventDispatcher::Dispatch(
    const XEvent& event)
{
    switch(event.type)
    {
        case KeyPress:

            m_keyboard.HandleKeyPress(
                event.xkey);

            break;

        case ButtonPress:

            m_mouse.HandleButtonPress(
                event.xbutton);

            break;

        case ButtonRelease:

            m_mouse.HandleButtonRelease(
                event.xbutton);

            break;

        case MotionNotify:

            m_mouse.HandleMotion(
                event.xmotion);

            break;

        default:

            break;
    }
}

}