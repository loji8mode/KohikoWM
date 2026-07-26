#pragma once

#include "Types.h"

#include <X11/Xlib.h>

#include <string>

namespace Kohiko
{

class XConnection;
class Config;

// A minimal always-on-top status bar: workspace list, active window
// title, a scratchpad indicator, and a clock. Drawn with plain core
// Xlib text/rectangles (no Xft/fontconfig, no GTK/Qt) - matching the
// spec's "own bar, no toolkit" goal and keeping the dependency list
// (and therefore the whole point of this project) as light as
// possible.
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

    void SetScratchpadActive(
        bool active
    );

    // Lights up a "[N]" indicator - the doc asks for a small icon
    // ("for example, 📝") to show whether the Notepad exists/is open,
    // but Kohiko's bar only ever draws with a plain X11 core font (no
    // Xft/fontconfig), which has no emoji glyphs to draw with, so this
    // uses the same bracketed-letter style as the scratchpad indicator
    // instead of a glyph that would just render as a missing-tofu box.
    void SetNotepadActive(
        bool active
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
    XFontStruct* m_font = nullptr;

    Rect m_geometry;
    int m_height = 26;

    bool m_visible = false;

    int m_workspaceCount = 10;
    int m_currentWorkspace = 1;
    std::string m_title;
    bool m_scratchpadActive = false;
    bool m_notepadActive = false;

    unsigned long m_backgroundPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_activePixel = 0;

};

}
