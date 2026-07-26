#pragma once

#include "ManagedWindow.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace Kohiko
{

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

    void ClearFocus();

private:

    std::unordered_map
    <
        WindowID,
        std::unique_ptr<ManagedWindow>
    > m_windows;

};

}