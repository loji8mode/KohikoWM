#pragma once

#include "Font.h"
#include "Types.h"

#include <X11/Xlib.h>

#include <string>
#include <vector>

namespace Kohiko
{

class XConnection;
class Config;

enum class NotepadResult
{
    Editing,
    Closed // Escape - WindowManager hides it (Notepad::Close() also saves)
};

// A small, native, always-available scratch notepad (Super+N by
// default): free-form multi-line text, persisted to disk, toggled
// on/off like the Scratchpad. Deliberately NOT "spawn some GUI text
// editor into a hidden special workspace and toggle that" - that's
// the Hyprland-ecosystem pattern ("dropdown terminal"-style special
// workspaces wrapping an external program) the doc explicitly wants
// Kohiko to differ from - so this is a genuinely native widget, drawn
// the same way as Bar/Launcher: plain Xlib shapes plus Xft text (see
// Font.h), no GTK/Qt, no extra process.
class Notepad
{
public:

    explicit Notepad(
        XConnection& connection
    );

    ~Notepad();

    // Also eagerly loads any persisted content from disk, so
    // HasContent() is accurate from startup - not just after the
    // first time the notepad is opened.
    void Configure(
        const Config& config,
        const Rect& monitorGeometry
    );

    // Re-centers the (already-created) window on `monitorGeometry` -
    // see Launcher::Reposition() for why this is a separate, cheaper
    // call from Configure() rather than just calling Configure() again.
    void Reposition(
        const Rect& monitorGeometry
    );

    void Open();

    // Hides the window and saves content to disk.
    void Close();

    bool IsOpen() const;

    // True once there is any saved/typed text, or the window is
    // currently open - i.e. exactly what the bar's "[N]" indicator
    // means by "the notepad exists or is open".
    bool HasContent() const;

    ::Window WindowId() const;

    NotepadResult HandleKeyPress(
        const XKeyEvent& event
    );

    void HandleExpose();

    void Blink();

private:

    Rect ComputeGeometry(
        const Rect& monitorGeometry
    ) const;

    // Word boundary within m_lines[m_row], for Ctrl+Backspace /
    // Ctrl+Delete - clamped to the line itself (0 / line.size()), so
    // hitting either end just falls back to the plain BackSpace/Delete
    // line-join behaviour rather than needing its own cross-line case.
    std::size_t PreviousWordBoundary(
        std::size_t col
    ) const;

    std::size_t NextWordBoundary(
        std::size_t col
    ) const;

    void Redraw();

    void Load();
    void Save();

    std::string JoinedText() const;

private:

    XConnection& m_connection;

    // notepad.width/notepad.height, resolved once in Configure() and
    // reused by Reposition() so re-centering on a different monitor
    // never needs a Config reference of its own.
    float m_widthFraction  = 0.4f;
    float m_heightFraction = 0.5f;

    ::Window m_window = 0;
    GC m_gc = nullptr;
    Font m_font;
    XftDraw* m_xftDraw = nullptr;

    XIM m_xim = nullptr;
    XIC m_xic = nullptr;

    Rect m_geometry;
    bool m_open = false;
    bool m_loaded = false;

    std::vector<std::string> m_lines{std::string()};
    std::size_t m_row = 0;
    std::size_t m_col = 0;
    bool m_caretOn = true;

    std::string m_savePath;

    unsigned long m_backgroundPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_borderPixel = 0;

};

}
