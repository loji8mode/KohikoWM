#include "MouseManager.h"

#include "Config.h"
#include "KeyCombo.h"
#include "Utils.h"
#include "WindowManager.h"
#include "XConnection.h"

namespace Kohiko
{

MouseManager::MouseManager(
    XConnection& connection,
    WindowManager& windowManager)
    :
    m_connection(connection),
    m_windowManager(windowManager)
{
}

void MouseManager::Configure(
    const Config& config)
{
    m_connection.UngrabAllButtonsOnRoot();

    auto parse = [this](const std::string& text)
    {
        Binding binding;
        ParsedCombo combo = ParseCombo(text);

        if (combo.valid)
        {
            binding.modifiers = combo.modifiers;
            binding.button = ParseButton(combo.token);
        }

        return binding;
    };

    m_swapBinding   = parse(config.GetString("mouse.swap",   "SUPER+BTN1"));
    m_resizeBinding = parse(config.GetString("mouse.resize", "SUPER+BTN3"));

    long eventMask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    if (m_swapBinding.button != 0)
    {
        for (unsigned int variant : m_connection.LockVariants(m_swapBinding.modifiers))
            m_connection.GrabButtonOnRoot(m_swapBinding.button, variant, eventMask);
    }

    if (m_resizeBinding.button != 0)
    {
        for (unsigned int variant : m_connection.LockVariants(m_resizeBinding.modifiers))
            m_connection.GrabButtonOnRoot(m_resizeBinding.button, variant, eventMask);
    }
}

void MouseManager::HandlePress(
    const XButtonEvent& event)
{
    Point point{event.x_root, event.y_root};

    if (Matches(m_swapBinding, event.state, event.button))
    {
        m_dragWindow = m_windowManager.WindowAt(point);
        m_lastX = event.x_root;
        m_lastY = event.y_root;

        if (!m_dragWindow)
            return; // nothing under the cursor to pick up

        m_mode = DragMode::Swap;
        m_windowManager.BeginSwapDrag(m_dragWindow, point);
        return;
    }

    if (Matches(m_resizeBinding, event.state, event.button))
    {
        m_dragWindow = m_windowManager.WindowAt(point);
        m_lastX = event.x_root;
        m_lastY = event.y_root;

        if (!m_dragWindow)
            return;

        m_mode = DragMode::Resize;
        m_windowManager.SetResizingCursor(true);
        return;
    }
}

void MouseManager::HandleMotion(
    const XMotionEvent& event)
{
    if (m_mode == DragMode::Idle || !m_dragWindow)
        return;

    Point point{event.x_root, event.y_root};

    if (m_mode == DragMode::Swap)
    {
        // The picked-up window just follows the cursor 1:1 here -
        // nothing else moves, and no swap happens, until the button
        // is released. "Where it lands" is decided once, at drop
        // time, in WindowManager::EndSwapDrag().
        m_windowManager.UpdateSwapDrag(m_dragWindow, point);
    }
    else if (m_mode == DragMode::Resize)
    {
        int dx = event.x_root - m_lastX;
        int dy = event.y_root - m_lastY;

        m_windowManager.ResizeWindow(m_dragWindow, dx, dy);
    }

    m_lastX = event.x_root;
    m_lastY = event.y_root;
}

void MouseManager::HandleRelease(
    const XButtonEvent& event)
{
    if (m_mode == DragMode::Swap && m_dragWindow)
    {
        Point point{event.x_root, event.y_root};
        m_windowManager.EndSwapDrag(m_dragWindow, point);
    }
    else if (m_mode == DragMode::Resize)
    {
        m_windowManager.SetResizingCursor(false);
    }

    m_mode = DragMode::Idle;
    m_dragWindow = nullptr;
}

bool MouseManager::Matches(
    const Binding& binding,
    unsigned int state,
    unsigned int button) const
{
    if (binding.button == 0 || binding.button != button)
        return false;

    static constexpr unsigned int kRelevantMask =
        ShiftMask | ControlMask | Mod1Mask | Mod4Mask;

    return (state & kRelevantMask) == binding.modifiers;
}

unsigned int MouseManager::ParseButton(
    const std::string& token) const
{
    std::string lower = Utils::Lower(token);

    if (lower.rfind("btn", 0) != 0 || lower.size() <= 3)
        return 0;

    try
    {
        switch (std::stoi(lower.substr(3)))
        {
            case 1: return Button1;
            case 2: return Button2;
            case 3: return Button3;
            case 4: return Button4;
            case 5: return Button5;
            default: return 0;
        }
    }
    catch (...)
    {
        return 0;
    }
}

}
