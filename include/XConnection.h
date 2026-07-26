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
