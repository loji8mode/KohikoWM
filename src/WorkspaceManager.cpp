#include "WorkspaceManager.h"

#include <algorithm>

namespace Kohiko
{

WorkspaceManager::WorkspaceManager(
    int count)
{
    if (count < 1)
        count = 1;

    for (int i = 1; i <= count; ++i)
        m_workspaces.push_back(std::make_unique<Workspace>(i));
}

int WorkspaceManager::Count() const
{
    return static_cast<int>(m_workspaces.size());
}

Workspace& WorkspaceManager::Current()
{
    return *m_workspaces[static_cast<std::size_t>(m_current - 1)];
}

const Workspace& WorkspaceManager::Current() const
{
    return *m_workspaces[static_cast<std::size_t>(m_current - 1)];
}

Workspace& WorkspaceManager::Get(
    int id)
{
    id = std::clamp(id, 1, static_cast<int>(m_workspaces.size()));

    return *m_workspaces[static_cast<std::size_t>(id - 1)];
}

int WorkspaceManager::CurrentId() const
{
    return m_current;
}

int WorkspaceManager::PreviousId() const
{
    return m_previous;
}

bool WorkspaceManager::Switch(
    int id)
{
    if (id < 1 || id > static_cast<int>(m_workspaces.size()))
        return false;

    if (id == m_current)
        return false;

    m_previous = m_current;
    m_current = id;

    return true;
}

}
