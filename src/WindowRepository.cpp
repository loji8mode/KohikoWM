#include "WindowRepository.h"

namespace Kohiko
{

ManagedWindow*
WindowRepository::Add(
    WindowID id)
{
    auto ptr = std::make_unique<ManagedWindow>(id);
    auto* raw = ptr.get();

    m_windows.emplace(id, std::move(ptr));

    return raw;
}

void WindowRepository::Remove(
    WindowID id)
{
    m_windows.erase(id);
}

ManagedWindow*
WindowRepository::Get(
    WindowID id)
{
    auto it = m_windows.find(id);

    if (it == m_windows.end())
        return nullptr;

    return it->second.get();
}

bool WindowRepository::Contains(
    WindowID id) const
{
    return m_windows.find(id) != m_windows.end();
}

std::vector<ManagedWindow*>
WindowRepository::All() const
{
    std::vector<ManagedWindow*> result;
    result.reserve(m_windows.size());

    for (const auto& [id, window] : m_windows)
        result.push_back(window.get());

    return result;
}

std::vector<ManagedWindow*>
WindowRepository::Workspace(
    int workspace) const
{
    std::vector<ManagedWindow*> result;

    for (const auto& [id, window] : m_windows)
    {
        if (window->Workspace() == workspace)
            result.push_back(window.get());
    }

    return result;
}

ManagedWindow*
WindowRepository::Focused() const
{
    for (const auto& [id, window] : m_windows)
    {
        if (window->Focused())
            return window.get();
    }

    return nullptr;
}

std::vector<ManagedWindow*>
WindowRepository::Floating() const
{
    std::vector<ManagedWindow*> result;

    for (const auto& [id, window] : m_windows)
    {
        if (window->IsFloating())
            result.push_back(window.get());
    }

    return result;
}

std::vector<ManagedWindow*>
WindowRepository::Scratchpad() const
{
    std::vector<ManagedWindow*> result;

    for (const auto& [id, window] : m_windows)
    {
        if (window->IsScratchpad())
            result.push_back(window.get());
    }

    return result;
}

std::vector<ManagedWindow*>
WindowRepository::Visible(
    int workspace) const
{
    std::vector<ManagedWindow*> result;

    for (const auto& [id, window] : m_windows)
    {
        if (window->Workspace() != workspace)
            continue;

        if (window->IsScratchpad())
            continue;

        result.push_back(window.get());
    }

    return result;
}

void WindowRepository::ClearFocus()
{
    for (auto& [id, window] : m_windows)
        window->SetFocused(false);
}

}
