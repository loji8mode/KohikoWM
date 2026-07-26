#include "Scratchpad.h"

namespace Kohiko
{

bool Scratchpad::HasWindow() const
{
    return m_window != 0;
}

WindowID Scratchpad::Window() const
{
    return m_window;
}

bool Scratchpad::IsVisible() const
{
    return m_visible;
}

void Scratchpad::Assign(
    WindowID window)
{
    if (HasWindow())
        return;

    m_window = window;
    m_visible = false;
}

void Scratchpad::Forget(
    WindowID window)
{
    if (m_window != window)
        return;

    m_window = 0;
    m_visible = false;
}

bool Scratchpad::Toggle()
{
    if (!HasWindow())
        return false;

    m_visible = !m_visible;

    return m_visible;
}

}
