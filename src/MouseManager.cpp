#include "MouseManager.h"

#include "WindowManager.h"

namespace Kohiko
{

MouseManager::MouseManager(
    WindowManager& wm)
    :
    m_windowManager(wm)
{
}

void MouseManager::HandleButtonPress(
    const XButtonEvent& event)
{
    if(event.button == Button1)
        m_dragging = true;

    if(event.button == Button3)
        m_resizing = true;
}

void MouseManager::HandleButtonRelease(
    const XButtonEvent&)
{
    m_dragging = false;

    m_resizing = false;
}

void MouseManager::HandleMotion(
    const XMotionEvent&)
{
    if(m_dragging)
    {
        // swap буде реалізовано
        // через BSPTree::Swap()
    }

    if(m_resizing)
    {
        // resize буде реалізовано
        // через BSPTree::Resize()
    }
}

}