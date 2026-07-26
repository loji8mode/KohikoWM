#include "LockScreen.h"

#include "Authenticator.h"
#include "Config.h"
#include "Monitor.h"
#include "MonitorManager.h"
#include "Utils.h"
#include "XConnection.h"

#include <Imlib2.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <unistd.h>

namespace Kohiko
{

LockScreen::LockScreen(
    XConnection& connection)
    :
    m_connection(connection)
{
}

LockScreen::~LockScreen()
{
    Display* display = m_connection.GetDisplay();

    if (!display)
        return;

    if (m_xic)
        XDestroyIC(m_xic);

    if (m_xim)
        XCloseIM(m_xim);

    if (m_xftDraw)
        XftDrawDestroy(m_xftDraw);

    m_font.Unload();

    for (Pixmap pixmap : m_backgroundPixmaps)
        if (pixmap)
            XFreePixmap(display, pixmap);

    if (m_logoPixmap)
        XFreePixmap(display, m_logoPixmap);

    if (m_logoMask)
        XFreePixmap(display, m_logoMask);

    if (m_gc)
        XFreeGC(display, m_gc);

    if (m_window)
        XDestroyWindow(display, m_window);
}

void LockScreen::Configure(
    const Config& config)
{
    m_backgroundPixel = std::strtoul(config.GetString("lockscreen.background_color", "0x1e1e2e").c_str(), nullptr, 0);
    m_foregroundPixel = std::strtoul(config.GetString("lockscreen.foreground", "0xcdd6f4").c_str(), nullptr, 0);
    m_fieldPixel       = std::strtoul(config.GetString("lockscreen.field_color", "0x313244").c_str(), nullptr, 0);
    m_errorPixel       = std::strtoul(config.GetString("lockscreen.error_color", "0xf38ba8").c_str(), nullptr, 0);

    m_backgroundImagePath = config.GetString("lockscreen.background_image", "");
    m_logoPath            = config.GetString("lockscreen.logo", "");

    std::string fontPattern = config.GetString(
        "lockscreen.font",
        config.GetString("general.font", "monospace:pixelsize=14"));

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    if (m_window == 0)
    {
        int width  = DisplayWidth(display, screen);
        int height = DisplayHeight(display, screen);

        XSetWindowAttributes attrs{};
        attrs.override_redirect = True; // this is *our* window - never redirect it back to ourselves as a MapRequest
        attrs.background_pixel = m_backgroundPixel;
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

        m_window = XCreateWindow(
            display,
            m_connection.Root(),
            0, 0,
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            0,
            DefaultDepth(display, screen),
            InputOutput,
            DefaultVisual(display, screen),
            CWOverrideRedirect | CWBackPixel | CWEventMask,
            &attrs
        );

        m_gc = XCreateGC(display, m_window, 0, nullptr);

        m_xftDraw = XftDrawCreate(
            display,
            m_window,
            DefaultVisual(display, screen),
            DefaultColormap(display, screen));

        // Same reasoning as Notepad's own XIM/XIC setup: without this,
        // a password containing anything outside plain ASCII wouldn't
        // compose correctly (dead keys, IME sequences, ...).
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
        XSetWindowBackground(display, m_window, m_backgroundPixel);
    }

    m_font.Load(display, screen, fontPattern);

    if (m_logoPixmap)
    {
        XFreePixmap(display, m_logoPixmap);
        m_logoPixmap = 0;
    }

    if (m_logoMask)
    {
        XFreePixmap(display, m_logoMask);
        m_logoMask = 0;
    }

    if (!m_logoPath.empty())
    {
        imlib_context_set_display(display);
        imlib_context_set_visual(DefaultVisual(display, screen));
        imlib_context_set_colormap(DefaultColormap(display, screen));
        imlib_context_set_drawable(m_window);

        Imlib_Image image = imlib_load_image(m_logoPath.c_str());

        if (image)
        {
            imlib_context_set_image(image);

            imlib_render_pixmaps_for_whole_image_at_size(
                &m_logoPixmap, &m_logoMask,
                m_logoSize, m_logoSize);

            imlib_free_image();
        }
    }
}

Pixmap LockScreen::LoadImageStretched(
    const std::string& path,
    int width,
    int height) const
{
    if (path.empty() || width <= 0 || height <= 0 || m_window == 0)
        return 0;

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    imlib_context_set_display(display);
    imlib_context_set_visual(DefaultVisual(display, screen));
    imlib_context_set_colormap(DefaultColormap(display, screen));
    imlib_context_set_drawable(m_window);

    Imlib_Image image = imlib_load_image(path.c_str());

    if (!image)
        return 0;

    imlib_context_set_image(image);

    Pixmap pixmap = 0;
    Pixmap mask = 0; // a full-bleed background never needs alpha clipping - see this function's own header comment

    imlib_render_pixmaps_for_whole_image_at_size(&pixmap, &mask, width, height);
    imlib_free_image();

    if (mask)
        XFreePixmap(display, mask);

    return pixmap;
}

void LockScreen::Lock(
    const MonitorManager& monitors)
{
    if (m_locked || m_window == 0)
        return;

    struct passwd* pw = getpwuid(getuid());
    m_username = pw ? pw->pw_name : std::string();

    // Per the spec: an account with no password configured at all
    // unlocks immediately - which, using PAM for the check the exact
    // same way a real attempt would, just means trying an empty
    // password right now, before ever mapping anything. See
    // Authenticator's own header comment.
    if (Authenticator::Authenticate(m_username, ""))
        return;

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    m_monitorGeometries.clear();

    for (Pixmap pixmap : m_backgroundPixmaps)
        if (pixmap)
            XFreePixmap(display, pixmap);

    m_backgroundPixmaps.clear();

    for (const auto& monitor : monitors.All())
    {
        m_monitorGeometries.push_back(monitor->Geometry());
        m_backgroundPixmaps.push_back(
            LoadImageStretched(m_backgroundImagePath, monitor->Geometry().width, monitor->Geometry().height));
    }

    Rect full{0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen)};
    m_connection.MoveResizeWindow(m_window, full);
    m_connection.MapWindow(m_window);
    m_connection.Raise(m_window);

