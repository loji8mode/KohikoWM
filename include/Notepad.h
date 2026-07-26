#pragma once

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
// the same way as Bar/Launcher (plain core Xlib text, no toolkit, no
// extra process, no dependency beyond libX11).
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

    void Redraw();

    void Load();
    void Save();

    std::string JoinedText() const;

private:

    XConnection& m_connection;

    ::Window m_window = 0;
    GC m_gc = nullptr;
    XFontSet m_fontSet = nullptr;

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
