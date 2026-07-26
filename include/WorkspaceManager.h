#pragma once

#include "Workspace.h"

#include <memory>
#include <vector>

namespace Kohiko
{

// Owns every workspace and tracks which one is current vs. previous.
// Actually hiding/showing windows on switch is WindowManager's job
// (it owns the WindowRepository this needs to touch); this class is
// just the bookkeeping of "which id is active right now".
class WorkspaceManager
{
public:

    explicit WorkspaceManager(
        int count = 10
    );

    int Count() const;

    Workspace& Current();

    const Workspace& Current() const;

    Workspace& Get(
        int id
    );

    int CurrentId() const;

    int PreviousId() const;

    // Returns false (and does nothing) if `id` is out of range or is
    // already the current workspace.
    bool Switch(
        int id
    );

private:

    std::vector<
        std::unique_ptr<Workspace>
    > m_workspaces;

    int m_current = 1;

    int m_previous = 1;

};

}
