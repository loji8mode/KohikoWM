#include "XAtoms.h"

#include "XConnection.h"

namespace Kohiko
{

XAtoms::XAtoms(
    XConnection& connection)
    :
    m_connection(connection)
{
}

void XAtoms::Initialize()
{
    Display* display =
        m_connection.Display();

    WM_DELETE_WINDOW =
        XInternAtom(
            display,
            "WM_DELETE_WINDOW",
            False);

    NET_ACTIVE_WINDOW =
        XInternAtom(
            display,
            "_NET_ACTIVE_WINDOW",
            False);

    NET_WM_STATE =
        XInternAtom(
            display,
            "_NET_WM_STATE",
            False);

    NET_WM_STATE_FULLSCREEN =
        XInternAtom(
            display,
            "_NET_WM_STATE_FULLSCREEN",
            False);

    NET_CURRENT_DESKTOP =
        XInternAtom(
            display,
            "_NET_CURRENT_DESKTOP",
            False);

    NET_WM_DESKTOP =
        XInternAtom(
            display,
            "_NET_WM_DESKTOP",
            False);

    UTF8_STRING =
        XInternAtom(
            display,
            "UTF8_STRING",
            False);
}

}