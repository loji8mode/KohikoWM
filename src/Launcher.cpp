#include "Launcher.h"

#include "Config.h"
#include "XConnection.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace Kohiko
{

Launcher::Launcher(
    XConnection& connection)
    :
    m_connection(connection)
{
}

Launcher::~Launcher()
{
    Display* display = m_connection.GetDisplay();

    if (!display)
        return;

    if (m_font)
        XFreeFont(display, m_font);

    if (m_gc)
        XFreeGC(display, m_gc);

    if (m_window)
        XDestroyWindow(display, m_window);
}

void Launcher::Configure(
    const Config& config,
    const Rect& monitorGeometry)
{
    // "About 1/12 of the screen" read as *area*, held at a wide,
    // single-line-input aspect ratio (a literal 1/12-area square would
    // be far taller than a one-line box needs), then clamped to sane
    // pixel bounds so it stays sensible on very small or very large
    // monitors.
    constexpr float kAreaFraction = 1.0f / 12.0f;
    constexpr float kAspect = 6.0f; // width : height

    float targetArea = static_cast<float>(monitorGeometry.width) *
                        static_cast<float>(monitorGeometry.height) *
                        kAreaFraction;

    int height = static_cast<int>(std::sqrt(targetArea / kAspect));
    int width  = static_cast<int>(static_cast<float>(height) * kAspect);

    height = std::clamp(height, 48, 220);
    width  = std::clamp(width, 320, static_cast<int>(static_cast<float>(monitorGeometry.width) * 0.9f));

    m_geometry.width  = width;
    m_geometry.height = height;
    m_geometry.x = monitorGeometry.x + (monitorGeometry.width  - width)  / 2;
    m_geometry.y = monitorGeometry.y + (monitorGeometry.height - height) / 2;

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    // Reuses the bar's palette plus the same accent already used for a
    // focused window's border, so the launcher looks like part of
    // Kohiko rather than a bolted-on extra widget - no new config keys
    // needed for it to fit in.
    m_backgroundPixel  = std::strtoul(config.GetString("bar.background", "0x1e1e2e").c_str(), nullptr, 0);
    m_foregroundPixel  = std::strtoul(config.GetString("bar.foreground", "0xcdd6f4").c_str(), nullptr, 0);
    m_borderPixel      = std::strtoul(config.GetString("general.border_color_active", "0x89b4fa").c_str(), nullptr, 0);
    m_placeholderPixel = std::strtoul(config.GetString("general.border_color_inactive", "0x45475a").c_str(), nullptr, 0);

    if (m_window == 0)
    {
        XSetWindowAttributes attrs{};
        attrs.override_redirect = True; // this is *our* window - never redirect it back to ourselves as a MapRequest
        attrs.background_pixel = m_backgroundPixel;
        attrs.border_pixel = m_borderPixel;
        attrs.event_mask = ExposureMask | KeyPressMask;

        m_window = XCreateWindow(
            display,
            m_connection.Root(),
            m_geometry.x, m_geometry.y,
            static_cast<unsigned int>(m_geometry.width),
            static_cast<unsigned int>(m_geometry.height),
            2,
            DefaultDepth(display, screen),
            InputOutput,
            DefaultVisual(display, screen),
            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
            &attrs
        );

        m_gc = XCreateGC(display, m_window, 0, nullptr);

        m_font = XLoadQueryFont(display, "-*-fixed-medium-r-*-*-16-*-*-*-*-*-*-*");

        if (!m_font)
            m_font = XLoadQueryFont(display, "fixed");

        if (m_font)
            XSetFont(display, m_gc, m_font->fid);
    }
    else
    {
        XMoveResizeWindow(
            display,
            m_window,
            m_geometry.x, m_geometry.y,
            static_cast<unsigned int>(m_geometry.width),
            static_cast<unsigned int>(m_geometry.height));

        XSetWindowBackground(display, m_window, m_backgroundPixel);
        XSetWindowBorder(display, m_window, m_borderPixel);
    }
}

void Launcher::Open()
{
    if (m_window == 0)
        return;

    m_query.clear();
    m_cursor = 0;
    m_caretOn = true;
    m_open = true;

    m_connection.MapWindow(m_window);
    m_connection.Raise(m_window);

    Redraw();
}

void Launcher::Close()
{
    if (m_window == 0 || !m_open)
        return;

    m_open = false;
    m_connection.UnmapWindow(m_window);
}

bool Launcher::IsOpen() const
{
    return m_open;
}

::Window Launcher::WindowId() const
{
    return m_window;
}

const std::string& Launcher::Query() const
{
    return m_query;
}

LauncherResult Launcher::HandleKeyPress(
    const XKeyEvent& event)
{
    char buffer[32];
    KeySym keysym = NoSymbol;

    XKeyEvent mutableEvent = event; // XLookupString wants a non-const pointer
    int len = XLookupString(&mutableEvent, buffer, sizeof(buffer) - 1, &keysym, nullptr);

    if (len < 0)
        len = 0;

    switch (keysym)
    {
        case XK_Escape:
            return LauncherResult::Cancelled;

        case XK_Return:
        case XK_KP_Enter:
            return LauncherResult::Confirmed;

        case XK_BackSpace:
            if (m_cursor > 0)
            {
                m_query.erase(m_cursor - 1, 1);
                --m_cursor;
            }
            Redraw();
            return LauncherResult::Editing;

        case XK_Delete:
            if (m_cursor < m_query.size())
                m_query.erase(m_cursor, 1);
            Redraw();
            return LauncherResult::Editing;

        case XK_Left:
            if (m_cursor > 0)
                --m_cursor;
            Redraw();
            return LauncherResult::Editing;

        case XK_Right:
            if (m_cursor < m_query.size())
                ++m_cursor;
            Redraw();
            return LauncherResult::Editing;

        case XK_Home:
            m_cursor = 0;
            Redraw();
            return LauncherResult::Editing;

        case XK_End:
            m_cursor = m_query.size();
            Redraw();
            return LauncherResult::Editing;

        case XK_u:

            // Ctrl+U: clear the line - a small, well-known readline-ism
            // that's handy given there's no undo otherwise. Only fires
            // with Control actually held, so plain "u" still types.
            if (event.state & ControlMask)
            {
                m_query.clear();
                m_cursor = 0;
                Redraw();
                return LauncherResult::Editing;
            }
            break;

        default:
            break;
    }

    // Anything else that produced printable text - XLookupString
    // already resolves Shift/level via the active keyboard mapping.
    for (int i = 0; i < len; ++i)
    {
        unsigned char c = static_cast<unsigned char>(buffer[i]);

        if (c < 0x20 || c == 0x7f) // control character, e.g. an unhandled Ctrl+combo - not text
            continue;

        m_query.insert(m_query.begin() + static_cast<long>(m_cursor), static_cast<char>(c));
        ++m_cursor;
    }

    Redraw();
    return LauncherResult::Editing;
}

void Launcher::HandleExpose()
{
    Redraw();
}

void Launcher::Blink()
{
    if (!m_open)
        return;

    m_caretOn = !m_caretOn;
    Redraw();
}

void Launcher::Redraw()
{
    if (!m_open || m_window == 0)
        return;

    Display* display = m_connection.GetDisplay();

    XClearWindow(display, m_window);

    int padding = 16;
    int fontHeight = m_font ? (m_font->ascent + m_font->descent) : 14;
    int baseline = (m_geometry.height - fontHeight) / 2 + (m_font ? m_font->ascent : 10);

    if (m_query.empty() && !m_caretOn)
    {
        static const std::string placeholder = "Type a command and press Enter...";

        XSetForeground(display, m_gc, m_placeholderPixel);
        XDrawString(
            display, m_window, m_gc,
            padding, baseline,
            placeholder.c_str(), static_cast<int>(placeholder.size()));

        XFlush(display);
        return;
    }

    std::string visible = m_query;

    if (m_caretOn)
        visible.insert(visible.begin() + static_cast<long>(m_cursor), '|');

    XSetForeground(display, m_gc, m_foregroundPixel);
    XDrawString(
        display, m_window, m_gc,
        padding, baseline,
        visible.c_str(), static_cast<int>(visible.size()));

    XFlush(display);
}

}
