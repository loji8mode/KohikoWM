#include "WindowManager.h"

#include "ManagedWindow.h"
#include "Workspace.h"
#include "XConnection.h"

namespace Kohiko
{

WindowManager::WindowManager(
    XConnection& connection)
    :
    m_connection(connection)
{
}

void WindowManager::HandleMapRequest(
    const XMapRequestEvent& event)
{
    Manage(event.window);
}

void WindowManager::HandleConfigureRequest(
    const XConfigureRequestEvent&)
{
    Arrange();
}

void WindowManager::HandleDestroyNotify(
    const XDestroyWindowEvent& event)
{
    Unmanage(event.window);
}

void WindowManager::HandleUnmapNotify(
    const XUnmapEvent& event)
{
    Unmanage(event.window);
}

void WindowManager::HandleEnterNotify(
    const XCrossingEvent& event)
{
    Focus(event.window);
}

void WindowManager::Manage(
    WindowID id)
{
    if (m_repository.Contains(id))
        return;

    auto* window =
        m_repository.Add(id);

    window->SetWorkspace(
        m_workspaces.Current().Id());

    m_workspaces
        .Current()
        .Tree()
        .Insert(window);

    m_connection.MapWindow(id);

    Arrange();

    Focus(id);
}

void WindowManager::Unmanage(
    WindowID id)
{
    auto* window =
        m_repository.Get(id);

    if (!window)
        return;

    m_workspaces
        .Get(window->Workspace())
        .Tree()
        .Remove(window);

    m_repository.Remove(id);

    Arrange();
}

void WindowManager::Focus(
    WindowID id)
{
    auto* window =
        m_repository.Get(id);

    if (!window)
        return;

    m_repository.ClearFocus();

    window->SetFocused(true);

    m_workspaces
        .Current()
        .Tree()
        .Focus(window);

    m_connection.SetInputFocus(id);
}

void WindowManager::Arrange()
{
    Rect monitor;

    monitor.x = 0;
    monitor.y = 0;

    monitor.width =
        DisplayWidth(
            m_connection.Display(),
            m_connection.Screen());

    monitor.height =
        DisplayHeight(
            m_connection.Display(),
            m_connection.Screen());

    m_layout.Apply(
        m_workspaces
            .Current()
            .Tree()
            .Root(),
        monitor);

    for (auto* window :
         m_repository.Workspace(
             m_workspaces.Current().Id()))
    {
        if (window->IsFloating())
            continue;

        if (window->IsScratchpad())
            continue;

        m_connection.MoveResizeWindow(
            window->Id(),
            window->Geometry());
    }

    m_connection.Flush();
}

   void WindowManager::SwitchWorkspace(
    int workspace)
{
    m_workspaces.Switch(
        workspace);

    Arrange();
}

void WindowManager::CloseFocused()
{
    for(auto* window :
        m_repository.All())
    {
        if(!window->Focused())
            continue;

        m_connection.DestroyWindow(
            window->Id());

        break;
    }
}

void WindowManager::ToggleFloating()
{
    for(auto* window :
        m_repository.All())
    {
        if(!window->Focused())
            continue;

        if(window->IsFloating())
            window->SetState(
                WindowState::Tiled);
        else
            window->SetState(
                WindowState::Floating);

        Arrange();

        break;
    }
}