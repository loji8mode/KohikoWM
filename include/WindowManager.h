#pragma once

#include "Animator.h"
#include "Bar.h"
#include "CursorManager.h"
#include "EventDispatcher.h"
#include "IPCServer.h"
#include "KeyboardManager.h"
#include "Launcher.h"
#include "LayoutEngine.h"
#include "MonitorManager.h"
#include "MouseManager.h"
#include "Notepad.h"
#include "Scratchpad.h"
#include "SystemTray.h"
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

    // Called by EventLoop: roughly once a second when idle so the
    // bar's clock keeps ticking with no X11/IPC activity, or as often
    // as ~120Hz while a Swap-drop animation is in flight (see
    // HasActiveAnimation()).
    void Tick();

    // Tells EventLoop whether to shorten its select() timeout so a
    // Swap-drop animation plays smoothly instead of idling at ~1Hz.
    bool HasActiveAnimation() const;

    // --- X11 event handlers (called by EventDispatcher) -------------------

    void HandleMapRequest(const XMapRequestEvent& event);
    void HandleConfigureRequest(const XConfigureRequestEvent& event);
    void HandleUnmapNotify(const XUnmapEvent& event);
    void HandleDestroyNotify(const XDestroyWindowEvent& event);
    void HandleEnterNotify(const XCrossingEvent& event);
    void HandlePropertyNotify(const XPropertyEvent& event);
    void HandleButtonPressOnClient(const XButtonEvent& event);
    void HandleExpose(const XExposeEvent& event);
    void HandleClientMessage(const XClientMessageEvent& event);
    void HandleLauncherButtonPress(const XButtonEvent& event);
    ::Window LauncherWindowId() const;

    // Gives the Launcher/Notepad first refusal on every KeyPress while
    // either is open, returning true if it consumed the event (which
    // is always, while one is open - see the class comment on
    // Launcher for why this never touches an X11 keyboard grab).
    // EventDispatcher falls through to the normal KeyboardManager path
    // only when this returns false.
    bool HandleModalKeyPress(const XKeyEvent& event);

    bool IsManaged(WindowID id) const;

    // --- Command execution (keybinds + `kohikoctl dispatch ...`) ----------

    void Execute(const Command& command);

    // --- MouseManager-facing API --------------------------------------------

    ManagedWindow* WindowAt(const Point& point);
    void SwapWindows(ManagedWindow* first, ManagedWindow* second);
    void ResizeWindow(ManagedWindow* window, int dx, int dy);
    void SetResizingCursor(bool resizing);

    // Super+LMB drag lifecycle - see MouseManager's class comment for
    // the press/motion/release sequencing that drives these.
    void BeginSwapDrag(ManagedWindow* window, const Point& cursor);
    void UpdateSwapDrag(ManagedWindow* window, const Point& cursor);
    void EndSwapDrag(ManagedWindow* window, const Point& cursor);

    // --- IPCServer-facing API ------------------------------------------------

    std::string HandleIpcCommand(const std::string& request);
    std::string m_fileManager;

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

    void ToggleLauncher();
    void CloseLauncher(bool run);
    void ToggleNotepad();
    void CloseNotepad();
    void RestoreFocusAfterModal();

    // The monitor area actually available for tiling (primary monitor
    // minus the bar), shared by Arrange() and the capacity checks
    // below so they can never disagree about what "fits".
    Rect TilingArea();

    // Bug #4: before Insert()-ing a new tile, checks whether doing so
    // would shrink some area below the configured minimum. TryTile()
    // is the single choke point every tiling insertion goes through
    // (Manage(), MoveFocusedToWorkspace(), ToggleFloating()); it only
    // mutates `window` on success. FindWorkspaceWithRoom() is Manage()'s
    // fallback search across the *other* workspaces when the current
    // one is full.
    bool TryTile(ManagedWindow* window, int workspaceId);
    int FindWorkspaceWithRoom(int excludeId);

    // Recomputes and applies `window`'s border colour from its own
    // current focused/unfocused state - shared by Focus() (repainting
    // every window's border after a focus change) and the Swap-drag
    // hover highlight (restoring a window's real colour once it's no
    // longer the hovered drop target).
    void RefreshBorderColor(ManagedWindow* window);

    // Recomputes and caches node/window geometry for `workspaceId`'s
    // tree without touching X11 window mapping - used right after
    // tiling onto a workspace that isn't the current one (Arrange()
    // only ever lays out the current workspace). Without this, a
    // workspace built up entirely "in the background" would keep
    // every node's cached Geometry() at its default {0,0,0,0}, which
    // would make Insert()'s DirectionForRect(anchor->Geometry()) see
    // a 0x0 rect and always default to a Vertical split - silently
    // chaining every subsequent window into a narrower and narrower
    // strip regardless of what TryTile()'s capacity check approved.
    void RefreshWorkspaceGeometry(int workspaceId);

    // Keeps the root window's _NET_CLIENT_LIST in sync with
    // m_repository - called from Manage()/Unmanage() every time a
    // window starts or stops being managed, so EWMH-aware tools (see
    // XConnection::InitializeEwmhSupport()) always see the current set.
    void RefreshClientList();

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

    // The invisible window WindowManager::Initialize() creates via
    // XConnection::InitializeEwmhSupport() to advertise EWMH support -
    // see that method's comment for why tools like flameshot check it.
    // Kohiko never touches it again after startup, but keeps it around
    // for the lifetime of the connection rather than letting it dangle.
    ::Window m_ewmhCheckWindow = 0;

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
    SystemTray m_tray;
    Launcher m_launcher;
    Notepad m_notepad;
    Animator m_animator;

    bool m_running = true;

    // Last real pointer position we saw via EnterNotify, so we can
    // tell a genuine mouse-entered-this-window crossing apart from a
    // *window* moving to sit under an unmoved pointer (which X11 also
    // reports as EnterNotify, and would otherwise steal focus every
    // time a layout change happens to land a different window under
    // wherever the cursor happens to be resting).
    int m_lastPointerX = -1;
    int m_lastPointerY = -1;

    // Whichever window is currently being followed by the cursor
    // during a Super+LMB Swap drag (nullptr the rest of the time).
    // Arrange() skips repositioning this one window so an unrelated
    // relayout mid-drag can't yank it back to its old tiled slot out
    // from under the cursor.
    ManagedWindow* m_activeDragWindow = nullptr;
    ManagedWindow* m_dragHoverTarget = nullptr;
    int m_dragOffsetX = 0;
    int m_dragOffsetY = 0;
    Rect m_dragCurrentRect;

    // Whatever was focused right before the Launcher/Notepad opened,
    // so closing either one gives focus back rather than leaving
    // whatever the mouse happens to be over focused instead.
    WindowID m_focusBeforeModal = 0;

};

}
