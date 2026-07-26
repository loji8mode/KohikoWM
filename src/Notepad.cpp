#include "Notepad.h"

#include "Config.h"
#include "XConnection.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace Kohiko
{

Notepad::Notepad(
    XConnection& connection)
    :
    m_connection(connection)
{
}

Notepad::~Notepad()
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

void Notepad::Configure(
    const Config& config,
    const Rect& monitorGeometry)
{
    float widthFraction  = config.GetPercent("notepad.width",  0.4f);
    float heightFraction = config.GetPercent("notepad.height", 0.5f);

    m_geometry.width  = static_cast<int>(static_cast<float>(monitorGeometry.width)  * widthFraction);
    m_geometry.height = static_cast<int>(static_cast<float>(monitorGeometry.height) * heightFraction);
    m_geometry.x = monitorGeometry.x + (monitorGeometry.width  - m_geometry.width)  / 2;
    m_geometry.y = monitorGeometry.y + (monitorGeometry.height - m_geometry.height) / 2;

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    // Same palette as Bar/Launcher, so all three read as one
    // consistent visual family instead of three separately-themed
    // widgets.
    m_backgroundPixel = std::strtoul(config.GetString("bar.background", "0x1e1e2e").c_str(), nullptr, 0);
    m_foregroundPixel = std::strtoul(config.GetString("bar.foreground", "0xcdd6f4").c_str(), nullptr, 0);
    m_borderPixel     = std::strtoul(config.GetString("general.border_color_active", "0x89b4fa").c_str(), nullptr, 0);

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

        m_font = XLoadQueryFont(display, "-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*");

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

    const char* home = std::getenv("HOME");
    m_savePath = (home ? std::string(home) : std::string(".")) + "/.config/kohiko/notepad.txt";

    // Eager, one-time load: HasContent() (and therefore the bar's
    // "[N]" indicator) needs to be right immediately at startup, not
    // only after the notepad has been opened once this session.
    if (!m_loaded)
    {
        Load();
        m_loaded = true;
    }
}

void Notepad::Open()
{
    if (m_window == 0)
        return;

    m_open = true;
    m_caretOn = true;

    if (m_lines.empty())
        m_lines.push_back(std::string());

    m_row = std::min(m_row, m_lines.size() - 1);
    m_col = std::min(m_col, m_lines[m_row].size());

    m_connection.MapWindow(m_window);
    m_connection.Raise(m_window);

    Redraw();
}

void Notepad::Close()
{
    if (m_window == 0 || !m_open)
        return;

    m_open = false;
    Save();

    m_connection.UnmapWindow(m_window);
}

bool Notepad::IsOpen() const
{
    return m_open;
}

bool Notepad::HasContent() const
{
    if (m_open)
        return true;

    return !JoinedText().empty();
}

::Window Notepad::WindowId() const
{
    return m_window;
}

NotepadResult Notepad::HandleKeyPress(
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
            return NotepadResult::Closed;

        case XK_Return:
        case XK_KP_Enter:
        {
            std::string tail = m_lines[m_row].substr(m_col);
            m_lines[m_row].erase(m_col);
            m_lines.insert(m_lines.begin() + static_cast<long>(m_row) + 1, tail);
            ++m_row;
            m_col = 0;
            Redraw();
            return NotepadResult::Editing;
        }

        case XK_BackSpace:

            if (m_col > 0)
            {
                m_lines[m_row].erase(m_col - 1, 1);
                --m_col;
            }
            else if (m_row > 0)
            {
                std::size_t previousLen = m_lines[m_row - 1].size();
                m_lines[m_row - 1] += m_lines[m_row];
                m_lines.erase(m_lines.begin() + static_cast<long>(m_row));
                --m_row;
                m_col = previousLen;
            }

            Redraw();
            return NotepadResult::Editing;

        case XK_Delete:

            if (m_col < m_lines[m_row].size())
            {
                m_lines[m_row].erase(m_col, 1);
            }
            else if (m_row + 1 < m_lines.size())
            {
                m_lines[m_row] += m_lines[m_row + 1];
                m_lines.erase(m_lines.begin() + static_cast<long>(m_row) + 1);
            }

            Redraw();
            return NotepadResult::Editing;

        case XK_Left:

            if (m_col > 0)
            {
                --m_col;
            }
            else if (m_row > 0)
            {
                --m_row;
                m_col = m_lines[m_row].size();
            }

            Redraw();
            return NotepadResult::Editing;

        case XK_Right:

            if (m_col < m_lines[m_row].size())
            {
                ++m_col;
            }
            else if (m_row + 1 < m_lines.size())
            {
                ++m_row;
                m_col = 0;
            }

            Redraw();
            return NotepadResult::Editing;

        case XK_Up:

            if (m_row > 0)
            {
                --m_row;
                m_col = std::min(m_col, m_lines[m_row].size());
            }

            Redraw();
            return NotepadResult::Editing;

        case XK_Down:

            if (m_row + 1 < m_lines.size())
            {
                ++m_row;
                m_col = std::min(m_col, m_lines[m_row].size());
            }

            Redraw();
            return NotepadResult::Editing;

        case XK_Home:
            m_col = 0;
            Redraw();
            return NotepadResult::Editing;

        case XK_End:
            m_col = m_lines[m_row].size();
            Redraw();
            return NotepadResult::Editing;

        default:
            break;
    }

    for (int i = 0; i < len; ++i)
    {
        unsigned char c = static_cast<unsigned char>(buffer[i]);

        if (c < 0x20 || c == 0x7f) // control character - not text
            continue;

        m_lines[m_row].insert(m_lines[m_row].begin() + static_cast<long>(m_col), static_cast<char>(c));
        ++m_col;
    }

    Redraw();
    return NotepadResult::Editing;
}

