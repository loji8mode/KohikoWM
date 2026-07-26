#pragma once

#include "Types.h"

#include <X11/Xlib.h>

#include <string>

namespace Kohiko
{

class XConnection;
class Config;

// What happened to one XKeyEvent fed into a currently-open Launcher.
enum class LauncherResult
{
    Editing,    // consumed - the query text changed, just redraw
    Confirmed,  // Enter - Query() is what should be run, then close
    Cancelled   // Escape - close without running anything
};

// Kohiko's native Super+D launcher, replacing the old
// `exec.launcher=dmenu_run` default: a small centered input box that
// "instantly opens, cursor already in the input field, type, Enter,
// program starts, launcher closes" - no results list, no external
// process, no toolkit. Rendered the same way as Bar (plain core-Xlib
// text) for the same "own bar, no toolkit" reason.
//
// Deliberately never uses an active keyboard grab (XGrabKeyboard) -
// WindowManager just gives it ordinary X input focus and, while it's
// open, routes every KeyPress to it directly (see
// WindowManager::HandleModalKeyPress). That keeps this feature from
// ever being the reason Kohiko's global hotkeys stop responding - the
// exact failure mode the Super+Q hardening in WindowManager::Manage()
// guards against.
class Launcher
{
public:

    explicit Launcher(
        XConnection& connection
    );

    ~Launcher();

    void Configure(
        const Config& config,
        const Rect& monitorGeometry
    );

    // Opens the box empty, mapped, raised - WindowManager still has to
    // give it real input focus afterwards (it owns focus policy).
    void Open();

    // Unmaps the box. Safe to call even when already closed.
    void Close();

    bool IsOpen() const;

    ::Window WindowId() const;

    // The currently typed line - only meaningful while IsOpen().
    const std::string& Query() const;

    LauncherResult HandleKeyPress(
        const XKeyEvent& event
    );

    void HandleExpose();

    // Called on Kohiko's ~1s idle heartbeat while open, to blink the
    // caret - cheap enough not to need a dedicated timer.
    void Blink();

private:

    void Redraw();

private:

    XConnection& m_connection;

    ::Window m_window = 0;
    GC m_gc = nullptr;
    XFontStruct* m_font = nullptr;

    Rect m_geometry;
    bool m_open = false;

    std::string m_query;
    std::size_t m_cursor = 0;
    bool m_caretOn = true;

    unsigned long m_backgroundPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_borderPixel = 0;
    unsigned long m_placeholderPixel = 0;

};

}
