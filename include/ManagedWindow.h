#pragma once

#include "Types.h"

#include <string>

namespace Kohiko
{

// Everything Kohiko knows about one client window - the spec's list:
// geometry, state, workspace, monitor, title, PID, class, role,
// border, focus, urgent, floating, fullscreen, scratchpad.
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

    // Where a floating window sits/should be restored to - kept
    // separate from Geometry() so toggling fullscreen or re-floating
    // a tiled window can put it back where it was.
    const Rect& FloatingGeometry() const;

    void SetFloatingGeometry(
        const Rect& rect
    );

    void SetTitle(
        const std::string& title
    );

    const std::string& Title() const;

    void SetClassName(
        const std::string& className
    );

    const std::string& ClassName() const;

    void SetInstanceName(
        const std::string& instanceName
    );

    const std::string& InstanceName() const;

    void SetRole(
        const std::string& role
    );

    const std::string& Role() const;

    void SetPid(
        long pid
    );

    long Pid() const;

    void SetWorkspace(
        int workspace
    );

    int Workspace() const;

    void SetMonitor(
        int monitor
    );

    int Monitor() const;

    void SetFocused(
        bool focused
    );

    bool Focused() const;

    void SetUrgent(
        bool urgent
    );

    bool Urgent() const;

    void SetState(
        WindowState state
    );

    WindowState State() const;

    // What State() was before it got overridden by Fullscreen, so
    // ToggleFullscreen(off) knows what to go back to.
    void SetPreviousState(
        WindowState state
    );

    WindowState PreviousState() const;

    bool IsTiled() const;

    bool IsFloating() const;

    bool IsFullscreen() const;

    bool IsScratchpad() const;

    void SetBorderWidth(
        int width
    );

    int BorderWidth() const;

    // Called right before WindowManager unmaps this window itself
    // (workspace switch, scratchpad hide) so the resulting UnmapNotify
    // isn't mistaken for the client withdrawing/closing.
    void IgnoreNextUnmap();

    // Returns true (and consumes one) if an ignored unmap was pending.
    bool ConsumeIgnoredUnmap();

private:

    WindowID m_id;

    Rect m_geometry;
    Rect m_floatingGeometry;

    std::string m_title;
    std::string m_className;
    std::string m_instanceName;
    std::string m_role;
    long m_pid = 0;

    int m_workspace = 1;
    int m_monitor = 0;

    bool m_focused = false;
    bool m_urgent = false;

    WindowState m_state = WindowState::Tiled;
    WindowState m_previousState = WindowState::Tiled;

    int m_borderWidth = 0;

    int m_ignoredUnmaps = 0;

};

}
