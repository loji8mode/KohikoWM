#pragma once

#include "ManagedWindow.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace Kohiko
{

// Owns every ManagedWindow and answers the "which windows..." queries
// the rest of the code needs, so nobody else has to loop over every
// window by hand: Focused(), Floating(), Scratchpad(), Workspace(),
// Visible().
class WindowRepository
{
public:

    ManagedWindow* Add(
        WindowID id
    );

    void Remove(
        WindowID id
    );

    ManagedWindow* Get(
        WindowID id
    );

    bool Contains(
        WindowID id
    ) const;

    std::vector<ManagedWindow*> All() const;

    std::vector<ManagedWindow*> Workspace(
        int workspace
    ) const;

    ManagedWindow* Focused() const;

    std::vector<ManagedWindow*> Floating() const;

    std::vector<ManagedWindow*> Scratchpad() const;

    // Tiled + floating windows that belong on `workspace` and should
    // therefore be mapped when it is the active one (scratchpad is
    // excluded - its visibility is independent of workspace switches).
    std::vector<ManagedWindow*> Visible(
        int workspace
    ) const;

    void ClearFocus();

private:

    std::unordered_map
    <
        WindowID,
        std::unique_ptr<ManagedWindow>
    > m_windows;

};

}