    // XGrabKeyboard/XGrabPointer, not just stacking + real X input
    // focus the way Launcher/Notepad get away with - see this class's
    // own header comment for why a *cooperative* focus model isn't
    // enough for a lock screen specifically: any client that calls
    // XSetInputFocus on itself (some do, bypassing the WM entirely)
    // could otherwise steal keystrokes - including the password
    // itself - right out from under it. An active grab makes Kohiko
    // the sole arbiter of where input goes, full stop, regardless of
    // what any other client does.
    //
    // GrabNotViewable can happen transiently for a window that was
    // *just* mapped (the map request may not have reached the server
    // yet) - a short bounded retry is the standard fix every other
    // X11 locker uses for this exact race, not a sign of anything
    // actually wrong.
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        if (XGrabKeyboard(display, m_window, False, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
            break;

        usleep(20000);
    }

    for (int attempt = 0; attempt < 50; ++attempt)
    {
        if (XGrabPointer(
                display, m_window, False,
                ButtonPressMask | ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync,
                m_window, None, CurrentTime) == GrabSuccess)
            break;

        usleep(20000);
    }

    m_connection.SetInputFocus(m_window);

    m_typed.clear();
    m_showError = false;
    m_locked = true;

    Redraw();
}

void LockScreen::Unlock()
{
    if (!m_locked)
        return;

    Display* display = m_connection.GetDisplay();

    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);

    m_connection.UnmapWindow(m_window);

    m_typed.clear();
    m_showError = false;
    m_locked = false;
}

bool LockScreen::IsLocked() const
{
    return m_locked;
}

::Window LockScreen::WindowId() const
{
    return m_window;
}

void LockScreen::HandleExpose()
{
    Redraw();
}

void LockScreen::Reposition(
    const MonitorManager& monitors)
{
    if (!m_locked || m_window == 0)
        return;

    Display* display = m_connection.GetDisplay();
    int screen = m_connection.Screen();

    m_monitorGeometries.clear();

    for (Pixmap pixmap : m_backgroundPixmaps)
        if (pixmap)
            XFreePixmap(display, pixmap);

    m_backgroundPixmaps.clear();

    for (const auto& monitor : monitors.All())
    {
        m_monitorGeometries.push_back(monitor->Geometry());
        m_backgroundPixmaps.push_back(
            LoadImageStretched(m_backgroundImagePath, monitor->Geometry().width, monitor->Geometry().height));
    }

    Rect full{0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen)};
    m_connection.MoveResizeWindow(m_window, full);

    Redraw();
}

