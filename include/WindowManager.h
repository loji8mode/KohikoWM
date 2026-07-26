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
#include "WindowRule.h"
#include "WorkspaceManager.h"
#include "XAtoms.h"

#include <X11/Xlib.h>

#include <string>
#include <vector>

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
//
// Multi-monitor: each Monitor (MonitorManager.h) owns exactly one
// ActiveWorkspace() at a time, and no two monitors ever show the same
// one - so "the current workspace" is no longer a single global
// notion. Every place that used to mean that now means "the focused
// monitor's active workspace" (FocusedMonitor(), FocusedWorkspaceId())
// for keyboard commands, or "whichever monitor a given window/
// workspace is actually on" (MonitorShowing()) for placement/
// relocation logic. Arrange() is the one place that still walks every
// monitor at once - see its comment.
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

    // Second line of defence against focus-stealing while the
    // Launcher/Notepad is open (the first is Manage() not calling
    // Focus() on a newly-mapped window in the first place) - catches
    // any window, however it got there, ending up with real X input
    // focus while a modal should have it instead, and immediately
    // hands focus back. See EventDispatcher's FocusIn case for why
    // this always fires.
    void HandleFocusIn(const XFocusChangeEvent& event);
    void HandleButtonPressOnClient(const XButtonEvent& event);
    void HandleExpose(const XExposeEvent& event);
    void HandleClientMessage(const XClientMessageEvent& event);
    void HandleLauncherButtonPress(const XButtonEvent& event);
    ::Window LauncherWindowId() const;

    // XRandR's event numbers are only known at runtime (see
    // MonitorManager::Initialize()), not fixed constants like every
    // core X11 event - EventDispatcher checks this before its normal
    // event.type switch, and routes anything it accepts to
    // HandleMonitorEvent() instead.
    bool IsMonitorEvent(const XEvent& event) const;
    void HandleMonitorEvent(const XEvent& event);

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

    // Focuses the front visible window on `monitor`'s active
    // workspace, or clears X input focus entirely if it has none -
    // the same "what should have focus right now" fallback used after
    // a workspace switch, a window closing, a Swap drag ending
    // somewhere empty, and so on. FocusNextAvailable() is just this
    // for FocusedMonitor(), which is what every one of those call
    // sites actually wants; FocusNextAvailableOn() exists separately
    // for the handful of places (closing/moving a window that lives
    // on a monitor other than the focused one) that need to fix up
    // *that* monitor's focus instead, without stealing real input
    // focus away from whatever the user is actually looking at.
    void FocusNextAvailable();
    void FocusNextAvailableOn(Monitor& monitor);

    // Lays out and maps/unmaps every monitor's active workspace in
    // one pass - the only place that still needs to think about every
    // monitor at once, since it's also what decides which windows are
    // visible *anywhere* right now (anything not on some monitor's
    // ActiveWorkspace() gets unmapped) and which aren't. Every other
    // method that changes what should be visible (a workspace switch,
    // a window moving to another workspace/monitor, a hotplug, ...)
    // just mutates state and calls this - it never needs to reason
    // about mapping/unmapping itself. See ArrangeMonitor() for the
    // actual per-monitor layout work this delegates to.
    void Arrange();
    void ArrangeMonitor(Monitor& monitor);

    // The monitor keyboard commands/new windows operate on right now.
    // Always valid (MonitorManager never leaves this null once
    // Initialize() has run).
    Monitor& FocusedMonitor() const;

    // FocusedMonitor()'s ActiveWorkspace()'s id - shorthand for the
    // single most common thing callers actually want.
    int FocusedWorkspaceId() const;

    // Whichever connected monitor currently has `workspaceId` as its
    // ActiveWorkspace(), or nullptr if it's a background workspace
    // nothing is showing right now.
    Monitor* MonitorShowing(int workspaceId) const;

    // Shorthand for MonitorShowing(workspaceId) != nullptr - reads
    // better at call sites that only care about the yes/no.
    bool IsWorkspaceVisible(int workspaceId) const;

    // Moves keyboard/placement focus to `monitor` itself (Super+drag
    // and click-to-focus already do this implicitly via Focus(), see
    // HandleEnterNotify()/HandleButtonPressOnClient(); this is the
    // explicit `focusmonitor` command's target) - focuses whatever was
    // last visible there, or clears focus if it has nothing.
    void FocusMonitor(Monitor& monitor);
    void FocusMonitorCommand(const std::string& arg);

    // Switches `monitor`'s own active workspace to `id`, leaving
    // every other monitor's completely untouched - see the class
    // comment on why this replaced the old single global "current
    // workspace" model. If `id` is already active on some *other*
    // monitor, the two monitors swap active workspaces instead of
    // ever showing the same one twice (matches i3's `workspace <n>`
    // behaviour) - either way, no workspace ever silently vanishes
    // off-screen just because the id you asked for happened to
    // already be visible somewhere else.
    void SwitchWorkspace(int id);
    void SwitchWorkspaceOnMonitor(Monitor& monitor, int id);

    void MoveFocusedToWorkspace(int id);

    // Moves `window` onto `target` monitor's own active workspace,
    // preserving floating/tiled/fullscreen state - see the class doc
    // and MoveFocusedToMonitorCommand() for the keybind surface.
    void MoveWindowToMonitor(ManagedWindow* window, Monitor& target);
    void MoveFocusedToMonitorCommand(const std::string& arg);

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

    // Applies `keyboard.layouts`/`keyboard.layout_toggle` via
    // setxkbmap - see the class comment on the call site in
    // Initialize() for why this makes every listed language work in
    // every window, not just ones Kohiko manages. Safe to call again
    // on `reload` (setxkbmap is idempotent), unlike RunAutostart().
    void ApplyKeyboardLayouts();

    // Launches every program listed in `auto_start_programs` exactly
    // once, right after startup - see Initialize(). Deliberately never
    // called from ReloadConfig(), or `kohikoctl reload` would relaunch
    // every autostart program (a second Telegram, a second Discord, ...)
    // every time someone reloads the config.
    void RunAutostart();

    // The area actually available for tiling on `monitor` - its own
    // WorkArea() (Geometry() minus the bar, for whichever monitor is
    // hosting it - see RefreshMonitorWorkAreas()) - shared by
    // ArrangeMonitor() and the capacity checks below so they can
    // never disagree about what "fits".
    Rect TilingArea(Monitor& monitor);

    // Recomputes every monitor's WorkArea() from its Geometry() minus
    // whatever screen-edge furniture Kohiko reserves there (currently
    // just the bar, and currently only ever on Primary() - see Bar.h).
    // Called at the top of Arrange() and whenever the bar's configured
    // height might have changed (ReloadConfig(), a hotplug that
    // changed which monitor is Primary()).
    void RefreshMonitorWorkAreas();

    // Bug #4: before Insert()-ing a new tile, checks whether doing so
    // would shrink some area below the configured minimum. TryTile()
    // is the single choke point every tiling insertion goes through
    // (Manage(), MoveFocusedToWorkspace(), MoveWindowToMonitor(),
    // ToggleFloating()); it only mutates `window` on success.
    // `referenceMonitor` is whichever monitor's WorkArea() the
    // capacity math should be sized against - the monitor `window` is
    // actually landing on when it's currently visible anywhere, or a
    // best-effort guess (whichever monitor asked for the tile) for a
    // purely background workspace nothing is displaying yet; see
    // BSPTree::Insert()'s comment for why a workspace's ratios adapt
    // to whatever monitor eventually shows it regardless; this only
    // affects the *absolute*-pixel accept/reject decision, never the
    // actual on-screen result once something real. FindWorkspaceWithRoom()
    // is Manage()'s fallback search across the *other* workspaces when
    // the requested one is full.
    bool TryTile(ManagedWindow* window, int workspaceId, Monitor& referenceMonitor);
    int FindWorkspaceWithRoom(ManagedWindow* window, int excludeId, Monitor& referenceMonitor);

    // `new_workspace` fallback destination: whichever *other*
    // workspace currently has the fewest windows on it (ties broken
    // by lowest id) - unlike FindWorkspaceWithRoom() above, this
    // doesn't care whether that workspace has tiling capacity, since
    // the whole point of this fallback is landing the window
    // somewhere floating, not necessarily tiled. Returns 0 if
    // `excludeId` is the only workspace that exists.
    int FindWorkspaceWithFewestWindows(int excludeId);

    // Called once a window's TilingMisbehaviorCount() reaches
    // general.tiling_misbehavior_threshold: pulls it out of its
    // workspace's BSP tree (if it's in one) and lands it floating -
    // on the same workspace, or on whichever has the fewest windows,
    // per general.tiling_misbehavior_fallback - and never puts it
    // back into any tile as part of this call. That last part is
    // deliberate: a window that's already proven it fights tiled
    // geometry would just as easily fight a *different* tile, so
    // unlike Manage()'s own "nowhere had room" fallback, this one
    // always floats rather than trying TryTile() again anywhere.
    void ApplyTilingMisbehaviorFallback(ManagedWindow* window);

    // Recomputes and applies `window`'s border colour from its own
    // current focused/unfocused state - shared by Focus() (repainting
    // every window's border after a focus change) and the Swap-drag
    // hover highlight (restoring a window's real colour once it's no
    // longer the hovered drop target).
    void RefreshBorderColor(ManagedWindow* window);

    // Recomputes and caches node/window geometry for `workspaceId`'s
    // tree - against `referenceMonitor`'s WorkArea() - without
    // touching X11 window mapping. Used right after tiling onto a
    // workspace that isn't currently visible on any monitor
    // (ArrangeMonitor() only ever lays out a workspace that IS one's
    // ActiveWorkspace()). Without this, a workspace built up entirely
    // "in the background" would keep every node's cached Geometry()
    // at its default {0,0,0,0}, which would make Insert()'s
    // DirectionForRect(anchor->Geometry()) see a 0x0 rect and always
    // default to a Vertical split - silently chaining every
    // subsequent window into a narrower and narrower strip regardless
    // of what TryTile()'s capacity check approved.
    void RefreshWorkspaceGeometry(int workspaceId, Monitor& referenceMonitor);

    // Keeps the root window's _NET_CLIENT_LIST in sync with
    // m_repository - called from Manage()/Unmanage() every time a
    // window starts or stops being managed, so EWMH-aware tools (see
    // XConnection::InitializeEwmhSupport()) always see the current set.
    void RefreshClientList();

    // `monitor` is whichever one the resulting rect should be
    // centered/clamped against - the window's own target monitor
    // (FocusedMonitor() for a fresh top-level window, the parent's
    // monitor for a transient, the destination monitor for
    // MoveWindowToMonitor(), ...), never assumed to be Primary().
    Rect CenteredFloatingRect(Monitor& monitor, float widthFraction, float heightFraction);

    // What a floating window (a transient/dialog, a `windowrule=float`
    // match, or the "no workspace has room" fallback) should actually
    // be sized to: the client's own requested size where it gave one
    // (WM_NORMAL_HINTS, else its current on-screen size at map time),
    // clamped to comfortably fit `monitor` and centered - never the
    // flat 50%-of-the-screen guess CenteredFloatingRect(monitor, 0.5,
    // 0.5) makes on its own. Falls back to that same 50/50 guess only
    // when the client's own idea of its size is missing or unusably
    // small.
    //
    // `parent` is the managed window this one is WM_TRANSIENT_FOR, if
    // any (nullptr otherwise, e.g. an ordinary `windowrule=float` match
    // or a transient hint that didn't resolve to a window Kohiko
    // manages). When given, the result is centered over `parent`'s own
    // Geometry() instead of `monitor` - "center the child relative to
    // its parent when possible" - and only clamped against `monitor`'s
    // bounds so it can never end up partly off-screen even if the
    // parent itself is sitting near an edge. `monitor` should always
    // be the parent's own monitor when `parent` is given (see
    // MonitorShowing()) - transient/dialog windows always follow
    // their parent's monitor, never the focused one.
    Rect CenteredFloatingRectForWindow(
        WindowID id,
        const XWindowAttributes& attrs,
        Monitor& monitor,
        ManagedWindow* parent = nullptr
    );

    // Re-centers/clamps a floating (or fullscreen-while-floating)
    // window's rect from one monitor's coordinate space into
    // another's, keeping it at the same *relative* position and size
    // where that still fits (a window docked to a corner stays
    // docked to the analogous corner) rather than always recentering
    // outright - used by MoveWindowToMonitor() and by
    // RelocateOrphanedFloatingWindows() after a monitor disconnects
    // out from under a window's old absolute coordinates.
    Rect RelocateRectToMonitor(
        const Rect& rect,
        const Rect& fromArea,
        const Rect& toArea
    ) const;

    // Part of "relocate affected windows safely" (see the class doc
    // and MonitorManager::Detect()'s removal callback): after any
    // monitor topology change, a floating or fullscreen-while-was-
    // floating window whose rect no longer overlaps *any* currently
    // connected monitor (its old monitor is simply gone) is
    // re-homed onto whichever monitor now shows its workspace, or
    // Primary() if that workspace isn't visible anywhere either.
    // Tiled windows need no such fixup - their tree's ratios already
    // adapt to whatever monitor ends up laying them out, exactly like
    // any other background workspace (see BSPTree::Insert()'s
    // comment).
    void RelocateOrphanedFloatingWindows();

    // Called once after MonitorManager::Detect() reports the monitor
    // *set* changed shape (see MonitorManager::Detect()'s return
    // value) - refreshes work areas, relocates anything orphaned by a
    // disconnected monitor, and re-arranges. Also the handler wired
    // up as MonitorManager::SetBeforeMonitorRemovedCallback(), for
    // anything that specifically needs to run *before* a Monitor
    // object is actually destroyed (currently nothing does - see its
    // definition for why the orphan sweep above is enough on its own -
    // but the hook stays available for whatever "relocate affected
    // windows safely" grows to mean later, e.g. once per-monitor bars
    // exist and need tearing down here too).
    void HandleMonitorTopologyChanged();

    unsigned long ParseColor(const std::string& key, const std::string& fallback) const;

    // Every `windowrule=` line whose selector matches this window,
    // collapsed into one effect - see WindowRule.h.
    WindowRuleEffect ResolveWindowRules(
        const std::string& className,
        const std::string& instanceName,
        const std::string& title) const;

    // Re-raises the Launcher/Notepad above whatever Arrange() (or a
    // manual Raise() elsewhere, e.g. the Scratchpad or a Swap drag)
    // just put on top of it. Without this, a window that opens - or
    // any window that goes floating/fullscreen/dragged - while either
    // is up would silently end up stacked *above* it, even though
    // input focus (and the ability to type into it) correctly stayed
    // on the modal the entire time; the two falling out of sync like
    // that is exactly what made it look "stuck behind" the new window.
    // A no-op when neither is open.
    void RaiseModalWindows();

    std::string DumpClientsJson() const;
    std::string DumpMonitorsJson() const;
    std::string DumpActiveWindowJson() const;

private:

    XConnection& m_connection;
    Config& m_config;
    std::string m_configPath;

    XAtoms m_atoms;
    CursorManager m_cursor;

    // Loaded from every `windowrule=` line in the config, refreshed by
    // both Initialize() and ReloadConfig() - see WindowRule.h.
    std::vector<WindowRule> m_windowRules;

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
