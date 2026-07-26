#include "Launcher.h"

#include "Config.h"
#include "Utils.h"
#include "XConnection.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <Imlib2.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "AppRatings.h"

#include <filesystem>
#include <fstream>

namespace Kohiko
{
    namespace
{

    int GetPopularityScore(
    const LauncherEntry& entry)
{
    for (const auto& app : GlobalPopularity)
    {
        bool desktopMatch =
            !app.desktopId.empty() &&
            entry.desktopId == app.desktopId;

        bool execMatch =
            !app.execContains.empty() &&
            entry.exec.find(app.execContains)
                != std::string::npos;

        if (desktopMatch || execMatch)
        {
            return app.popularity;
        }
    }

    for (const auto& app : PenaltyPrograms)
    {
        bool desktopMatch =
            !app.desktopId.empty() &&
            entry.desktopId == app.desktopId;

        bool execMatch =
            !app.execContains.empty() &&
            entry.exec.find(app.execContains)
                != std::string::npos;

        if (desktopMatch || execMatch)
        {
            return app.popularity;
        }
    }

    return 0;
}

static const char* kHistoryFile =
    "/tmp/kohiko_launcher_history";

static bool IsSubsequence(
    const std::string& query,
    const std::string& text)
{
    std::size_t q = 0;

    for (char c : text)
    {
        if (q < query.size() &&
            std::tolower(c) ==
            std::tolower(query[q]))
        {
            ++q;
        }
    }

    return q == query.size();
}

int CalculateScore(
    const std::string& query,
    const std::string& name,
    int launchCount,
    int popularity)
{
    std::string lowerName = name;
    std::string lowerQuery = query;

    // ::tolower takes an int that must be representable as unsigned
    // char (or EOF) - passing a plain (signed) char straight through
    // is undefined behaviour for any UTF-8 continuation/lead byte
    // (anything >= 0x80), which any non-ASCII app name or query will
    // contain. Casting through unsigned char first keeps ASCII
    // lowercasing correct and leaves multi-byte bytes untouched
    // instead of invoking UB.
    auto toLowerByte = [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    };

    std::transform(
        lowerName.begin(),
        lowerName.end(),
        lowerName.begin(),
        toLowerByte);

    std::transform(
        lowerQuery.begin(),
        lowerQuery.end(),
        lowerQuery.begin(),
        toLowerByte);

    if (query.empty())
    {
        return popularity +
               launchCount * 50 -
               static_cast<int>(lowerName.length());
    }

    int score = 0;

    if (lowerName == lowerQuery)
    {
        score += 10000;
    }
    else if (lowerName.starts_with(lowerQuery))
    {
        score += 5000;
    }
    else
{
    auto pos = lowerName.find(lowerQuery);

    if (pos != std::string::npos)
    {
        score += 1000 - static_cast<int>(pos);
    }
    else if (IsSubsequence(
                 lowerQuery,
                 lowerName))
    {
        score += 2000;
    }
    else
    {
        return -1;
    }
}

    score += popularity;
    score += launchCount * 50;

    // коротші назви мають невелику перевагу
    score -= static_cast<int>(lowerName.length());

    return score;
}

}

Launcher::Launcher(
    XConnection& connection)
    :
    m_connection(connection)
{
    

    static bool gtkInitialized = false;

    if (!gtkInitialized)
    {
        gtk_init(nullptr, nullptr);
        gtkInitialized = true;
        LoadLaunchHistory();
    }
}

Launcher::~Launcher()
{
    Display* display = m_connection.GetDisplay();

    if (!display)
        return;

    for (auto& [path, pixmap] : m_iconCache)
    {
    if (pixmap)
        XFreePixmap(display, pixmap);
    }

    if (m_folderIconPixmap)
        XFreePixmap(display, m_folderIconPixmap);

    if (m_fileIconPixmap)
        XFreePixmap(display, m_fileIconPixmap);

    if (m_folderIconMask)
        XFreePixmap(display, m_folderIconMask);

    if (m_fileIconMask)
        XFreePixmap(display, m_fileIconMask);

    if (m_xic)
        XDestroyIC(m_xic);

    if (m_xim)
        XCloseIM(m_xim);

    if (m_fontSet)
        XFreeFontSet(display, m_fontSet);

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
    constexpr float kAspect = 1.5f; // width : height

    float targetArea = static_cast<float>(monitorGeometry.width) *
                        static_cast<float>(monitorGeometry.height) *
                        kAreaFraction;

    int height = static_cast<int>(std::sqrt(targetArea / kAspect));
    int width  = static_cast<int>(static_cast<float>(height) * kAspect);

    height = std::clamp(height, 180, 350);
    width  = std::clamp(width, 320, static_cast<int>(static_cast<float>(monitorGeometry.width) * 0.9f));

    m_geometry.width  = width;
    m_geometry.height = 300;
    m_geometry.x = monitorGeometry.x + (monitorGeometry.width  - width)  / 2;
    m_geometry.y = monitorGeometry.y + (monitorGeometry.height - height) / 2.5;

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
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

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

        char** missing = nullptr;
int nmissing = 0;
char* def = nullptr;

// XCreateFontSet takes classic comma-separated XLFD base font
// names, not fontconfig pattern syntax - "monospace:size=13" is
// fontconfig-only syntax and never matches an XLFD font, which is
// why m_fontSet stayed null and no text ever drew. "fixed" is the
// XLFD alias every X11 install ships (it's xterm's own default
// font), so it's included in both lists as a guaranteed-to-match
// fallback.
m_fontSet = XCreateFontSet(
    display,
    "-*-fixed-medium-r-normal--13-*-*-*-*-*-*-*,-*-*-medium-r-normal--13-*-*-*-*-*-*-*,fixed",
    &missing,
    &nmissing,
    &def);

if (!m_fontSet)
{
    m_fontSet = XCreateFontSet(
        display,
        "-*-helvetica-medium-r-normal--12-*-*-*-*-*-*-*,-*-*-medium-r-normal--12-*-*-*-*-*-*-*,fixed",
        &missing,
        &nmissing,
        &def);
}

        // Xutf8LookupString (used in HandleKeyPress below) needs an
        // actual input method + input context - without this m_xic
        // stays null and every keypress would either crash or have to
        // be special-cased, and dead-key/compose sequences for
        // non-ASCII input wouldn't work at all.
        m_xim = XOpenIM(display, nullptr, nullptr, nullptr);

        if (m_xim)
        {
            m_xic = XCreateIC(
                m_xim,
                XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                XNClientWindow, m_window,
                XNFocusWindow, m_window,
                nullptr);
        }

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
    if (!m_entriesLoaded)
{
    LoadDesktopEntries();
    BuildFileIndex();
}
}

void Launcher::Open()
{
    if (m_window == 0)
        return;

    m_query.clear();
    m_selectedIndex = 0;
    m_scrollOffset = 0;

    UpdateMatches();
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

std::string Launcher::SelectedCommand()
{
    if (m_matches.empty())
        return m_query;

    if (m_selectedIndex >= m_matches.size())
        return "";

    const auto& match =
        m_matches[m_selectedIndex];

    if (match.type == MatchType::File)
    {
        return "";
    }

    if (match.index >= m_entries.size())
        return "";

    const auto& app =
        m_entries[match.index];

    m_launchCounts[app.name]++;

    SaveLaunchHistory();

    return app.exec;
}

bool Launcher::SelectedIsFile() const
{
    if (m_matches.empty())
        return false;

    return
        m_matches[m_selectedIndex].type ==
        MatchType::File;
}

std::string Launcher::SelectedPath()
{
    if (m_matches.empty())
        return "";

    const auto& match =
        m_matches[m_selectedIndex];

    if (match.type != MatchType::File)
        return "";

    return m_files[match.index].path;
}

LauncherResult Launcher::HandleKeyPress(
    const XKeyEvent& event)
{
    char buffer[32];
    KeySym keysym = NoSymbol;

    XKeyEvent mutableEvent = event; // Xutf8LookupString wants a non-const pointer
    Status status;

    int len;

    if (m_xic)
    {
        len = Xutf8LookupString(
            m_xic,
            &mutableEvent,
            buffer,
            sizeof(buffer) - 1,
            &keysym,
            &status);
    }
    else
    {
        // No input method could be opened (e.g. none configured on
        // this system) - fall back to plain, non-UTF-8 lookup rather
        // than passing a null XIC to Xutf8LookupString.
        len = XLookupString(
            &mutableEvent,
            buffer,
            sizeof(buffer) - 1,
            &keysym,
            nullptr);
    }

    if (len < 0)
        len = 0;

    switch (keysym)
    {   
        case XK_Up:
{
    if (!m_matches.empty() &&
        m_selectedIndex > 0)
    {
        --m_selectedIndex;

        if (m_selectedIndex <
            m_scrollOffset)
        {
            m_scrollOffset =
                m_selectedIndex;
        }
    }

    Redraw();
    return LauncherResult::Editing;
}

case XK_Down:
{
    if (!m_matches.empty() &&
        m_selectedIndex + 1 < m_matches.size())
    {
        ++m_selectedIndex;

        if (m_selectedIndex >=
            m_scrollOffset +
            static_cast<std::size_t>(VisibleRows()))
        {
            ++m_scrollOffset;
        }

    }

    Redraw();
    return LauncherResult::Editing;
}

        case XK_Escape:
            return LauncherResult::Cancelled;

        case XK_Return:
        case XK_KP_Enter:
            return LauncherResult::Confirmed;

        case XK_BackSpace:
            if (m_cursor > 0)
            {
                // Erase the whole UTF-8 codepoint before the cursor,
                // not just its last byte - otherwise a single
                // backspace on a multi-byte character (e.g. Cyrillic,
                // accented Latin) leaves the string as invalid UTF-8
                // and takes 2-4 presses to actually remove.
                std::size_t start = Utils::Utf8PrevBoundary(m_query, m_cursor);
                m_query.erase(start, m_cursor - start);
                m_cursor = start;
            }
            UpdateMatches();
            Redraw();
            return LauncherResult::Editing;

        case XK_Delete:
            if (m_cursor < m_query.size())
            {
                std::size_t end = Utils::Utf8NextBoundary(m_query, m_cursor);
                m_query.erase(m_cursor, end - m_cursor);
            }
            UpdateMatches();
            Redraw();
            return LauncherResult::Editing;

        case XK_Left:
            if (m_cursor > 0)
                m_cursor = Utils::Utf8PrevBoundary(m_query, m_cursor);
            Redraw();
            return LauncherResult::Editing;

        case XK_Right:
            if (m_cursor < m_query.size())
                m_cursor = Utils::Utf8NextBoundary(m_query, m_cursor);
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

    // Anything else that produced printable text - Xutf8LookupString
    // already resolves Shift/level via the active keyboard mapping.
    for (int i = 0; i < len; ++i)
    {
        unsigned char c = static_cast<unsigned char>(buffer[i]);

        if (c < 0x20 || c == 0x7f) // control character, e.g. an unhandled Ctrl+combo - not text
            continue;

        m_query.insert(m_query.begin() + static_cast<long>(m_cursor), static_cast<char>(c));
        ++m_cursor;
    }
    UpdateMatches();
    Redraw();
    return LauncherResult::Editing;
}

void Launcher::HandleExpose()
{
    Redraw();
}

void Launcher::HandleButtonPress(
    const XButtonEvent& event)
{
    if (event.button == Button4)
    {
        if (m_scrollOffset > 0)
        {
            --m_scrollOffset;
            Redraw();
        }
    }
    else if (event.button == Button5)
    {
        std::size_t maxOffset = 0;

        if (m_matches.size() >
            static_cast<std::size_t>(VisibleRows()))
        {
            maxOffset =
                m_matches.size() -
                VisibleRows();
        }

        if (m_scrollOffset < maxOffset)
        {
            ++m_scrollOffset;
            Redraw();
        }
    }
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
    int fontHeight = 18;
    int baseline = padding + fontHeight;


    if (m_query.empty())
{
    static const std::string placeholder =
        "Type a command and press Enter...";

    XSetForeground(
        display,
        m_gc,
        m_placeholderPixel);

    // XCreateFontSet can legitimately return null (e.g. no XLFD font
    // matches the requested charsets on this X server) - Xutf8DrawString
    // requires a real fontset, so skip drawing rather than passing it null.
    if (m_fontSet)
    {
        Xutf8DrawString(
            display,
            m_window,
            m_fontSet,
            m_gc,
            padding,
            baseline,
            placeholder.c_str(),
            static_cast<int>(placeholder.size()));
    }
}
else
{
    std::string visible = m_query;

    if (m_caretOn)
        visible.insert(
            visible.begin() +
            static_cast<long>(m_cursor),
            '|');

    XSetForeground(
        display,
        m_gc,
        m_foregroundPixel);

    if (m_fontSet)
    {
        Xutf8DrawString(
            display,
            m_window,
            m_fontSet,
            m_gc,
            padding,
            baseline,
            visible.c_str(),
            static_cast<int>(visible.size()));
    }
}

    std::string visible = m_query;

    if (m_caretOn)
        visible.insert(visible.begin() + static_cast<long>(m_cursor), '|');

    XSetForeground(display, m_gc, m_foregroundPixel);

    if (m_fontSet)
    {
        Xutf8DrawString(
            display, m_window, m_fontSet, m_gc,
            padding, baseline,
            visible.c_str(), static_cast<int>(visible.size()));
    }

        XSetForeground(
    display,
    m_gc,
    m_placeholderPixel);

XDrawLine(
    display,
    m_window,
    m_gc,
    padding,
    baseline + 15,
    m_geometry.width - padding,
    baseline + 15);

    int y = baseline + 40;
    int lineHeight = 20;
    int bottomPadding = -100;

int visibleRows =
    (m_geometry.height - y - bottomPadding)
    / lineHeight;

    std::size_t lastVisibleIndex =
    std::min(
        m_matches.size(),
        m_scrollOffset +
        static_cast<std::size_t>(visibleRows));

for (std::size_t i = m_scrollOffset;
     i < lastVisibleIndex;
     ++i)
{
    int drawY =
    y +
    static_cast<int>(i - m_scrollOffset)
    * lineHeight;
    if (i == m_selectedIndex)
{
    XSetForeground(display, m_gc, m_borderPixel);

    XFillRectangle(
        display,
        m_window,
        m_gc,
        8,
        drawY - 14,
        m_geometry.width - 16,
        20);

    XSetForeground(
        display,
        m_gc,
        m_backgroundPixel);
}
else
{
    XSetForeground(display, m_gc, m_foregroundPixel);
}

    

    const auto& match =
    m_matches[i];

    std::string label;

if (match.type ==
    MatchType::Application)
{
    label = m_entries[match.index].name;
}
else
{
    label =
        m_files[match.index].name;
}

    if (match.type ==
    MatchType::Application)
{
    const auto& app =
        m_entries[match.index];

    if (app.iconPixmap)
    {
        int iconX = 12;
        int iconY = drawY - 14;

        if (app.iconMask)
        {
            XSetClipMask(display, m_gc, app.iconMask);
            XSetClipOrigin(display, m_gc, iconX, iconY);
        }

        XCopyArea(
            display,
            app.iconPixmap,
            m_window,
            m_gc,
            0,
            0,
            20,
            20,
            iconX,
            iconY);

        if (app.iconMask)
            XSetClipMask(display, m_gc, None);
    }
}
else
{
    Pixmap fileTypeIcon =
        m_files[match.index].isDirectory ?
        m_folderIconPixmap :
        m_fileIconPixmap;

    Pixmap fileTypeMask =
        m_files[match.index].isDirectory ?
        m_folderIconMask :
        m_fileIconMask;

    if (fileTypeIcon)
    {
        int iconX = 12;
        int iconY = drawY - 14;

        if (fileTypeMask)
        {
            XSetClipMask(display, m_gc, fileTypeMask);
            XSetClipOrigin(display, m_gc, iconX, iconY);
        }

        XCopyArea(
            display,
            fileTypeIcon,
            m_window,
            m_gc,
            0,
            0,
            20,
            20,
            iconX,
            iconY);

        if (fileTypeMask)
            XSetClipMask(display, m_gc, None);
    }
}

    if (m_fontSet)
    {
        Xutf8DrawString(
            display,
            m_window,
            m_fontSet,
            m_gc,
            padding + 28,
            drawY,
            label.c_str(),
            static_cast<int>(label.size()));
    }
}
XFlush(display);
}

void Launcher::LoadDesktopEntries()
{
    namespace fs = std::filesystem;

    // ReloadDesktopEntries() throws away every LauncherEntry below and
    // rebuilds the list from scratch - free the icon resources the
    // old entries were holding onto first, or every reload would leak
    // one Pixmap and one mask per application into the X server for
    // as long as Kohiko keeps running.
    if (Display* display = m_connection.GetDisplay())
    {
        for (const auto& app : m_entries)
        {
            if (app.iconPixmap)
                XFreePixmap(display, app.iconPixmap);

            if (app.iconMask)
                XFreePixmap(display, app.iconMask);
        }
    }

    m_entries.clear();

    const std::vector<std::string> dirs =
    {
        "/usr/share/applications"
    };

    for (const auto& dir : dirs)
    {
        if (!fs::exists(dir))
            continue;

        for (const auto& entry : fs::directory_iterator(dir))
        {
            if (entry.path().extension() != ".desktop")
                continue;

            LauncherEntry app;
            app.desktopId =
    entry.path().stem().string();
    

std::ifstream file(entry.path());

std::string line;

bool inDesktopEntry = false;

while (std::getline(file, line))
{
    if (line == "[Desktop Entry]")
    {
        inDesktopEntry = true;
        continue;
    }

    if (!line.empty() &&
        line[0] == '[')
    {
        inDesktopEntry = false;
        continue;
    }

    if (!inDesktopEntry)
    {
        continue;
    }

    if (line.starts_with("Name="))
    {
        if (line.size() > 5)
            app.name = line.substr(5);
    }
    else if (line.starts_with("Icon="))
    {
    if (line.size() > 5)
        app.icon = line.substr(5);
    }
    else if (line.starts_with("Exec="))
    {
        if (line.size() > 5)
            app.exec = line.substr(5);

        const char* fields[] =
        {
            "%u",
            "%U",
            "%f",
            "%F",
            "%i",
            "%c",
            "%k"
        };

        for (const char* field : fields)
        {
            std::size_t pos = app.exec.find(field);

            if (pos != std::string::npos)
                app.exec.erase(pos);
        }

        while (!app.exec.empty() &&
               std::isspace(static_cast<unsigned char>(app.exec.back())))
        {
            app.exec.pop_back();
        }
    }
}
if (!app.name.empty())
{
    app.iconPath =
        FindIconPath(app.icon);

    if (!app.iconPath.empty())
{
    app.iconPixmap =
        LoadIcon(app.iconPath, app.iconMask);
}

    m_entries.push_back(app);
}
        }
    }
    std::printf(
    "Loaded %zu applications\n",
    m_entries.size());
    for (const auto& app : m_entries)
{
    if (!app.icon.empty())
    {
        std::printf(
            "%s -> %s\n",
            app.name.c_str(),
            app.icon.c_str());
    }
}
m_entriesLoaded = true;
}

std::string Launcher::FindIconPath(
    const std::string& iconName)
{
    if (iconName.empty())
        return "";

    if (iconName.starts_with("/"))
        return iconName;

    GtkIconTheme* theme =
        gtk_icon_theme_get_default();

    GtkIconInfo* info =
        gtk_icon_theme_lookup_icon(
            theme,
            iconName.c_str(),
            48,
            GTK_ICON_LOOKUP_FORCE_SIZE);

    if (!info)
        return "";

    const char* filename =
        gtk_icon_info_get_filename(info);

    std::string result;

    if (filename)
        result = filename;

    g_object_unref(info);

    return result;
}

Pixmap Launcher::LoadIcon(
    const std::string& path,
    Pixmap& maskOut)
{
    maskOut = 0;

    if (path.empty())
        return 0;

    Display* display =
        m_connection.GetDisplay();

    Visual* visual =
        DefaultVisual(
            display,
            m_connection.Screen());

    Colormap colormap =
        DefaultColormap(
            display,
            m_connection.Screen());

    imlib_context_set_display(display);
    imlib_context_set_visual(visual);
    imlib_context_set_colormap(colormap);
    imlib_context_set_drawable(m_window);

    Imlib_Image image =
        imlib_load_image(path.c_str());

    if (!image)
        return 0;

    imlib_context_set_image(image);

    // Most icon themes hand out PNGs with an alpha channel (rounded
    // corners, transparent padding around a smaller glyph, etc).
    // Rendering straight onto a freshly created Pixmap - as this used
    // to do - ignores that alpha entirely and paints over whatever the
    // Pixmap's memory happened to already contain, which on every
    // system this was tested on reads back as solid black: every icon
    // showed up with a black square behind it instead of blending into
    // the Launcher's actual (dark) background.
    //
    // Asking Imlib2 for a 1-bit shape mask alongside the pixmap -
    // instead of just a flat-rendered pixmap - lets Redraw() clip its
    // XCopyArea to only the icon's own opaque pixels via XSetClipMask,
    // so whatever is already drawn underneath (the plain background,
    // or the selected-row highlight) shows through everywhere else.
    // That's real per-pixel transparency without needing a compositor
    // or an ARGB visual, which a plain core-Xlib window like this one
    // doesn't have.
    Pixmap pixmap = 0;
    Pixmap mask = 0;

    imlib_render_pixmaps_for_whole_image_at_size(
        &pixmap,
        &mask,
        20,
        20);

    imlib_free_image();

    maskOut = mask;
    return pixmap;
}

int Launcher::VisibleRows() const
{
    int lineHeight = 20;

    int startY = 50;

    return
        (m_geometry.height - startY - 10)
        / lineHeight;
}

void Launcher::UpdateMatches()
{
    m_matches.clear();
    m_scrollOffset = 0;

    for (std::size_t i = 0;
         i < m_entries.size();
         ++i)
    {
        if (m_query.empty())
        {
            m_matches.push_back(
{
    MatchType::Application,
    i,
    0
});

            continue;
        }

        int launches = 0;

        auto it =
            m_launchCounts.find(
                m_entries[i].name);

        if (it != m_launchCounts.end())
{
    launches = it->second;
}

int popularity = GetPopularityScore(m_entries[i]);

int score = CalculateScore(
    m_query,
    m_entries[i].name,
    launches,
    popularity);

        if (score >= 0)
        {
            m_matches.push_back(
{
    MatchType::Application,
    i,
    score
});
        }
    } // <-- закінчився for

    if (m_selectedIndex >=
        m_matches.size())
    {
        m_selectedIndex = 0;
        m_scrollOffset = 0;
    }

    for (std::size_t i = 0;
     i < m_files.size();
     ++i)
{
    int score =
        CalculateScore(
            m_query,
            m_files[i].name,
            0,
            0);

    if (score >= 0)
    {
        m_matches.push_back(
        {
            MatchType::File,
            i,
            score
        });
    }
}

std::sort(
        m_matches.begin(),
        m_matches.end(),
        [](const MatchResult& a,
           const MatchResult& b)
        {
            if (a.type != b.type)
            {
                return a.type == MatchType::Application;
            }

            return a.score > b.score;
        });

}

void Launcher::LoadLaunchHistory()
{
    m_launchCounts.clear();

    std::ifstream file(kHistoryFile);

    std::string name;
    int count;

    while (file >> name >> count)
    {
        m_launchCounts[name] = count;
    }
}

void Launcher::SaveLaunchHistory()
{
    std::ofstream file(kHistoryFile);

    for (const auto& [name, count] : m_launchCounts)
    {
        file << name
             << " "
             << count
             << "\n";
    }
}

void Launcher::ReloadDesktopEntries()
{
    m_entriesLoaded = false;
    LoadDesktopEntries();
    BuildFileIndex();
    UpdateMatches();
}

void Launcher::BuildFileIndex()
{
    namespace fs = std::filesystem;

    m_files.clear();

    // Loaded once and reused for every File match in Redraw() - these
    // are generic type icons (not per-file), so there's no point
    // re-resolving them from the icon theme for every entry.
    if (!m_folderIconPixmap)
    {
        m_folderIconPixmap =
            LoadIcon(FindIconPath("folder"), m_folderIconMask);
    }

    if (!m_fileIconPixmap)
    {
        m_fileIconPixmap =
            LoadIcon(FindIconPath("text-x-generic"), m_fileIconMask);
    }

    const char* home = std::getenv("HOME");

    if (!home)
        return;

    try
    {
        for (const auto& entry :
             fs::recursive_directory_iterator(home))
        {
            if (!entry.is_regular_file() &&
                !entry.is_directory())
                continue;

            FileEntry file;

            file.path =
                entry.path().string();

            file.name =
                entry.path().filename().string();

            file.isDirectory =
                entry.is_directory();

            m_files.push_back(
                std::move(file));
        }
    }
    catch (...)
    {
    }

    std::printf(
        "Indexed %zu files\n",
        m_files.size());
}

}