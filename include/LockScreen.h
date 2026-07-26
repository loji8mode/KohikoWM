#pragma once

#include "Font.h"
#include "Types.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <string>
#include <vector>

namespace Kohiko
{

class XConnection;
class Config;
class MonitorManager;

// Kohiko's own lock screen - no i3lock/betterlockscreen dependency,
// authenticated via PAM (see Authenticator.h). Full-screen password
// field, hidden input, Escape clears what's typed, Enter submits.
//
// Deliberately a single override-redirect window sized to the whole X
// *screen* rather than one per monitor: under XRandR (the only
// multi-monitor backend this project supports - see MonitorManager),
// the X screen itself already spans the bounding box of every
// connected monitor, so one window covers all of them at once with no
// per-monitor keyboard-focus juggling needed. The password
// prompt/field is simply drawn once per monitor region within that
// single canvas - see Redraw().
//
// While locked, this holds an *active* XGrabKeyboard/XGrabPointer, not
// just window stacking + real X input focus the way Launcher/Notepad
// do - stacking alone wouldn't stop Kohiko's own global hotkeys (bound
// as passive grabs on the root window) from still firing underneath.
// See Lock()/Unlock().
class LockScreen
{
public:

    explicit LockScreen(
        XConnection& connection
    );

    ~LockScreen();

    // Reads every lockscreen.* setting - safe to call again later
    // (a config reload), same as Bar/Launcher/Notepad/PowerMenu's own
    // Configure().
    void Configure(
        const Config& config
    );

    // Locks immediately - UNLESS the current user's account has no
    // password configured at all (see Authenticator's own header
    // comment for exactly how that's detected), in which case this
    // returns without ever mapping anything, per the spec.
    void Lock(
        const MonitorManager& monitors
    );

    // Only ever called internally, once Authenticate() actually
    // succeeds (see HandleKeyPress()) - there is deliberately no other
    // way to reach this from the outside.
    void Unlock();

    bool IsLocked() const;

    ::Window WindowId() const;

    void HandleExpose();

    // Escape clears whatever's typed so far (never unlocks - see this
    // class's own header comment on why that's the *only* thing
    // Escape does here, unlike every other modal in this codebase).
    // Enter attempts authentication: on success, unlocks; on failure,
    // clears the field and shows a brief error instead.
    void HandleKeyPress(
        const XKeyEvent& event
    );

    // Re-covers every monitor after a hotplug/topology change while
    // still locked - WindowManager's own HandleMonitorTopologyChanged()
    // calls this the same way it already rebuilds bars. A no-op if
    // not currently locked.
    void Reposition(
        const MonitorManager& monitors
    );

    // Clears the brief post-failed-attempt error message once its
    // time is up - called from WindowManager::Tick(), the same place
    // Bar's own notification and Launcher's caret blink already get
    // driven from.
    void Tick();

private:

    void Redraw();

    // Loads `path` via Imlib2 at exactly `width`x`height`, stretched
    // to fill (not aspect-preserving - see this function's one call
    // site in Redraw() for why that's an acceptable simplification
    // here). Returns 0 on any failure (missing file, bad path, ...),
    // which every caller already treats as "fall back to a solid
    // color" rather than a hard error.
    Pixmap LoadImageStretched(
        const std::string& path,
        int width,
        int height
    ) const;

private:

    XConnection& m_connection;

    ::Window m_window = 0;
    GC m_gc = nullptr;
    Font m_font;
    XftDraw* m_xftDraw = nullptr;

    XIM m_xim = nullptr;
    XIC m_xic = nullptr;

    bool m_locked = false;
    std::string m_username;
    std::string m_typed;

    // See Tick()/HandleKeyPress().
    bool m_showError = false;
    std::chrono::steady_clock::time_point m_errorUntil;

    // One entry per currently-locked-onto monitor, refreshed by
    // Lock()/Reposition() - Redraw()/HandleExpose() only ever read
    // this, so neither needs a MonitorManager reference threaded
    // through them.
    std::vector<Rect> m_monitorGeometries;

    // The background image, if configured, rendered once per monitor
    // at that monitor's own size - the same Lock()/Reposition() calls
    // that refresh m_monitorGeometries above refresh this too. See
    // LoadImageStretched()'s own comment for why a fresh render per
    // monitor (rather than one shared pixmap) is simplest here.
    std::vector<Pixmap> m_backgroundPixmaps;

    // The optional logo, loaded once at a fixed size and reused
    // as-is on every monitor (unlike the background, its native size
    // is exactly what should be drawn everywhere, not stretched to
    // fill each monitor).
    Pixmap m_logoPixmap = 0;
    Pixmap m_logoMask = 0;
    int m_logoSize = 96;

    std::string m_backgroundImagePath;
    std::string m_logoPath;

    unsigned long m_backgroundPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_fieldPixel = 0;
    unsigned long m_errorPixel = 0;

};

}
