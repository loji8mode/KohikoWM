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

#include <map>
#include <memory>
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

    // Ambient pointer tracking (root has PointerMotionMask selected
    // whether or not a Super+drag is in progress - see XConnection::
    // BecomeWindowManager()) - MouseManager calls this itself whenever
    // it sees a MotionNotify while no drag is active, purely so
    // "focused monitor" can follow the cursor across bare desktop
    // between monitors, not just across window boundaries (which
    // HandleEnterNotify already covers). See UpdateFocusedMonitorFromPointer().
    void HandlePointerMotion(const XMotionEvent& event);

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

    // The topmost *floating* window whose rect contains `point`, if
    // any - checked by MouseManager before WindowAt() (which only ever
    // hit-tests the tiled BSP tree) so a Super+LMB press actually picks
    // up a floating window sitting visually on top of a tile, instead
    // of grabbing the tile underneath it. Prefers the currently-focused
    // window if it's a match (the most likely thing the user meant to
    // grab); otherwise approximates "topmost" by the same iteration
    // order ArrangeMonitor() raises floating windows in, since nothing
    // here tracks real X11 stacking order beyond that.
    ManagedWindow* FloatingWindowAt(const Point& point) const;

    void SwapWindows(ManagedWindow* first, ManagedWindow* second);
    void ResizeWindow(ManagedWindow* window, int dx, int dy);
    void SetResizingCursor(bool resizing);

    // Super+LMB drag lifecycle - see MouseManager's class comment for
    // the press/motion/release sequencing that drives these.
    void BeginSwapDrag(ManagedWindow* window, const Point& cursor);
    void UpdateSwapDrag(ManagedWindow* window, const Point& cursor);
    void EndSwapDrag(ManagedWindow* window, const Point& cursor);

    // The floating-window equivalent of the Swap-drag trio above - a
    // plain 1:1 follow-the-cursor move rather than a pick-up-and-swap,
    // since a floating window has no tiled slot to swap. Live monitor
    // crossing (see requirement 3): every UpdateFloatingDrag() checks
    // which monitor the window's center now sits over and re-homes it
    // onto that monitor's own active workspace immediately, without
    // waiting for release - "drag across the boundary, it belongs to
    // the new monitor" - only its on-screen position is left exactly
    // where the cursor put it until EndFloatingDrag() clamps/finalizes it.
    void BeginFloatingDrag(ManagedWindow* window, const Point& cursor);
    void UpdateFloatingDrag(ManagedWindow* window, const Point& cursor);
    void EndFloatingDrag(ManagedWindow* window, const Point& cursor);

    // The floating-window equivalent of RMB resize - ResizeWindow()
    // above only ever means "adjust the BSP divider ratio between two
    // tiled siblings", which has no meaning for a window that isn't in
    // the tree at all (a floating window's own Resize() call on the
    // tree simply finds no leaf and silently no-ops). Instead,
    // whichever edge(s) of the window are closest to the point it was
    // grabbed at grow/shrink live with the cursor, and the opposite
    // edge(s) stay anchored in place, exactly like dragging a border
    // on any other floating window manager - grab near the right edge
    // to resize width only, near a corner to resize both axes at once.
    // Clamped throughout to the window's own declared minimum size
    // (falling back to general.min_tile_width/height, same floor
    // TryTile() enforces) and to the bounds of whichever monitor it's
    // currently on, so it can never be resized down to nothing or out
    // past the edge of the screen.
    void BeginFloatingResize(ManagedWindow* window, const Point& cursor);
    void UpdateFloatingResize(ManagedWindow* window, const Point& cursor);
    void EndFloatingResize(ManagedWindow* window, const Point& cursor);

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

    // Moves keyboard/placement focus to `monitor` itself - the engine
    // behind *every* way focus can move to a different monitor: the
    // pointer entering a window there (HandleEnterNotify()), the
    // pointer merely crossing into its bare desktop
    // (UpdateFocusedMonitorFromPointer(), see below - this is the
    // primary mechanism now; there is deliberately no default keybind
    // for this anymore, see FocusMonitorCommand()'s comment), or the
    // explicit `focusmonitor` command for anyone who still wants to
    // bind one. Focuses whatever was last visible there, or clears
    // focus if it has nothing.
    void FocusMonitor(Monitor& monitor);

    // Resolves `monitor`'s Containing() monitor and calls FocusMonitor()
    // if it's not already the focused one - the single chokepoint both
    // HandleEnterNotify() (crossing into a window) and
    // HandlePointerMotion() (crossing bare desktop) funnel through, so
    // "which monitor last saw the pointer" can never disagree between
    // the two. A no-op if the pointer is transiently outside every
    // known monitor rect (e.g. mid-hotplug) or already on the focused one.
    void UpdateFocusedMonitorFromPointer(const Point& pointer);

    // `focusmonitor`/`movetomonitor` remain fully functional commands
    // (kohikoctl, or a bind someone adds back themselves) - they're
    // just not bound by default anymore now that the pointer crossing
    // into a monitor is what focuses it in normal use (see
    // UpdateFocusedMonitorFromPointer()).
    void FocusMonitorCommand(const std::string& arg);

    // Switches `monitor`'s own active workspace to `id`, leaving every
    // other monitor's completely untouched - see the class comment on
    // why this replaced the old single global "current workspace"
    // model. If `id` is already active on some *other* monitor, the
    // request is rejected outright - a notification is shown on
    // `monitor`'s own bar (see ShowNotificationOnMonitor()) and nothing
    // else changes. i3-style: workspaces never silently swap or get
    // stolen out from under the monitor already showing one just
    // because another monitor asked for it too.
    void SwitchWorkspace(int id);
    void SwitchWorkspaceOnMonitor(Monitor& monitor, int id);

    // Shows `text` on `monitor`'s own bar for a few seconds (in place
    // of its title) - Tick() keeps redrawing it until the notification
    // expires. Used for a rejected workspace-switch request (see
    // SwitchWorkspaceOnMonitor()) - could grow other uses later, but
    // that's the only one today.
    void ShowNotificationOnMonitor(Monitor& monitor, const std::string& text);

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
    // the bar it hosts - every monitor gets its own bar now (see
    // RebuildBars()), so this reserves space on all of them, not just
    // Primary(). Called at the top of Arrange() and whenever the bar's
    // configured height might have changed (ReloadConfig()).
    void RefreshMonitorWorkAreas();

    // Creates/destroys Bar instances to match the current monitor set,
    // keyed by Monitor* so a monitor that merely changed geometry (as
    // opposed to actually disconnecting) keeps its same Bar rather than
    // flickering through a destroy/recreate. Called from Initialize()
    // and from HandleMonitorTopologyChanged() after every hotplug.
    // Re-attaches the system tray (a single, X11-wide selection - see
    // SystemTray.h - so it can only ever live on *one* bar) to
    // whichever bar now corresponds to Primary().
    void RebuildBars();

    // nullptr if `monitor` has no bar yet (shouldn't normally happen -
    // RebuildBars() keeps every connected monitor's bar current - but
    // callers that run mid-reconciliation should still check).
    Bar* BarFor(Monitor& monitor) const;

    // Whichever bar owns X window `id`, or nullptr if `id` isn't one -
    // replaces the old single "id == m_bar.WindowId()" checks now that
    // there's one per monitor.
    Bar* BarWindowMatching(::Window id) const;

    // Pushes each monitor's own workspace-count/active-id, its own
    // title (the focused monitor shows whatever window actually holds
    // real X input focus right now; every other monitor is left blank -
    // X focus is singular, so an unfocused monitor has no genuine
    // "focused window" of its own to show, and a stale one would be
    // more misleading than nothing), and the (global) scratchpad/
    // notepad indicator state to every bar, then redraws all of them -
    // the multi-monitor replacement for the old single
    // "m_bar.SetWorkspaces(...); m_bar.SetTitle(...); m_bar.Redraw();"
    // call sites.
    void UpdateAllBars();

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

    // Shared by UpdateFloatingResize()/EndFloatingResize(): recomputes
    // `window`'s rect from m_resizeStartRect/m_resizeStartCursor/
    // m_resizeFromLeft/m_resizeFromTop (set once in
    // BeginFloatingResize()) and the current cursor position, then
    // clamps it - first to `window`'s own effective minimum size
    // (its declared MinWidth()/MinHeight(), floored by
    // general.min_tile_width/height same as TryTile()), then to
    // `monitor`'s WorkArea() - without ever moving the anchored
    // edge(s) in the process.
    Rect ResizedFloatingRect(
        ManagedWindow* window,
        const Point& cursor,
        Monitor& monitor
    ) const;

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
    // value) - refreshes work areas, rebuilds per-monitor bars (see
    // RebuildBars()), relocates anything orphaned by a disconnected
    // monitor, and re-arranges. Also the handler wired up as
    // MonitorManager::SetBeforeMonitorRemovedCallback(), for anything
    // that specifically needs to run *before* a Monitor object is
    // actually destroyed (currently nothing does - the orphan sweep
    // below runs after the fact and RebuildBars() itself handles
    // tearing down that monitor's bar - but the hook stays available
    // for whatever "relocate affected windows safely" grows to mean later).
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

    // One per connected monitor, keyed by Monitor* so a monitor that
    // merely changes geometry keeps the same Bar object (Monitor*
    // itself stays stable across that - see MonitorManager::Detect())
    // rather than flickering through a destroy/recreate. Built/torn
    // down by RebuildBars().
    std::map<Monitor*, std::unique_ptr<Bar>> m_bars;
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

    // Whichever floating window is currently being resized by a
    // Super+RMB drag (nullptr the rest of the time) - kept entirely
    // separate from m_activeDragWindow since a resize never touches
    // the BSP tree the way a Swap drag does, and the two gestures
    // never overlap in practice (MouseManager only ever has one drag
    // in flight at once) but there's no reason to conflate their state.
    ManagedWindow* m_activeResizeWindow = nullptr;

    // The window's own geometry at the moment the resize began - every
    // UpdateFloatingResize() computes the new rect from this fixed
    // starting point and the total cursor delta since then, rather
    // than accumulating small per-motion deltas, so the result can
    // never drift from what the cursor actually did.
    Rect m_resizeStartRect;
    Point m_resizeStartCursor;

    // Which edge(s) of m_resizeStartRect the cursor grabbed nearest to
    // - decided once in BeginFloatingResize() from where inside the
    // window the press landed, and fixed for the rest of that drag.
    // Growing "from the left" moves x and shrinks width in step (the
    // right edge stays put); growing "from the right" only changes
    // width (the left edge stays put) - and the same for top/bottom
    // against height.
    bool m_resizeFromLeft = false;
    bool m_resizeFromTop = false;

    // Whatever was focused right before the Launcher/Notepad opened,
    // so closing either one gives focus back rather than leaving
    // whatever the mouse happens to be over focused instead.
    WindowID m_focusBeforeModal = 0;

};

}
