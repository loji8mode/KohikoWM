#pragma once

#include "Types.h"

#include <string>

namespace Kohiko
{

class ManagedWindow
{
public:

    explicit ManagedWindow(
        WindowID id
    );

    WindowID Id() const;

    const Rect& Geometry() const;

    void SetGeometry(
        const Rect& rect
    );

    void SetTitle(
        const std::string& title
    );

    const std::string& Title() const;

    void SetWorkspace(
        int workspace
    );

    int Workspace() const;

    void SetFocused(
        bool focused
    );

    bool Focused() const;

    void SetState(
        WindowState state
    );

    WindowState State() const;

    bool IsFloating() const;

    bool IsFullscreen() const;

    bool IsScratchpad() const;

private:

    WindowID m_id;

    Rect m_geometry;

    std::string m_title;

    int m_workspace = 1;

    bool m_focused = false;

    WindowState m_state =
        WindowState::Tiled;

};

}