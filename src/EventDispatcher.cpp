#include "EventDispatcher.h"

#include "KeyboardManager.h"
#include "MouseManager.h"
#include "WindowManager.h"

namespace Kohiko
{

EventDispatcher::EventDispatcher(
    WindowManager& windowManager,
    KeyboardManager& keyboard,
    MouseManager& mouse)
    :
    m_windowManager(windowManager),
    m_keyboard(keyboard),
    m_mouse(mouse)
{
}

void EventDispatcher::Dispatch(
    const XEvent& event)
{
    switch (event.type)
    {
        case MapRequest:
            m_windowManager.HandleMapRequest(event.xmaprequest);
            break;

        case ConfigureRequest:
            m_windowManager.HandleConfigureRequest(event.xconfigurerequest);
            break;

        case UnmapNotify:
            m_windowManager.HandleUnmapNotify(event.xunmap);
            break;

        case DestroyNotify:
            m_windowManager.HandleDestroyNotify(event.xdestroywindow);
            break;

        case EnterNotify:
            m_windowManager.HandleEnterNotify(event.xcrossing);
            break;

        case PropertyNotify:
            m_windowManager.HandlePropertyNotify(event.xproperty);
            break;

        // Every managed window has FocusChangeMask selected (see
        // WindowManager::Manage()), so this fires no matter *how* a
        // window ends up focused - through our own SetInputFocus calls
        // as much as through a client calling XSetInputFocus on itself
        // directly, which nothing in the X protocol stops a client
        // from doing regardless of what the window manager wants. This
        // is what lets WindowManager notice and immediately hand focus
        // back to the Launcher/Notepad when that happens (see
        // WindowManager::HandleFocusIn()) instead of the modal quietly
        // going deaf to the keyboard.
        case FocusIn:
            m_windowManager.HandleFocusIn(event.xfocus);
            break;

        case Expose:
            m_windowManager.HandleExpose(event.xexpose);
            break;

        case ClientMessage:
            m_windowManager.HandleClientMessage(event.xclient);
            break;

        case KeyPress:

            // The Launcher/Notepad get first refusal on every key
            // while either is open (see WindowManager::Manage()'s
            // Super+Q hardening notes for why this is a plain
            // conditional here and never an XGrabKeyboard) - only
            // fall through to ordinary hotkey matching once neither
            // is open.
            if (!m_windowManager.HandleModalKeyPress(event.xkey))
                m_keyboard.HandleKeyPress(event.xkey);

            break;

        case ButtonPress:

    if (event.xbutton.window ==
        m_windowManager.LauncherWindowId())
    {
        m_windowManager.HandleLauncherButtonPress(
            event.xbutton);
    }
    else if (event.xbutton.state & Mod4Mask)
    {
        m_mouse.HandlePress(event.xbutton);
    }
    else
    {
        m_windowManager.HandleButtonPressOnClient(
            event.xbutton);
    }

    break;

        case ButtonRelease:
            m_mouse.HandleRelease(event.xbutton);
            break;

        case MotionNotify:
            m_mouse.HandleMotion(event.xmotion);
            break;

        default:
            break;
    }
}

}
