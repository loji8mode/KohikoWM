#pragma once

#include <X11/Xlib.h>

namespace Kohiko
{

class WindowManager;

class MouseManager
{
public:

    explicit MouseManager(
        WindowManager& wm
    );

    void HandleButtonPress(
        const XButtonEvent&
    );

    void HandleButtonRelease(
        const XButtonEvent&
    );

    void HandleMotion(
        const XMotionEvent&
    );

private:

    WindowManager& m_windowManager;

    bool m_dragging = false;

    bool m_resizing = false;

};

}