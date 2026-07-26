#include "XConnection.h"

#include "XAtoms.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

namespace Kohiko
{

XConnection* XConnection::s_instance = nullptr;

XConnection::XConnection()
{
}

XConnection::~XConnection()
{
    Disconnect();
}

bool XConnection::Connect()
{
    m_display = XOpenDisplay(nullptr);

    if (!m_display)
    {
        m_lastError = "Cannot open display.";
        return false;
    }

    s_instance = this;

    m_screen = DefaultScreen(m_display);
    m_root = RootWindow(m_display, m_screen);

    return true;
}

bool XConnection::BecomeWindowManager()
{
    XSetErrorHandler(ErrorHandler);

    XSelectInput(
        m_display,
        m_root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        StructureNotifyMask |
        PropertyChangeMask |
        EnterWindowMask |
        LeaveWindowMask
    );

    XSync(m_display, False);

    if (m_wmExists)
    {
        m_lastError = "Window manager already running.";
        return false;
    }

    return true;
}

void XConnection::Disconnect()
{
    if (!m_display)
        return;

    XCloseDisplay(m_display);
    m_display = nullptr;
}

Display* XConnection::GetDisplay() const
{
    return m_display;
}

::Window XConnection::Root() const
{
    return m_root;
}

int XConnection::Screen() const
{
    return m_screen;
}

std::string XConnection::DisplayName() const
{
    if (!m_display)
        return std::string();

    // XDisplayString() reports the name actually in effect (Connect()
    // passes nullptr to XOpenDisplay(), which resolves via $DISPLAY
    // itself) - reading it back here rather than re-reading getenv()
    // guarantees we report exactly the session this connection is on.
    return XDisplayString(m_display);
}

int XConnection::ConnectionFd() const
{
    return ConnectionNumber(m_display);
}

void XConnection::Flush()
{
    XFlush(m_display);
}

void XConnection::Sync()
{
    XSync(m_display, False);
}

void XConnection::MapWindow(::Window window)
{
    XMapWindow(m_display, window);
}

void XConnection::UnmapWindow(::Window window)
{
    XUnmapWindow(m_display, window);
}

void XConnection::DestroyWindow(::Window window)
{
    XDestroyWindow(m_display, window);
}

void XConnection::CloseWindow(::Window window, const XAtoms& atoms)
{
    Atom* protocols = nullptr;
    int count = 0;
    bool supportsDelete = false;

    if (XGetWMProtocols(m_display, window, &protocols, &count))
    {
        for (int i = 0; i < count; ++i)
        {
            if (protocols[i] == atoms.WM_DELETE_WINDOW)
            {
                supportsDelete = true;
                break;
            }
        }

        if (protocols)
            XFree(protocols);
    }

    if (supportsDelete)
    {
        XEvent event{};
        event.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = atoms.WM_PROTOCOLS;
        event.xclient.format = 32;
        event.xclient.data.l[0] = static_cast<long>(atoms.WM_DELETE_WINDOW);
        event.xclient.data.l[1] = CurrentTime;

        XSendEvent(m_display, window, False, NoEventMask, &event);
    }
    else
    {
        XKillClient(m_display, window);
    }
}

void XConnection::MoveResizeWindow(::Window window, const Rect& rect)
{
    XMoveResizeWindow(
        m_display,
        window,
        rect.x,
        rect.y,
        static_cast<unsigned int>(rect.width > 0 ? rect.width : 1),
        static_cast<unsigned int>(rect.height > 0 ? rect.height : 1)
    );
}

void XConnection::MoveWindowTo(::Window window, int x, int y)
{
    XMoveWindow(m_display, window, x, y);
}

void XConnection::SetBorderWidth(::Window window, int width)
{
    XSetWindowBorderWidth(m_display, window, static_cast<unsigned int>(width));
}

void XConnection::SetBorderColor(::Window window, unsigned long pixel)
{
    XSetWindowBorder(m_display, window, pixel);
}

void XConnection::Raise(::Window window)
{
    XRaiseWindow(m_display, window);
}

void XConnection::Lower(::Window window)
{
    XLowerWindow(m_display, window);
}

void XConnection::SetInputFocus(::Window window)
{
    XSetInputFocus(m_display, window, RevertToPointerRoot, CurrentTime);
}

void XConnection::SelectInputFor(::Window window, long mask)
{
    XSelectInput(m_display, window, mask);
}

void XConnection::SendConfigureNotify(::Window window, const Rect& rect, int borderWidth)
{
    XConfigureEvent event{};

    event.type = ConfigureNotify;
    event.event = window;
    event.window = window;
    event.x = rect.x;
    event.y = rect.y;
    event.width = rect.width;
    event.height = rect.height;
    event.border_width = borderWidth;
    event.above = None;
    event.override_redirect = False;

    XSendEvent(m_display, window, False, StructureNotifyMask, reinterpret_cast<XEvent*>(&event));
}

void XConnection::ConfigureWindowRaw(::Window window, XWindowChanges changes, unsigned int valueMask)
{
    XConfigureWindow(m_display, window, valueMask, &changes);
}

void XConnection::GrabKey(unsigned int keycode, unsigned int modifiers)
{
    XGrabKey(
        m_display,
        static_cast<int>(keycode),
        modifiers,
        m_root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );
}

void XConnection::GrabButtonOnRoot(unsigned int button, unsigned int modifiers, long eventMask)
{
    XGrabButton(
        m_display,
        button,
        modifiers,
        m_root,
        True,
        static_cast<unsigned int>(eventMask),
        GrabModeAsync,
        GrabModeAsync,
        None,
        None
    );
}

void XConnection::UngrabAllKeys()
{
    XUngrabKey(m_display, AnyKey, AnyModifier, m_root);
}

void XConnection::UngrabAllButtonsOnRoot()
{
    XUngrabButton(m_display, AnyButton, AnyModifier, m_root);
}

void XConnection::GrabButtonOnWindow(::Window window, unsigned int button, unsigned int modifiers)
{
    XGrabButton(
        m_display,
        button,
        modifiers,
        window,
        False,
        ButtonPressMask,
        GrabModeSync,
        GrabModeAsync,
        None,
        None
    );
}

void XConnection::AllowReplayPointer()
{
    XAllowEvents(m_display, ReplayPointer, CurrentTime);
}

unsigned int XConnection::NumLockMask()
{
    if (m_numLockComputed)
        return m_numLockMask;

    m_numLockComputed = true;
    m_numLockMask = 0;

    XModifierKeymap* map = XGetModifierMapping(m_display);

    if (!map)
        return m_numLockMask;

    KeyCode numLockCode = XKeysymToKeycode(m_display, XK_Num_Lock);

    if (numLockCode != 0)
    {
        for (int mod = 0; mod < 8; ++mod)
        {
            for (int k = 0; k < map->max_keypermod; ++k)
            {
                KeyCode code = map->modifiermap[mod * map->max_keypermod + k];

                if (code == numLockCode)
                    m_numLockMask = (1u << mod);
            }
        }
    }

    XFreeModifiermap(map);

    return m_numLockMask;
}

std::vector<unsigned int> XConnection::LockVariants(unsigned int baseModifiers)
{
    unsigned int numLock = NumLockMask();

    return {
        baseModifiers,
        baseModifiers | LockMask,
        baseModifiers | numLock,
        baseModifiers | numLock | LockMask,
    };
}

KeyCode XConnection::KeysymToKeycode(KeySym keysym)
{
    return XKeysymToKeycode(m_display, keysym);
}

bool XConnection::GetWindowAttributes(::Window window, XWindowAttributes& out)
{
    return XGetWindowAttributes(m_display, window, &out) != 0;
}

std::string XConnection::GetWindowTitle(::Window window, const XAtoms& atoms)
{
    Atom actualType;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesLeft = 0;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(
            m_display, window, atoms.NET_WM_NAME, 0, 1024, False,
            AnyPropertyType, &actualType, &actualFormat, &itemCount, &bytesLeft, &data
        ) == Success && data)
    {
        std::string title(reinterpret_cast<char*>(data), itemCount);
        XFree(data);

        if (!title.empty())
            return title;
    }