void LockScreen::HandleKeyPress(
    const XKeyEvent& event)
{
    if (!m_locked)
        return;

    char buffer[32];
    KeySym keysym = NoSymbol;
    XKeyEvent mutableEvent = event; // Xutf8LookupString wants a non-const pointer
    Status status;
    int len;

    if (m_xic)
        len = Xutf8LookupString(m_xic, &mutableEvent, buffer, sizeof(buffer) - 1, &keysym, &status);
    else
        len = XLookupString(&mutableEvent, buffer, sizeof(buffer) - 1, &keysym, nullptr);

    if (len < 0)
        len = 0;

    switch (keysym)
    {
        case XK_Escape:
            m_typed.clear();
            m_showError = false;
            break;

        case XK_BackSpace:

            if (!m_typed.empty())
            {
                std::size_t prev = Utils::Utf8PrevBoundary(m_typed, m_typed.size());
                m_typed.erase(prev);
            }

            break;

        case XK_Return:
        case XK_KP_Enter:

            if (Authenticator::Authenticate(m_username, m_typed))
            {
                Unlock();
                return;
            }

            m_typed.clear();
            m_showError = true;
            m_errorUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(1800);
            break;

        default:

            buffer[len] = '\0';

            // Filters out control characters (Tab, arrow keys mapping
            // to escape sequences under plain XLookupString without an
            // XIC, ...) - a real typed character never starts below
            // 0x20 in UTF-8.
            if (len > 0 && static_cast<unsigned char>(buffer[0]) >= 0x20)
                m_typed.append(buffer, static_cast<std::size_t>(len));

            break;
    }

    Redraw();
}

void LockScreen::Tick()
{
    if (m_showError && std::chrono::steady_clock::now() >= m_errorUntil)
    {
        m_showError = false;
        Redraw();
    }
}

void LockScreen::Redraw()
{
    if (!m_locked || m_window == 0 || !m_xftDraw)
        return;

    Display* display = m_connection.GetDisplay();
    Visual* visual = DefaultVisual(display, m_connection.Screen());
    Colormap colormap = DefaultColormap(display, m_connection.Screen());

    TextColor foreground(display, visual, colormap, m_foregroundPixel);
    TextColor error(display, visual, colormap, m_errorPixel);

    for (std::size_t i = 0; i < m_monitorGeometries.size(); ++i)
    {
        const Rect& geo = m_monitorGeometries[i];

        if (i < m_backgroundPixmaps.size() && m_backgroundPixmaps[i] != 0)
        {
            XCopyArea(
                display, m_backgroundPixmaps[i], m_window, m_gc,
                0, 0,
                static_cast<unsigned int>(geo.width), static_cast<unsigned int>(geo.height),
                geo.x, geo.y);
        }
        else
        {
            XSetForeground(display, m_gc, m_backgroundPixel);
            XFillRectangle(
                display, m_window, m_gc,
                geo.x, geo.y,
                static_cast<unsigned int>(geo.width), static_cast<unsigned int>(geo.height));
        }

        int centerX = geo.CenterX();
        int centerY = geo.CenterY();

        if (m_logoPixmap)
        {
            int logoX = centerX - m_logoSize / 2;
            int logoY = centerY - 110;

            if (m_logoMask)
            {
                XSetClipMask(display, m_gc, m_logoMask);
                XSetClipOrigin(display, m_gc, logoX, logoY);
            }

            XCopyArea(
                display, m_logoPixmap, m_window, m_gc,
                0, 0,
                static_cast<unsigned int>(m_logoSize), static_cast<unsigned int>(m_logoSize),
                logoX, logoY);

            if (m_logoMask)
                XSetClipMask(display, m_gc, None);
        }

        int fieldWidth = 260;
        int fieldHeight = 40;
        int fieldX = centerX - fieldWidth / 2;
        int fieldY = centerY - fieldHeight / 2;

        XSetForeground(display, m_gc, m_fieldPixel);
        XFillRectangle(
            display, m_window, m_gc,
            fieldX, fieldY,
            static_cast<unsigned int>(fieldWidth), static_cast<unsigned int>(fieldHeight));

        XSetForeground(display, m_gc, m_showError ? m_errorPixel : m_foregroundPixel);
        XDrawRectangle(
            display, m_window, m_gc,
            fieldX, fieldY,
            static_cast<unsigned int>(fieldWidth) - 1, static_cast<unsigned int>(fieldHeight) - 1);

        // Masked input: a fixed placeholder character rather than the
        // real text, and a plain ASCII one specifically (not a nicer
        // bullet glyph) so it's guaranteed to render on every font -
        // same reasoning as the bar's "[S]"/"[N]" indicators.
        std::string dots(std::min(m_typed.size(), static_cast<std::size_t>(24)), '*');
        int fieldBaseline = fieldY + fieldHeight / 2 + m_font.Ascent() / 2;
        m_font.DrawString(m_xftDraw, fieldX + 14, fieldBaseline, dots, foreground.Get());

        std::string caption = m_showError ? "Wrong password" : (m_username.empty() ? "Locked" : m_username);
        int captionWidth = m_font.TextWidth(caption);

        m_font.DrawString(
            m_xftDraw,
            centerX - captionWidth / 2,
            fieldY - 16,
            caption,
            m_showError ? error.Get() : foreground.Get());
    }

    XFlush(display);
}

}
