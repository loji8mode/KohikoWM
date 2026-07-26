#pragma once

#include "Font.h"
#include "Types.h"

#include <X11/Xlib.h>

#include <functional>
#include <string>
#include <vector>

namespace Kohiko
{

class XConnection;
class Config;

// The bar's power button (see Bar::PowerButtonRect()) opens this: a
// tiny popup with exactly three rows - Shutdown, Restart, Suspend,
// per the spec - positioned just under wherever the button was
// clicked. Deliberately not built on Notepad/Launcher's shared
// machinery (XIM/XIC, text editing) since a fixed 3-item menu needs
// none of that - see Notepad.h for the widget this project already
// uses when real text entry is needed. Mouse-only (click a row to run
// it, click anywhere else/Escape to dismiss without running
// anything) - no hover highlighting, keeping this the same "plain
// Xlib shapes, no toolkit" minimal style as Bar itself.
class PowerMenu
{
public:

    explicit PowerMenu(
        XConnection& connection
    );

    ~PowerMenu();

    // Reads power.shutdown_command/restart_command/suspend_command -
    // safe to call again on a config reload, same as Bar/Launcher/
    // Notepad's own Configure().
    void Configure(
        const Config& config
    );

    // Called right before the Suspend row's command runs (if one has
    // been set) - see WindowManager's own call site for how this
    // integrates with LockScreen without PowerMenu needing to know
    // that class exists at all. Never required - a PowerMenu with no
    // callback set works exactly as it did before this existed.
    void SetSuspendCallback(
        std::function<void()> callback
    );

    // Opens with its top-left corner at `anchor` (the power button's
    // own on-screen position), clamped onto `monitorGeometry` so it
    // never opens partly off-screen for a bar sitting flush against
    // an edge.
    void Open(
        const Point& anchor,
        const Rect& monitorGeometry
    );

    void Close();

    bool IsOpen() const;

    ::Window WindowId() const;

    void HandleExpose();

    // Runs whichever row's command the click landed on (if any) via
    // Process::Spawn(). Doesn't close the window itself - a click
    // here always closes the menu regardless of which row (if any)
    // it landed on, so WindowManager::HandleBarOrPowerMenuButtonPress()
    // calls ClosePowerMenu() right after this either way, same
    // reasoning as HandleKeyPress() below.
    void HandleButtonPress(
        const XButtonEvent& event
    );

    // Escape signals "close without running anything" - true if
    // Escape was the key pressed. Doesn't close the window itself;
    // WindowManager::ClosePowerMenu() does that (see its own comment
    // for why that needs to be the single place focus-after-modal
    // gets restored from, matching Launcher/Notepad's own Close()
    // split).
    bool HandleKeyPress(
        const XKeyEvent& event
    );

private:

    void Redraw();

private:

    XConnection& m_connection;

    ::Window m_window = 0;
    GC m_gc = nullptr;
    Font m_font;
    XftDraw* m_xftDraw = nullptr;

    Rect m_geometry;
    bool m_open = false;

    unsigned long m_backgroundPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_borderPixel = 0;

    struct Row
    {
        std::string label;
        std::string command;
    };

    std::vector<Row> m_rows;
    int m_rowHeight = 28;
    std::function<void()> m_suspendCallback;

};

}