    char* legacyName = nullptr;

    if (XFetchName(m_display, window, &legacyName) && legacyName)
    {
        std::string title(legacyName);
        XFree(legacyName);
        return title;
    }

    return {};
}

void XConnection::GetWindowClass(::Window window, std::string& className, std::string& instanceName)
{
    XClassHint hint{};

    if (XGetClassHint(m_display, window, &hint))
    {
        if (hint.res_class)
        {
            className = hint.res_class;
            XFree(hint.res_class);
        }

        if (hint.res_name)
        {
            instanceName = hint.res_name;
            XFree(hint.res_name);
        }
    }
}

std::string XConnection::GetWindowRole(::Window window, const XAtoms& atoms)
{
    Atom actualType;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesLeft = 0;
    unsigned char* data = nullptr;

    std::string role;

    if (XGetWindowProperty(
            m_display, window, atoms.WM_WINDOW_ROLE, 0, 1024, False,
            AnyPropertyType, &actualType, &actualFormat, &itemCount, &bytesLeft, &data
        ) == Success && data)
    {
        role.assign(reinterpret_cast<char*>(data), itemCount);
        XFree(data);
    }

    return role;
}

long XConnection::GetWindowPid(::Window window, const XAtoms& atoms)
{
    Atom actualType;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesLeft = 0;
    unsigned char* data = nullptr;

    long pid = 0;

    if (XGetWindowProperty(
            m_display, window, atoms.NET_WM_PID, 0, 1, False,
            XA_CARDINAL, &actualType, &actualFormat, &itemCount, &bytesLeft, &data
        ) == Success && data)
    {
        if (itemCount > 0)
            pid = static_cast<long>(*reinterpret_cast<unsigned long*>(data));

        XFree(data);
    }

    return pid;
}

bool XConnection::GetTransientFor(::Window window, ::Window& owner)
{
    ::Window transient = 0;

    if (XGetTransientForHint(m_display, window, &transient) && transient != 0)
    {
        owner = transient;
        return true;
    }

    return false;
}

bool XConnection::IsDialog(::Window window, const XAtoms& atoms)
{
    Atom actualType;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesLeft = 0;
    unsigned char* data = nullptr;

    bool isDialog = false;

    if (XGetWindowProperty(
            m_display, window, atoms.NET_WM_WINDOW_TYPE, 0, 16, False,
            XA_ATOM, &actualType, &actualFormat, &itemCount, &bytesLeft, &data
        ) == Success && data)
    {
        Atom* types = reinterpret_cast<Atom*>(data);

        for (unsigned long i = 0; i < itemCount; ++i)
        {
            if (types[i] == atoms.NET_WM_WINDOW_TYPE_DIALOG)
            {
                isDialog = true;
                break;
            }
        }

        XFree(data);
    }

    return isDialog;
}

std::string XConnection::LastError() const
{
    return m_lastError;
}

int XConnection::ErrorHandler(Display*, XErrorEvent* event)
{
    if (!s_instance)
        return 0;

    if (event->error_code == BadAccess)
        s_instance->m_wmExists = true;

    return 0;
}

}
