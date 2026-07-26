#include "KeyboardManager.h"

#include "Process.h"
#include "WindowManager.h"

#include <X11/keysym.h>

namespace Kohiko
{

KeyboardManager::KeyboardManager(
    WindowManager& wm)
    :
    m_windowManager(wm)
{
}

void KeyboardManager::HandleKeyPress(
    const XKeyEvent& event)
{
    KeySym key =
        XLookupKeysym(
            const_cast<XKeyEvent*>(&event),
            0);

    if(!(event.state & Mod4Mask))
        return;

    switch(key)
    {
        case XK_Return:

            Process::Spawn(
                "xterm");

            break;

        case XK_d:

            Process::Spawn(
                "jgmenu_run");

            break;

        case XK_1:

            m_windowManager.SwitchWorkspace(
                1);

            break;

        case XK_2:

            m_windowManager.SwitchWorkspace(
                2);

            break;

        case XK_3:

            m_windowManager.SwitchWorkspace(
                3);

            break;

        case XK_4:

            m_windowManager.SwitchWorkspace(
                4);

            break;

        case XK_5:

            m_windowManager.SwitchWorkspace(
                5);

            break;

        case XK_6:

            m_windowManager.SwitchWorkspace(
                6);

            break;

        case XK_7:

            m_windowManager.SwitchWorkspace(
                7);

            break;

        case XK_8:

            m_windowManager.SwitchWorkspace(
                8);

            break;

        case XK_9:

            m_windowManager.SwitchWorkspace(
                9);

            break;

        case XK_0:

            m_windowManager.SwitchWorkspace(
                10);

            break;

        default:
            break;
    }
}
}