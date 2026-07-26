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
// either a pick-up-and-drop swap (LMB on a tiled window), a live
// follow-the-cursor move (LMB on a floating window - see
// WindowManager::BeginFloatingDrag()), or a live ratio resize (RMB).
//
//   LMB on a tiled window:    press -> hit-test a leaf -> WindowManager
//        detaches it and follows the cursor with it on every motion
//        event (nothing else moves) -> release -> WindowManager
//        hit-tests the drop point and either swaps the two windows or
//        snaps back, both cases animated into place
//   LMB on a floating window: press -> hit-test the topmost floating
//        window under the cursor -> WindowManager moves it with the
//        cursor 1:1 on every motion event, re-homing it onto whichever
//        monitor it's currently over as it crosses (see
//        WindowManager::UpdateFloatingDrag()) -> release -> it stays
//        exactly there, clamped to its final monitor
//   RMB on a tiled window:     press -> hit-test a leaf -> cursor
//        moves -> feed the pixel delta straight into Resize() every
//        motion event -> repeat
//   RMB on a floating window:  press -> hit-test the topmost floating
//        window under the cursor -> WindowManager grows/shrinks
//        whichever edge(s) it was grabbed nearest to, live, on every
//        motion event, anchoring the opposite edge(s) in place (see
//        WindowManager::UpdateFloatingResize()) -> release -> it stays
//        exactly that size, clamped to its minimum and its monitor
//
// Also the entry point for ambient pointer tracking: every
// MotionNotify that arrives while no drag is active (root has
// PointerMotionMask selected all the time - see XConnection::
// BecomeWindowManager()) is forwarded to WindowManager::
// HandlePointerMotion() instead, purely so "focused monitor" can
// follow the cursor across bare desktop between monitors.
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
        Move,
        Resize,
        FloatingResize
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
