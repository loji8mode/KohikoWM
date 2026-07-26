#pragma once

#include "Types.h"

#include <X11/Xlib.h>

#include <string>

namespace Kohiko
{

class XConnection;
class WindowManager;
class Config;
class ManagedWindow;

// The interesting logic the spec calls out for this file: tracks a
// Super+drag from press through motion to release and turns it into
// either a pick-up-and-drop swap (LMB) or a live ratio resize (RMB).
//
//   LMB: press -> hit-test a leaf -> WindowManager detaches it and
//        follows the cursor with it on every motion event (nothing
//        else moves) -> release -> WindowManager hit-tests the drop
//        point and either swaps the two windows or snaps back, both
//        cases animated into place
//   RMB: press -> hit-test a leaf -> cursor moves -> feed the pixel
//        delta straight into Resize() every motion event -> repeat
//
// No floating-window move/resize-by-drag here on purpose - the spec
// is explicit that this is Hyprland-style BSP dragging, not the
// classic i3 floating-window drag.
class MouseManager
{
public:

    MouseManager(
        XConnection& connection,
        WindowManager& windowManager
    );

    // Reads mouse.swap / mouse.resize and grabs each one (with
    // NumLock/CapsLock variants) on the root window.
    void Configure(
        const Config& config
    );

    void HandlePress(
        const XButtonEvent& event
    );

    void HandleRelease(
        const XButtonEvent& event
    );

    void HandleMotion(
        const XMotionEvent& event
    );

private:

    struct Binding
    {
        unsigned int modifiers = 0;
        unsigned int button = 0;
    };

    enum class DragMode
    {
        Idle,
        Swap,
        Resize
    };

    bool Matches(
        const Binding& binding,
        unsigned int state,
        unsigned int button
    ) const;

    unsigned int ParseButton(
        const std::string& token
    ) const;

private:

    XConnection& m_connection;
    WindowManager& m_windowManager;

    Binding m_swapBinding;
    Binding m_resizeBinding;

    DragMode m_mode = DragMode::Idle;
    ManagedWindow* m_dragWindow = nullptr;

    int m_lastX = 0;
    int m_lastY = 0;

};

}
