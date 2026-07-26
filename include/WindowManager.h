#pragma once

#include "Bar.h"
#include "CursorManager.h"
#include "EventDispatcher.h"
#include "IPCServer.h"
#include "KeyboardManager.h"
#include "LayoutEngine.h"
#include "MonitorManager.h"
#include "MouseManager.h"
#include "Scratchpad.h"
#include "Types.h"
#include "WindowRepository.h"
#include "WorkspaceManager.h"
#include "XAtoms.h"

#include <X11/Xlib.h>

#include <string>

namespace Kohiko
{

class XConnection;
class Config;
class ManagedWindow;
struct Command;

// The coordinator. Owns every other manager and wires X11 events to
// them, but does no geometry math itself (LayoutEngine) and no tree
// surgery itself (BSPTree) - it just sequences calls like the spec's
// examples:
//
//   MapRequest -> Create ManagedWindow -> Workspace.Insert() -> Layout() -> Focus()
//   Destroy    -> Workspace.Remove() -> Layout()
//   Workspace switch -> Hide windows -> Show windows -> Layout()
class WindowManager
{
public:

    WindowManager(
        XConnection& connection,
        Config& config,
        std::string configPath
    );

    void Initialize();

    void Shutdown();

    bool IsRunning() const;

    EventDispatcher& Dispatcher();

    IPCServer& Ipc();

    // Called roughly once a second by EventLoop so the bar's clock
    // keeps ticking even with no X11/IPC activity.
    void Tick();

    // --- X11 event handlers (called by EventDispatcher) -------------------

    void HandleMapRequest(const XMapRequestEvent& event);
    void HandleConfigureRequest(const XConfigureRequestEvent& event);
    void HandleUnmapNotify(const XUnmapEvent& event);
    void HandleDestroyNotify(const XDestroyWindowEvent& event);
    void HandleEnterNotify(const XCrossingEvent& event);
    void HandlePropertyNotify(const XPropertyEvent& event);
    void HandleButtonPressOnClient(const XButtonEvent& event);
    void HandleExpose(const XExposeEvent& event);

    bool IsManaged(WindowID id) const;

    // --- Command execution (keybinds + `kohikoctl dispatch ...`) ----------

    void Execute(const Command& command);

    // --- MouseManager-facing API --------------------------------------------

    ManagedWindow* WindowAt(const Point& point);
    void SwapWindows(ManagedWindow* first, ManagedWindow* second);
    void ResizeWindow(ManagedWindow* window, int dx, int dy);
    void SetResizingCursor(bool resizing);

    // --- IPCServer-facing API ------------------------------------------------

    std::string HandleIpcCommand(const std::string& request);

private:

    void Manage(WindowID id);
    void Unmanage(WindowID id);

    void Focus(WindowID id);
    void FocusNextAvailable();

    void Arrange();

    void SwitchWorkspace(int id);
    void MoveFocusedToWorkspace(int id);

    void ToggleFloating();
    void ToggleFullscreen();
    void ToggleScratchpadForFocused();
    void CloseFocused();
    void FocusDirection(Direction direction);
    void RotateFocused();
    void FlipFocused();
    void ReloadConfig();

    Rect CenteredFloatingRect(float widthFraction, float heightFraction);
    unsigned long ParseColor(const std::string& key, const std::string& fallback) const;

    std::string DumpClientsJson() const;
    std::string DumpMonitorsJson() const;
    std::string DumpActiveWindowJson() const;

private:

    XConnection& m_connection;
    Config& m_config;
    std::string m_configPath;

    XAtoms m_atoms;
    CursorManager m_cursor;
    WindowRepository m_repository;
    WorkspaceManager m_workspaces;
    MonitorManager m_monitors;
    LayoutEngine m_layout;
    Scratchpad m_scratchpad;

    KeyboardManager m_keyboard;
    MouseManager m_mouse;
    EventDispatcher m_dispatcher;

    IPCServer m_ipc;
    Bar m_bar;

    bool m_running = true;

    // Last real pointer position we saw via EnterNotify, so we can
    // tell a genuine mouse-entered-this-window crossing apart from a
    // *window* moving to sit under an unmoved pointer (which X11 also
    // reports as EnterNotify, and would otherwise steal focus every
    // time a layout change happens to land a different window under
    // wherever the cursor happens to be resting).
    int m_lastPointerX = -1;
    int m_lastPointerY = -1;

};

}
