#pragma once

#include "LayoutEngine.h"
#include "WindowRepository.h"
#include "WorkspaceManager.h"

#include <X11/Xlib.h>

namespace Kohiko
{

class XConnection;

class WindowManager
{
public:

    explicit WindowManager(
        XConnection& connection
    );

    void HandleMapRequest(
        const XMapRequestEvent&
    );

    void HandleConfigureRequest(
        const XConfigureRequestEvent&
    );

    void HandleDestroyNotify(
        const XDestroyWindowEvent&
    );

    void HandleUnmapNotify(
        const XUnmapEvent&
    );

    void HandleEnterNotify(
        const XCrossingEvent&
    );

    void Arrange();

    void Focus(
        WindowID window
    );

    void SwitchWorkspace(
        int workspace
    );

    void CloseFocused();

    void ToggleFloating();

private:

    void Manage(
        WindowID window
    );

    void Unmanage(
        WindowID window
    );

private:

    XConnection& m_connection;

    WindowRepository m_repository;

    WorkspaceManager m_workspaces;

    LayoutEngine m_layout;

};
}