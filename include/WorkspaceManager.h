#pragma once

#include "Workspace.h"

#include <memory>
#include <vector>

namespace Kohiko
{

class WorkspaceManager
{
public:

    WorkspaceManager();

    Workspace& Current();

    const Workspace& Current() const;

    Workspace& Get(
        int id
    );

    void Switch(
        int id
    );

private:

    std::vector<
        std::unique_ptr<Workspace>
    > m_workspaces;

    int m_current = 1;

};

}