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
