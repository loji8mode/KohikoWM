#include "Scratchpad.h"

#include "WindowRepository.h"
#include "ManagedWindow.h"

namespace Kohiko
{

Scratchpad::Scratchpad(
    WindowRepository& repository)
    :
    m_repository(repository)
{
}

void Scratchpad::Add(
    WindowID window)
{
    m_window = window;
}

bool Scratchpad::Contains(
    WindowID window) const
{
    return m_window == window;
}

void Scratchpad::Toggle()
{
    if(m_window == 0)
        return;

    auto* managed =
        m_repository.Get(m_window);

    if(!managed)
        return;

    if(m_visible)
    {
        managed->SetState(
            WindowState::Scratchpad);

        m_visible = false;
    }
    else
    {
        managed->SetState(
            WindowState::Floating);

        m_visible = true;
    }
}

}