void Notepad::HandleExpose()
{
    Redraw();
}

void Notepad::Blink()
{
    if (!m_open)
        return;

    m_caretOn = !m_caretOn;
    Redraw();
}

void Notepad::Redraw()
{
    if (!m_open || m_window == 0)
        return;

    Display* display = m_connection.GetDisplay();

    XClearWindow(display, m_window);

    int padding = 12;
    int lineHeight = m_font ? (m_font->ascent + m_font->descent + 4) : 18;
    int baseline = padding + (m_font ? m_font->ascent : 12);

    XSetForeground(display, m_gc, m_foregroundPixel);

    std::size_t maxLines =
        static_cast<std::size_t>(std::max(1, (m_geometry.height - padding * 2) / lineHeight));

    // Keep-the-caret-on-screen scrolling: once there are more lines
    // than fit, slide the top of the view down just enough to keep
    // the current row visible. No smooth scrolling, no scrollbar -
    // this is a quick-notes box, not a full editor.
    std::size_t firstVisible = 0;

    if (m_row >= maxLines)
        firstVisible = m_row - maxLines + 1;

    for (std::size_t i = firstVisible; i < m_lines.size() && (i - firstVisible) < maxLines; ++i)
    {
        std::string text = m_lines[i];

        if (i == m_row && m_caretOn)
            text.insert(text.begin() + static_cast<long>(m_col), '|');

        if (text.empty())
            continue;

        XDrawString(
            display, m_window, m_gc,
            padding, baseline + static_cast<int>(i - firstVisible) * lineHeight,
            text.c_str(), static_cast<int>(text.size()));
    }

    XFlush(display);
}

std::string Notepad::JoinedText() const
{
    std::string joined;

    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        joined += m_lines[i];

        if (i + 1 < m_lines.size())
            joined += '\n';
    }

    return joined;
}

void Notepad::Load()
{
    m_lines = { std::string() };

    if (m_savePath.empty())
        return;

    std::ifstream in(m_savePath);

    if (!in)
        return;

    m_lines.clear();
    std::string line;

    while (std::getline(in, line))
        m_lines.push_back(line);

    if (m_lines.empty())
        m_lines.push_back(std::string());
}

void Notepad::Save()
{
    if (m_savePath.empty())
        return;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(m_savePath).parent_path(), ec);

    std::ofstream out(m_savePath, std::ios::trunc);

    if (!out)
        return;

    out << JoinedText();
}

}
