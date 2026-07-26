#pragma once

#include "Workspace.h"

#include <memory>
#include <vector>

namespace Kohiko
{

// Owns every workspace for the lifetime of the process (fixed count,
// set once from `workspace.count`, matching every other tiling WM's
// "workspaces are always there, whether or not anything is currently
// showing them" model).
//
// With multiple monitors there is no single "current" workspace
// anymore - each Monitor owns its own currently-displayed one (see
// Monitor::ActiveWorkspace()/WindowManager::SwitchWorkspaceOnMonitor())
// and several can be visible at once. This class deliberately no
// longer tracks any notion of a global current/previous workspace -
// it is just the array of Workspace objects (and their independent
// BSP trees) that every Monitor's ActiveWorkspace() points into.
class WorkspaceManager
{
public:

    explicit WorkspaceManager(
        int count = 10
    );

    int Count() const;

    // Clamped to [1, Count()] - never out of range, never null.
    Workspace& Get(
        int id
    );

    const Workspace& Get(
        int id
    ) const;

private:

    std::vector<
        std::unique_ptr<Workspace>
    > m_workspaces;

};

}
