#pragma once

#include <X11/Xlib.h>

namespace Kohiko
{

class XConnection;

class XAtoms
{
public:

    explicit XAtoms(
        XConnection& connection
    );

    void Initialize();

    Atom WM_DELETE_WINDOW;

    Atom NET_ACTIVE_WINDOW;

    Atom NET_WM_STATE;

    Atom NET_WM_STATE_FULLSCREEN;

    Atom NET_CURRENT_DESKTOP;

    Atom NET_WM_DESKTOP;

    Atom UTF8_STRING;

private:

    XConnection& m_connection;

};

}