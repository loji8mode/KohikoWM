#pragma once

#include "Types.h"

#include <X11/Xlib.h>

#include <string>
#include <vector>

namespace Kohiko
{

class XAtoms;

// A thin wrapper around Xlib so the rest of Kohiko almost never touches
// X11 directly: MoveResize, Map, Unmap, Focus, Raise, Lower, Destroy,
// SendEvent, GrabKey, GrabButton, plus the handful of property reads
// (title/class/role/pid/transient-for/window-type) every window
// manager ends up needing.
class XConnection
{
public:

    XConnection();

    ~XConnection();

    bool Connect();

    bool BecomeWindowManager();

    void Disconnect();

    Display* GetDisplay() const;

    ::Window Root() const;

    int Screen() const;

    // The actual display string this connection resolved to (e.g.
    // ":1"), regardless of whether Connect() was given an explicit
    // name or fell back to $DISPLAY. Used to pin every process Kohiko
    // spawns to *this* X11 session instead of whatever DISPLAY value
    // (or none) happens to be sitting in the environment.
    std::string DisplayName() const;

    int ConnectionFd() const;

    void Flush();

    void Sync();

    // --- window lifecycle ---------------------------------------------

    void MapWindow(::Window window);

    void UnmapWindow(::Window window);

    // Forceful close.
    void DestroyWindow(::Window window);

    // Graceful close: WM_DELETE_WINDOW if the client advertises support
    // for it via WM_PROTOCOLS, otherwise XKillClient so an
    // unresponsive/legacy client can't block Close.
    void CloseWindow(::Window window, const XAtoms& atoms);

    void MoveResizeWindow(::Window window, const Rect& rect);

    // Pure reposition, no resize - used while a Swap drag is following
    // the cursor, where the window's size must stay exactly as picked
    // up until it lands on its new tile.
    void MoveWindowTo(::Window window, int x, int y);

    void SetBorderWidth(::Window window, int width);

    void SetBorderColor(::Window window, unsigned long pixel);

    void Raise(::Window window);

    void Lower(::Window window);

    void SetInputFocus(::Window window);

    void SelectInputFor(::Window window, long mask);

    // Needed whenever we *don't* honour a ConfigureRequest as asked
    // (tiled windows): most toolkits still expect an acknowledgement.
    void SendConfigureNotify(::Window window, const Rect& rect, int borderWidth);

    // The one place a raw XWindowChanges/value-mask pair is
    // unavoidable: honouring a ConfigureRequest exactly as a floating
    // or not-yet-managed client asked for it.
    void ConfigureWindowRaw(::Window window, XWindowChanges changes, unsigned int valueMask);

    // --- EWMH ---------------------------------------------------------------

    // Creates the invisible check window, points root and the check
    // window's _NET_SUPPORTING_WM_CHECK at each other, and publishes
    // _NET_SUPPORTED - see XConnection.cpp for why tools like flameshot
    // care about this. Returns the check window (Kohiko doesn't need
    // to do anything else with it, but keeps it around for the
    // lifetime of the connection rather than leaking an anonymous X
    // window). Called once, from WindowManager::Initialize().
    ::Window InitializeEwmhSupport(const XAtoms& atoms, const std::string& wmName);

    // Kept in sync with WindowRepository by Manage()/Unmanage() every
    // time a window starts or stops being managed.
    void SetClientList(const XAtoms& atoms, const std::vector<::Window>& clients);

    // Kept in sync by Focus()/FocusNextAvailable() - `window` is None
    // when nothing is focused (e.g. the last client on a workspace
    // just closed).
    void SetActiveWindow(const XAtoms& atoms, ::Window window);

    // True if `window` already carries _NET_WM_STATE_FULLSCREEN in its
    // _NET_WM_STATE property. Some toolkits set this directly (via
    // XChangeProperty) before ever mapping the window, rather than
    // sending a ClientMessage - the ClientMessage form of this request
    // is only meaningful for an already-mapped window, per the EWMH
    // spec, so a pre-map fullscreen request has to be read back this
    // way instead. WindowManager::Manage() checks this once, at map
    // time; WindowManager::HandleClientMessage() handles the
    // after-map ClientMessage form.
    bool HasNetWmStateFullscreen(::Window window, const XAtoms& atoms);

    // Keeps the window's own _NET_WM_STATE property honest after
    // Kohiko changes its fullscreen state (whether that came from the
    // client's own request, `windowrule=fullscreen`, or Super+F) - a
    // handful of apps re-check this after asking, and every one of
    // them is entitled to see it reflect reality either way.
    void SetNetWmState(const XAtoms& atoms, ::Window window, bool fullscreen);

    // The size a client itself would prefer, read from WM_NORMAL_HINTS
    // (PSize/USize, falling back to PBaseSize) - what a floating
    // window should actually be sized to instead of an arbitrary
    // fraction of the screen. Returns false (leaving width/height
    // untouched) if the client never specified a preferred size at
    // all, which callers should treat as "fall back to the window's
    // current/default size instead".
    bool GetPreferredSize(::Window window, int& width, int& height);

    // --- grabs -----------------------------------------------------------

    // Global grabs on the root window (keybinds, Super+drag).
    void GrabKey(unsigned int keycode, unsigned int modifiers);

    void GrabButtonOnRoot(unsigned int button, unsigned int modifiers, long eventMask);

    void UngrabAllKeys();

    void UngrabAllButtonsOnRoot();

    // Synchronous grab on one client window (click-to-focus). The
    // caller must call AllowReplayPointer() after updating focus so
    // the click still reaches the client normally.
    void GrabButtonOnWindow(::Window window, unsigned int button, unsigned int modifiers);

    void AllowReplayPointer();

    // NumLock's modifier bit, detected once and cached. XGrabKey/
    // XGrabButton match an *exact* modifier mask, so every bind needs
    // to be grabbed once per NumLock/CapsLock combination or it will
    // silently stop firing whenever NumLock happens to be on.
    unsigned int NumLockMask();

    std::vector<unsigned int> LockVariants(unsigned int baseModifiers);

    KeyCode KeysymToKeycode(KeySym keysym);

    // --- queries -----------------------------------------------------------

    bool GetWindowAttributes(::Window window, XWindowAttributes& out);

    std::string GetWindowTitle(::Window window, const XAtoms& atoms);

    void GetWindowClass(::Window window, std::string& className, std::string& instanceName);

    std::string GetWindowRole(::Window window, const XAtoms& atoms);

    long GetWindowPid(::Window window, const XAtoms& atoms);

    bool GetTransientFor(::Window window, ::Window& owner);

    bool IsDialog(::Window window, const XAtoms& atoms);

    std::string LastError() const;

private:

    static int ErrorHandler(
        Display*,
        XErrorEvent*
    );

private:

    Display* m_display = nullptr;

    ::Window m_root = 0;

    int m_screen = 0;

    bool m_wmExists = false;

    std::string m_lastError;

    unsigned int m_numLockMask = 0;
    bool m_numLockComputed = false;

    static XConnection* s_instance;

};

}
