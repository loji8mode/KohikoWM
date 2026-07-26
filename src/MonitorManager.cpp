#include "MonitorManager.h"

#include "WorkspaceManager.h"
#include "Workspace.h"
#include "XConnection.h"

#include <X11/Xlib.h>

namespace Kohiko
{

MonitorManager::MonitorManager(
    XConnection& connection,
    WorkspaceManager& workspaces)
    :
    m_connection(connection),
    m_workspaces(workspaces)
{
}

void MonitorManager::Detect()
{
    m_monitors.clear();

    auto monitor =
        std::make_unique<Monitor>(0);

    Rect geometry;

    geometry.x = 0;
    geometry.y = 0;

    geometry.width =
        DisplayWidth(
            m_connection.Display(),
            m_connection.Screen());

    geometry.height =
        DisplayHeight(
            m_connection.Display(),
            m_connection.Screen());

    monitor->SetGeometry(
        geometry);

    monitor->SetWorkspace(
        &m_workspaces.Current());

    m_monitors.push_back(
        std::move(monitor));
}

Monitor&
MonitorManager::Primary()
{
    return *m_monitors.front();
}

const std::vector<
    std::unique_ptr<Monitor>>&
MonitorManager::All() const
{
    return m_monitors;
}

}