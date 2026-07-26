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

Workspace& WorkspaceManager::Get(
    int id)
{
    id = std::clamp(id, 1, static_cast<int>(m_workspaces.size()));

    return *m_workspaces[static_cast<std::size_t>(id - 1)];
}

const Workspace& WorkspaceManager::Get(
    int id) const
{
    id = std::clamp(id, 1, static_cast<int>(m_workspaces.size()));

    return *m_workspaces[static_cast<std::size_t>(id - 1)];
}

}
