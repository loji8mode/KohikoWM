#include "Bar.h"

#include "Config.h"
#include "SystemTray.h"
#include "XConnection.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

namespace Kohiko
{

Bar::Bar(
    XConnection& connection)
    :
    m_connection(connection)
{
}

Bar::~Bar()
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

void Bar::Configure(
    const Config& config,
    const Rect& monitorGeometry)
{
    m_height = config.GetInt("general.bar_height", 26);

    m_geometry = monitorGeometry;
    m_geometry.height = m_height;

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    if (m_window == 0)
    {
        XSetWindowAttributes attrs{};
        attrs.override_redirect = True; // this is *our* window - never redirect it back to ourselves as a MapRequest
        attrs.background_pixel = BlackPixel(display, screen);
        attrs.event_mask = ExposureMask | ButtonPressMask;

        m_window = XCreateWindow(
            display,
            m_connection.Root(),
            m_geometry.x, m_geometry.y,
            static_cast<unsigned int>(m_geometry.width > 0 ? m_geometry.width : 1),
            static_cast<unsigned int>(m_height),
            0,
            DefaultDepth(display, screen),
            InputOutput,
            DefaultVisual(display, screen),
            CWOverrideRedirect | CWBackPixel | CWEventMask,
            &attrs
        );

        m_gc = XCreateGC(display, m_window, 0, nullptr);

        m_font.Load(
            display,
            screen,
            config.GetString("general.font", "monospace:pixelsize=14"));

        m_xftDraw = XftDrawCreate(
            display,
            m_window,
            DefaultVisual(display, screen),
            DefaultColormap(display, screen));
    }
    else
    {
        XMoveResizeWindow(
            display,
            m_window,
            m_geometry.x, m_geometry.y,
            static_cast<unsigned int>(m_geometry.width > 0 ? m_geometry.width : 1),
            static_cast<unsigned int>(m_height)
        );
    }

    m_backgroundPixel = std::strtoul(config.GetString("bar.background", "0x1e1e2e").c_str(), nullptr, 0);
    m_foregroundPixel = std::strtoul(config.GetString("bar.foreground", "0xcdd6f4").c_str(), nullptr, 0);
    m_activePixel     = std::strtoul(config.GetString("bar.active",     "0x89b4fa").c_str(), nullptr, 0);

    XSetWindowBackground(display, m_window, m_backgroundPixel);
}

void Bar::Show()
{
    if (m_window == 0)
        return;

    m_visible = true;

    m_connection.MapWindow(m_window);
    m_connection.Raise(m_window);
}

void Bar::Hide()
{
    if (m_window == 0)
        return;

    m_visible = false;

    m_connection.UnmapWindow(m_window);
}

bool Bar::IsVisible() const
{
    return m_visible;
}

::Window Bar::WindowId() const
{
    return m_window;
}

void Bar::SetWorkspaces(
    int count,
    int current)
{
    m_workspaceCount = count;
    m_currentWorkspace = current;
}

void Bar::SetTitle(
    const std::string& title)
{
    m_title = title;
}

void Bar::ShowNotification(
    const std::string& text,
    int durationMs)
{
    m_notificationText = text;
    m_notificationExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
}

void Bar::SetScratchpadActive(
    bool active)
{
    m_scratchpadActive = active;
}

void Bar::SetNotepadActive(
    bool active)
{
    m_notepadActive = active;
}

void Bar::AttachSystemTray(
    SystemTray* tray)
{
    m_tray = tray;
}

int Bar::Height() const
{
    return m_height;
}

const Rect& Bar::PowerButtonRect() const
{
    return m_powerButtonRect;
}

const Rect& Bar::Geometry() const
{
    return m_geometry;
}

void Bar::Redraw()
{
    if (!m_visible || m_window == 0)
        return;

    Display* display = m_connection.GetDisplay();

    XClearWindow(display, m_window);

    int baseline = m_height / 2 + 5;
    int x = 10;

    for (int i = 1; i <= m_workspaceCount; ++i)
    {
        std::string label = std::to_string(i);
        unsigned long color = (i == m_currentWorkspace) ? m_activePixel : m_foregroundPixel;

        DrawText(x, baseline, label, color);
        x += 22;
    }

    if (m_scratchpadActive)
    {
        DrawText(x, baseline, "[S]", m_activePixel);
        x += 34;
    }

    if (m_notepadActive)
    {
        DrawText(x, baseline, "[N]", m_activePixel);
        x += 34;
    }

    if (!m_notificationText.empty() && std::chrono::steady_clock::now() >= m_notificationExpiry)
        m_notificationText.clear();

    if (!m_notificationText.empty())
        DrawText(x + 16, baseline, m_notificationText, m_activePixel);
    else if (!m_title.empty())
        DrawText(x + 16, baseline, m_title, m_foregroundPixel);

    int trayWidth = 0;

    if (m_tray)
    {
        m_tray->Reposition(m_geometry.width, m_height);
        trayWidth = m_tray->Width();

        if (trayWidth > 0)
            trayWidth += 12;
    }

    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);

    char clockText[16];
    std::strftime(clockText, sizeof(clockText), "%H:%M:%S", &local);

    int clockLen = static_cast<int>(std::strlen(clockText));
    int clockWidth = m_font.TextWidth(clockText);

    if (clockWidth == 0)
        clockWidth = clockLen * 8;

    // The power button - see PowerButtonRect()/WindowManager's own
    // ButtonPress routing for what opens PowerMenu. Plain bracketed
    // text rather than a glyph/icon, same reasoning as the "[S]"/"[N]"
    // indicators above: not every installed font covers a power
    // symbol, and a missing one would just draw a tofu box.
    static const std::string kPowerLabel = "[Power]";
    int powerWidth = m_font.TextWidth(kPowerLabel);

    if (powerWidth == 0)
        powerWidth = static_cast<int>(kPowerLabel.size()) * 8;

    int powerX = m_geometry.width - clockWidth - 12 - trayWidth - powerWidth - 16;

    DrawText(powerX, baseline, kPowerLabel, m_foregroundPixel);

    m_powerButtonRect = Rect{powerX - 6, 0, powerWidth + 12, m_height};

    DrawText(m_geometry.width - clockWidth - 12 - trayWidth, baseline, clockText, m_foregroundPixel);

    XFlush(display);
}

void Bar::DrawText(
    int x,
    int baseline,
    const std::string& text,
    unsigned long color)
{
    if (m_window == 0 || text.empty() || !m_xftDraw)
        return;

    Display* display = m_connection.GetDisplay();

    TextColor textColor(
        display,
        DefaultVisual(display, m_connection.Screen()),
        DefaultColormap(display, m_connection.Screen()),
        color);

    m_font.DrawString(m_xftDraw, x, baseline, text, textColor.Get());
}

}
