#pragma once

#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace Kohiko
{

class WindowManager;

class KeyboardManager
{
public:

    explicit KeyboardManager(
        WindowManager& wm
    );

    void HandleKeyPress(
        const XKeyEvent& event
    );

private:

    WindowManager& m_windowManager;

};

}