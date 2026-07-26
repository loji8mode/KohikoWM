#pragma once

#include <X11/Xlib.h>

namespace Kohiko
{

class XConnection;

// Every X atom Kohiko needs, interned once at startup.
class XAtoms
{
public:

    explicit XAtoms(
        XConnection& connection
    );

    void Initialize();

    Atom WM_PROTOCOLS;
    Atom WM_DELETE_WINDOW;

    // EWMH root-window identification, published by
    // XConnection::InitializeEwmhSupport() so tools that check for a
    // compliant window manager (flameshot's screenshot overlay among
    // them) can trust NET_CLIENT_LIST/NET_ACTIVE_WINDOW instead of
    // falling back to cruder window discovery.
    Atom NET_SUPPORTED;
    Atom NET_SUPPORTING_WM_CHECK;
    Atom NET_CLIENT_LIST;

    Atom NET_ACTIVE_WINDOW;
    Atom NET_WM_STATE;
    Atom NET_WM_STATE_FULLSCREEN;
    Atom NET_CURRENT_DESKTOP;
    Atom NET_WM_DESKTOP;
    Atom NET_WM_NAME;
    Atom NET_WM_WINDOW_TYPE;
    Atom NET_WM_WINDOW_TYPE_DIALOG;
    Atom NET_WM_PID;

    Atom WM_WINDOW_ROLE;

    Atom UTF8_STRING;

private:

    XConnection& m_connection;

};

}
