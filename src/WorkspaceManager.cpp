#include "WorkspaceManager.h"

namespace Kohiko
{

WorkspaceManager::WorkspaceManager()
{
    for (int i = 1; i <= 10; ++i)
    {
        m_workspaces.push_back(
            std::make_unique<Workspace>(i)
        );
    }
}

Workspace&
WorkspaceManager::Current()
{
    return *m_workspaces[
        static_cast<std::size_t>(
            m_current - 1)];
}

const Workspace&
WorkspaceManager::Current() const
{
    return *m_workspaces[
        static_cast<std::size_t>(
            m_current - 1)];
}

Workspace&
WorkspaceManager::Get(
    int id)
{
    return *m_workspaces[
        static_cast<std::size_t>(
            id - 1)];
}

void WorkspaceManager::Switch(
    int id)
{
    if (id < 1)
        return;

    if (id > static_cast<int>(
            m_workspaces.size()))
        return;

    m_current = id;
}

}