#include "InputManager.h"

#include "WindowManager.h"
#include "XConnection.h"

#include <X11/keysym.h>

namespace Kohiko
{

InputManager::InputManager(
    XConnection& connection,
    WindowManager& windowManager)
    :
    m_connection(connection),
    m_windowManager(windowManager)
{
}

void InputManager::Initialize()
{
    GrabKeys();

    GrabMouse();
}

void InputManager::GrabKeys()
{
    Display* display =
        m_connection.Display();

    Window root =
        m_connection.Root();

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_Return),
        Mod4Mask,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_q),
        Mod4Mask,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_F1),
        Mod4Mask,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );
}

void InputManager::GrabMouse()
{
    Display* display =
        m_connection.Display();

    Window root =
        m_connection.Root();

    XGrabButton(
        display,
        Button1,
        Mod4Mask,
        root,
        True,
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None
    );

    XGrabButton(
        display,
        Button3,
        Mod4Mask,
        root,
        True,
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None
    );
}

}