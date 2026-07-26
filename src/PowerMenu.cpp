#include "PowerMenu.h"

#include "Config.h"
#include "Process.h"
#include "XConnection.h"

#include <X11/keysym.h>

#include <cstdlib>

namespace Kohiko
{

PowerMenu::PowerMenu(
    XConnection& connection)
    :
    m_connection(connection)
{
}

PowerMenu::~PowerMenu()
{
    Display* display = m_connection.GetDisplay();

    if (!display)
        return;

    if (m_xftDraw)
        XftDrawDestroy(m_xftDraw);

    m_font.Unload();

    if (m_gc)
        XFreeGC(display, m_gc);

    if (m_window)
        XDestroyWindow(display, m_window);
}

void PowerMenu::Configure(
    const Config& config)
{
    m_rows.clear();
    m_rows.push_back({"Shutdown", config.GetString("power.shutdown_command", "systemctl poweroff")});
    m_rows.push_back({"Restart",  config.GetString("power.restart_command",  "systemctl reboot")});
    m_rows.push_back({"Suspend",  config.GetString("power.suspend_command",  "systemctl suspend")});

    // Same palette as Bar/Launcher/Notepad, so this reads as one
    // consistent visual family rather than a separately-themed popup.
    m_backgroundPixel = std::strtoul(config.GetString("bar.background", "0x1e1e2e").c_str(), nullptr, 0);
    m_foregroundPixel = std::strtoul(config.GetString("bar.foreground", "0xcdd6f4").c_str(), nullptr, 0);
    m_borderPixel     = std::strtoul(config.GetString("general.border_color_active", "0x89b4fa").c_str(), nullptr, 0);

    if (m_window != 0)
    {
        Display* display = m_connection.GetDisplay();
        XSetWindowBackground(display, m_window, m_backgroundPixel);
        XSetWindowBorder(display, m_window, m_borderPixel);
        return;
    }

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    m_font.Load(
        display,
        screen,
        config.GetString("general.font", "monospace:pixelsize=14"));

    m_rowHeight = m_font.Height() + 14;

    // Real geometry gets set on every Open() (it depends on the click
    // position) - this initial size just needs to be non-zero for
    // XCreateWindow.
    m_geometry.width = 140;
    m_geometry.height = m_rowHeight * static_cast<int>(m_rows.size());

    XSetWindowAttributes attrs{};
    attrs.override_redirect = True; // this is *our* window - never redirect it back to ourselves as a MapRequest
    attrs.background_pixel = m_backgroundPixel;
    attrs.border_pixel = m_borderPixel;
    attrs.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;

    m_window = XCreateWindow(
        display,
        m_connection.Root(),
        -1, -1,
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

    m_xftDraw = XftDrawCreate(
        display,
        m_window,
        DefaultVisual(display, screen),
        DefaultColormap(display, screen));
}

void PowerMenu::Open(
    const Point& anchor,
    const Rect& monitorGeometry)
{
    if (m_window == 0)
        return;

    m_geometry.width  = 140;
    m_geometry.height = m_rowHeight * static_cast<int>(m_rows.size());
    m_geometry.x = anchor.x;
    m_geometry.y = anchor.y;

    // Same "never partly off-screen" guarantee a floating window
    // gets, just applied to a popup instead - matters most for a bar
    // sitting flush against the right edge, where a naive anchor
    // would otherwise hang the menu half off the monitor.
    m_geometry = m_geometry.ClampedTo(monitorGeometry, 2);

    m_connection.MoveResizeWindow(m_window, m_geometry);
    m_connection.MapWindow(m_window);
    m_connection.Raise(m_window);

    m_open = true;

    Redraw();
}

void PowerMenu::Close()
{
    if (m_window == 0 || !m_open)
        return;

    m_open = false;
    m_connection.UnmapWindow(m_window);
}

bool PowerMenu::IsOpen() const
{
    return m_open;
}

::Window PowerMenu::WindowId() const
{
    return m_window;
}

void PowerMenu::HandleExpose()
{
    Redraw();
}

void PowerMenu::SetSuspendCallback(
    std::function<void()> callback)
{
    m_suspendCallback = std::move(callback);
}

void PowerMenu::HandleButtonPress(
    const XButtonEvent& event)
{
    int row = event.y / m_rowHeight;

    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return;

    // Row 2 is always Suspend - see Configure(), which pushes the
    // three rows in this fixed Shutdown/Restart/Suspend order and
    // never reorders them (the spec calls for exactly these three,
    // nothing else, in this order).
    if (row == 2 && m_suspendCallback)
        m_suspendCallback();

    Process::Spawn(m_rows[static_cast<std::size_t>(row)].command, m_connection.DisplayName());
}

bool PowerMenu::HandleKeyPress(
    const XKeyEvent& event)
{
    KeySym keysym = XLookupKeysym(const_cast<XKeyEvent*>(&event), 0);

    return keysym == XK_Escape;
}

void PowerMenu::Redraw()
{
    if (!m_open || m_window == 0 || !m_xftDraw)
        return;

    Display* display = m_connection.GetDisplay();

    XClearWindow(display, m_window);

    XSetForeground(display, m_gc, m_borderPixel);

    for (std::size_t i = 1; i < m_rows.size(); ++i)
    {
        int y = static_cast<int>(i) * m_rowHeight;
        XDrawLine(display, m_window, m_gc, 0, y, m_geometry.width, y);
    }

    TextColor textColor(
        display,
        DefaultVisual(display, m_connection.Screen()),
        DefaultColormap(display, m_connection.Screen()),
        m_foregroundPixel);

    for (std::size_t i = 0; i < m_rows.size(); ++i)
    {
        int baseline = static_cast<int>(i) * m_rowHeight + m_rowHeight / 2 + m_font.Ascent() / 2;

        m_font.DrawString(m_xftDraw, 14, baseline, m_rows[i].label, textColor.Get());
    }

    XFlush(display);
}

}
