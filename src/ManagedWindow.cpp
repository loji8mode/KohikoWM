#include "ManagedWindow.h"

namespace Kohiko
{

ManagedWindow::ManagedWindow(
    WindowID id)
    :
    m_id(id)
{
}

WindowID ManagedWindow::Id() const
{
    return m_id;
}

const Rect&
ManagedWindow::Geometry() const
{
    return m_geometry;
}

void ManagedWindow::SetGeometry(
    const Rect& rect)
{
    m_geometry = rect;
}

void ManagedWindow::SetTitle(
    const std::string& title)
{
    m_title = title;
}

const std::string&
ManagedWindow::Title() const
{
    return m_title;
}

void ManagedWindow::SetWorkspace(
    int workspace)
{
    m_workspace = workspace;
}

int ManagedWindow::Workspace() const
{
    return m_workspace;
}

void ManagedWindow::SetFocused(
    bool focused)
{
    m_focused = focused;
}

bool ManagedWindow::Focused() const
{
    return m_focused;
}

void ManagedWindow::SetState(
    WindowState state)
{
    m_state = state;
}

WindowState ManagedWindow::State() const
{
    return m_state;
}

bool ManagedWindow::IsFloating() const
{
    return m_state ==
        WindowState::Floating;
}

bool ManagedWindow::IsFullscreen() const
{
    return m_state ==
        WindowState::Fullscreen;
}

bool ManagedWindow::IsScratchpad() const
{
    return m_state ==
        WindowState::Scratchpad;
}

}