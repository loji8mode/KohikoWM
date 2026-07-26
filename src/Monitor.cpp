#include "Monitor.h"

#include "Workspace.h"

namespace Kohiko
{

Monitor::Monitor(
    int id)
    :
    m_id(id)
{
}

int Monitor::Id() const
{
    return m_id;
}

void Monitor::SetId(
    int id)
{
    m_id = id;
}

const std::string& Monitor::Name() const
{
    return m_name;
}

void Monitor::SetName(
    const std::string& name)
{
    m_name = name;
}

bool Monitor::Connected() const
{
    return m_connected;
}

void Monitor::SetConnected(
    bool connected)
{
    m_connected = connected;
}

bool Monitor::IsPrimary() const
{
    return m_primary;
}

void Monitor::SetPrimary(
    bool primary)
{
    m_primary = primary;
}

void Monitor::SetGeometry(
    const Rect& rect)
{
    m_geometry = rect;

    // A monitor that's never had SetWorkArea() called on it yet (a
    // freshly-connected one, or before WindowManager's first
    // RefreshMonitorWorkAreas() pass) should still report *something*
    // sane rather than an empty {0,0,0,0} - the full geometry is
    // exactly what it'll be once that first pass runs anyway, for
    // every monitor that isn't hosting the bar.
    if (m_workArea.width <= 0 || m_workArea.height <= 0)
        m_workArea = rect;
}

const Rect&
Monitor::Geometry() const
{
    return m_geometry;
}

void Monitor::SetWorkArea(
    const Rect& rect)
{
    m_workArea = rect;
}

const Rect&
Monitor::WorkArea() const
{
    return m_workArea;
}

void Monitor::SetWorkspace(
    Workspace* workspace)
{
    m_workspace = workspace;
}

Workspace*
Monitor::ActiveWorkspace() const
{
    return m_workspace;
}

}
