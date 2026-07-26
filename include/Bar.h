#pragma once

#include "Font.h"
#include "Types.h"

#include <X11/Xlib.h>

#include <chrono>
#include <string>

namespace Kohiko
{

class XConnection;
class Config;
class SystemTray;

// A minimal always-on-top status bar: workspace list, active window
// title, a scratchpad indicator, and a clock. Drawn with plain Xlib
// shapes and Xft text (see Font.h) - no GTK/Qt, no fixed layout
// toolkit - matching the spec's "own bar, no toolkit" goal while still
// rendering every script a system font actually has installed for,
// not just the handful of charsets classic X11 core fonts cover.
class Bar
{
public:

    explicit Bar(
        XConnection& connection
    );

    ~Bar();

    void Configure(
        const Config& config,
        const Rect& monitorGeometry
    );

    void Show();

    void Hide();

    bool IsVisible() const;

    ::Window WindowId() const;

    void SetWorkspaces(
        int count,
        int current
    );

    void SetTitle(
        const std::string& title
    );

    // Replaces the title area with `text` for `durationMs` (default a
    // few seconds), then reverts to showing the real title again on
    // its own - Redraw() checks the expiry itself, so nothing needs to
    // explicitly clear it. Used for things the user needs to notice
    // right now but that aren't worth a persistent indicator (a
    // rejected workspace-switch request - see WindowManager::
    // SwitchWorkspaceOnMonitor()). Calling this again before the
    // previous one expired just replaces it/resets the timer.
    void ShowNotification(
        const std::string& text,
        int durationMs = 2500
    );

    void SetScratchpadActive(
        bool active
    );

    // Lights up a "[N]" indicator - the doc asks for a small icon
    // ("for example, 📝") to show whether the Notepad exists/is open,
    // but this uses the same bracketed-letter style as the scratchpad
    // indicator instead, since not every font covers emoji glyphs
    // either and a missing one would just draw a tofu box.
    void SetNotepadActive(
        bool active
    );

    // Wires up the system tray (see SystemTray.h) so Redraw() can
    // reposition its container flush against the right edge and leave
    // room for it when placing the clock. Kohiko has exactly one tray
    // for the lifetime of the process, so this is meant to be called
    // once, right after both Bar and SystemTray are constructed.
    void AttachSystemTray(
        SystemTray* tray
    );

    // Redraws everything, including the clock. Cheap enough to call
    // on every relevant WindowManager event plus once a second.
    void Redraw();

    int Height() const;

private:

    void DrawText(
        int x,
        int baseline,
        const std::string& text,
        unsigned long color
    );

private:

    XConnection& m_connection;

    ::Window m_window = 0;
    GC m_gc = nullptr;
    Font m_font;
    XftDraw* m_xftDraw = nullptr;

    Rect m_geometry;
    int m_height = 26;

    bool m_visible = false;

    int m_workspaceCount = 10;
    int m_currentWorkspace = 1;
    std::string m_title;
    bool m_scratchpadActive = false;
    bool m_notepadActive = false;

    // See ShowNotification() - empty/default-constructed time_point
    // when there's nothing to show; Redraw() clears m_notificationText
    // itself the first time it notices `now >= m_notificationExpiry`.
    std::string m_notificationText;
    std::chrono::steady_clock::time_point m_notificationExpiry;

    SystemTray* m_tray = nullptr;

    unsigned long m_backgroundPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_activePixel = 0;

};

}
