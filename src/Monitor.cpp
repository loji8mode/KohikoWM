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

void Monitor::SetGeometry(
    const Rect& rect)
{
    m_geometry = rect;
}

const Rect&
Monitor::Geometry() const
{
    return m_geometry;
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