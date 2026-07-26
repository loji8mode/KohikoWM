#include "WindowManager.h"

#include "Command.h"
#include "Config.h"
#include "IpcPath.h"
#include "Json.h"
#include "Logger.h"
#include "ManagedWindow.h"
#include "Process.h"
#include "Utils.h"
#include "XConnection.h"

#include <X11/Xatom.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace Kohiko
{

WindowManager::WindowManager(
    XConnection& connection,
    Config& config,
    std::string configPath)
    :
    m_connection(connection),
    m_config(config),
    m_configPath(std::move(configPath)),
    m_atoms(connection),
    m_cursor(connection),
    m_repository(),
    m_workspaces(config.GetInt("workspace.count", 10)),
    m_monitors(connection, m_workspaces),
    m_layout(),
    m_scratchpad(),
    m_keyboard(connection, *this),
    m_mouse(connection, *this),
    m_dispatcher(*this, m_keyboard, m_mouse),
    m_ipc(),
    m_bar(connection),
    m_launcher(connection),
    m_notepad(connection),
    m_animator()
{
}

void WindowManager::Initialize()
{
    m_atoms.Initialize();
    m_cursor.Initialize();
    m_monitors.Detect();

    // Advertise EWMH support - _NET_SUPPORTED/_NET_SUPPORTING_WM_CHECK
    // let tools that check for a compliant window manager (flameshot's
    // screenshot overlay among them) trust _NET_CLIENT_LIST and
    // _NET_ACTIVE_WINDOW, which Manage()/Unmanage()/Focus() below keep
    // current from here on.
    m_ewmhCheckWindow = m_connection.InitializeEwmhSupport(m_atoms, "kohiko");

    m_bar.Configure(m_config, m_monitors.Primary().Geometry());
    m_bar.Show();

    m_tray.Initialize(m_connection, m_bar.WindowId());
    m_bar.AttachSystemTray(&m_tray);

    m_launcher.Configure(m_config, m_monitors.Primary().Geometry());
    m_notepad.Configure(m_config, m_monitors.Primary().Geometry());
    m_bar.SetNotepadActive(m_notepad.HasContent());

    m_windowRules = LoadWindowRules(m_config);

    m_keyboard.Configure(m_config);
    m_mouse.Configure(m_config);

     m_fileManager =
    m_config.GetString(
        "launcher.file_manager",
        "pcmanfm");

    m_ipc.Start(
        IpcSocketPath(),
        [this](const std::string& request)
        {
            return HandleIpcCommand(request);
        });

    // Keyboard layouts are applied via setxkbmap (real XKB groups),
    // not anything Kohiko's own KeyboardManager tracks - that's what
    // makes typing in any listed language work in every window, not
    // just ones Kohiko manages, and it's why Super+H/J/K/L etc. never
    // needed special-casing for non-English layouts in the first
    // place: KeyboardManager grabs by physical keycode (see its class
    // comment), and a keycode means the same physical key no matter
    // which XKB group is currently active.
    ApplyKeyboardLayouts();

    // Autostart programs launch exactly once here, after the bar/tray/
    // launcher above are already up, so anything they immediately try
    // to dock (a tray icon, a notification) has somewhere to land.
    RunAutostart();

    Arrange();


}

void WindowManager::Shutdown()
{
    m_ipc.Stop();
    m_tray.Shutdown();
    m_bar.Hide();
}

bool WindowManager::IsRunning() const
{
    return m_running;
}

EventDispatcher& WindowManager::Dispatcher()
{
    return m_dispatcher;
}

IPCServer& WindowManager::Ipc()
{
    return m_ipc;
}

void WindowManager::Tick()
{
    if (m_animator.Active())
        m_animator.Step(m_connection);

    if (m_launcher.IsOpen())
        m_launcher.Blink();

    if (m_notepad.IsOpen())
        m_notepad.Blink();

    m_bar.Redraw();
}

bool WindowManager::HasActiveAnimation() const
{
    return m_animator.Active();
}

// --- X11 event handlers -----------------------------------------------------

void WindowManager::HandleMapRequest(const XMapRequestEvent& event)
{
    Manage(event.window);
}

void WindowManager::HandleConfigureRequest(const XConfigureRequestEvent& event)
{
    ManagedWindow* window = m_repository.Get(event.window);

    if (!window || window->IsFloating())
    {
        // Start from whatever this window's geometry actually is right
        // now (falling back to the request itself if it isn't managed
        // yet) and only overwrite the fields this particular request
        // actually asked to change - a client resizing without moving,
        // say, shouldn't have its untouched x/y treated as "0,0" just
        // because CWX/CWY weren't set this time.
        Rect requested =
            window
                ? window->Geometry()
                : Rect{event.x, event.y, static_cast<int>(event.width), static_cast<int>(event.height)};

        if (event.value_mask & CWX)      requested.x      = event.x;
        if (event.value_mask & CWY)      requested.y      = event.y;
        if (event.value_mask & CWWidth)  requested.width  = event.width;
        if (event.value_mask & CWHeight) requested.height = event.height;

        // Geometry Rule: a floating client's own ConfigureRequest is
        // honoured - that's what makes it "floating" - but never at
        // the cost of letting it place itself off-screen. Clamped
        // right here, before it ever reaches X11, exactly like every
        // other geometry-then-apply site in this file.
        const Rect& monitor = m_monitors.Primary().Geometry();
        int borderWidth = window ? window->BorderWidth() : static_cast<int>(event.border_width);
        Rect clamped = requested.ClampedTo(monitor, borderWidth);

        XWindowChanges changes{};
        changes.x = clamped.x;
        changes.y = clamped.y;
        changes.width = clamped.width;
        changes.height = clamped.height;
        changes.border_width = event.border_width;
        changes.sibling = event.above;
        changes.stack_mode = event.detail;

        m_connection.ConfigureWindowRaw(event.window, changes, static_cast<unsigned int>(event.value_mask));

        if (window)
        {
            window->SetGeometry(clamped);
            window->SetFloatingGeometry(clamped);
        }

        return;
    }

    // Tiled: we own the geometry - acknowledge with what it actually
    // is so the client doesn't sit waiting for a reply.
    const Rect& actual = window->Geometry();
    m_connection.SendConfigureNotify(event.window, actual, window->BorderWidth());

    // A client that just asked for something *other* than what it's
    // actually getting - TLauncher re-asserting its own preferred size
    // mid-session is the reliable reproducer, e.g. right after its
    // "restart to finish updating" flow swaps in a smaller splash/
    // loading layout and tries to shrink to fit it - may well have
    // already speculatively repainted itself at the size it asked for,
    // before this refusal ever reaches it. Nothing about the window's
    // real geometry changed here, so the server has no reason to
    // generate an Expose on its own - without one, whatever the client
    // already drew over its old content just sits there, orphaned in a
    // corner of the real (unchanged, still fully tiled) window, which
    // is exactly the "opens crooked" look. Only forcing this when the
    // request actually asked for something different keeps it free for
    // a well-behaved client that only ever confirms the size it
    // already has.
    bool askedForSomethingElse =
        ((event.value_mask & CWX)      && event.x      != actual.x) ||
        ((event.value_mask & CWY)      && event.y      != actual.y) ||
        ((event.value_mask & CWWidth)  && event.width  != actual.width) ||
        ((event.value_mask & CWHeight) && event.height != actual.height);

    if (!askedForSomethingElse)
        return;

    m_connection.ClearArea(event.window);

    // Window Misbehavior Detection: a ConfigureRequest conflict on its
    // own isn't proof of anything - a client resizing itself once as
    // part of normal startup is completely ordinary. What actually
    // indicates a client fighting its tiled geometry is this
    // happening *repeatedly* in the same tile (repeated resize
    // attempts / a ConfigureRequest loop), which is exactly what
    // TilingMisbehaviorCount() accumulates - reset back to zero every
    // time this window is freshly tiled (TryTile()) or the fallback
    // below actually fires, so it only ever measures how the window
    // is behaving in its *current* placement.
    if (!window->IsTiled())
        return; // fullscreen/scratchpad ConfigureRequest handling is unrelated to tiling fit

    window->RegisterTilingMisbehavior();

    int threshold = m_config.GetInt("general.tiling_misbehavior_threshold", 3);

    if (threshold > 0 && window->TilingMisbehaviorCount() >= threshold)
        ApplyTilingMisbehaviorFallback(window);
}

void WindowManager::HandleUnmapNotify(const XUnmapEvent& event)
{
    ManagedWindow* window = m_repository.Get(event.window);

    if (!window)
        return;

    if (window->ConsumeIgnoredUnmap())
        return;

    Unmanage(event.window);
}

void WindowManager::HandleDestroyNotify(const XDestroyWindowEvent& event)
{
    if (m_repository.Contains(event.window))
        Unmanage(event.window);

    m_tray.HandleWindowDestroyed(event.window);
}

void WindowManager::HandleEnterNotify(const XCrossingEvent& event)
{
    if (event.detail == NotifyInferior)
        return;

    bool pointerActuallyMoved =
        (event.x_root != m_lastPointerX || event.y_root != m_lastPointerY);

    m_lastPointerX = event.x_root;
    m_lastPointerY = event.y_root;

    // Rearranging the tiled layout can put a different window under an
    // unmoved cursor, which X11 *also* reports as EnterNotify - as can
    // starting/ending a pointer grab (e.g. MouseManager's Super+drag).
    // Only treat this as "the user moved the mouse here" if it's a
    // normal crossing and the coordinates actually changed.
    if (event.mode != NotifyNormal || !pointerActuallyMoved)
        return;

    // The Launcher/Notepad hold input focus for typing - a background
    // window happening to be under the pointer must not steal it back
    // (that's the exact "hotkeys/typing quietly stop working" failure
    // mode the Super+Q hardening elsewhere in this file is about).
    if (m_launcher.IsOpen() || m_notepad.IsOpen())
        return;

    if (!m_config.GetBool("general.focus_follows_mouse", true))
        return;

    ManagedWindow* window = m_repository.Get(event.window);

    if (window && !window->Focused())
        Focus(event.window);
}

void WindowManager::HandleFocusIn(const XFocusChangeEvent& event)
{
    if (!m_launcher.IsOpen() && !m_notepad.IsOpen())
        return;

    ::Window modalWindow =
        m_launcher.IsOpen() ? m_launcher.WindowId() : m_notepad.WindowId();

    if (event.window == modalWindow)
        return;

    // Some apps (a handful of Electron/GTK ones, mostly) call
    // XSetInputFocus on their own window right after mapping, entirely
    // independent of whatever the window manager just did - there's
    // nothing in the X protocol that stops a client from doing this.
    // Without this, the Launcher/Notepad - which never uses an active
    // keyboard grab, see Launcher's class comment - would silently
    // stop receiving keystrokes the moment that happens, with no
    // visible sign anything had changed and no way to even close it
    // from the keyboard. Just take focus straight back.
    m_connection.SetInputFocus(modalWindow);
}

void WindowManager::HandlePropertyNotify(const XPropertyEvent& event)
{
    if (event.atom != m_atoms.NET_WM_NAME && event.atom != XA_WM_NAME)
        return;

    ManagedWindow* window = m_repository.Get(event.window);

    if (!window)
        return;

    window->SetTitle(m_connection.GetWindowTitle(event.window, m_atoms));

    if (window->Focused())
    {
        m_bar.SetTitle(window->Title());
        m_bar.Redraw();
    }
}

void WindowManager::HandleButtonPressOnClient(const XButtonEvent& event)
{
    // Clicking a background window while the Launcher/Notepad is open
    // reads as "I'm done with this" - dismiss it first (as a cancel,
    // not a run) rather than leaving it visually open but no longer
    // able to receive the typing it needs focus for.
    if (m_launcher.IsOpen())
        CloseLauncher(false);

    if (m_notepad.IsOpen())
        CloseNotepad();

    if (m_repository.Contains(event.window))
        Focus(event.window);

    m_connection.AllowReplayPointer();
}

void WindowManager::HandleExpose(const XExposeEvent& event)
{
    if (event.count != 0)
        return;

    if (event.window == m_bar.WindowId())
        m_bar.Redraw();
    else if (event.window == m_launcher.WindowId())
        m_launcher.HandleExpose();
    else if (event.window == m_notepad.WindowId())
        m_notepad.HandleExpose();
}

void WindowManager::HandleClientMessage(const XClientMessageEvent& event)
{
    m_tray.HandleClientMessage(event);

    // The standard EWMH way an already-mapped client asks the WM for a
    // state change - "please make me fullscreen" (flameshot's overlay,
    // a video player's own fullscreen button/shortcut, a chat client's
    // media viewer, ...) chief among the ones Kohiko understands.
    // Message-format details straight from the spec: data.l[0] is the
    // action (0 remove / 1 add / 2 toggle - the EWMH
    // _NET_WM_STATE_REMOVE/ADD/TOGGLE constants, which aren't
    // otherwise exposed as X11 atoms/macros), and data.l[1]/data.l[2]
    // are up to two state atoms being changed at once; Kohiko only
    // actually implements one state, so it's enough to check both
    // slots for it and ignore anything else it doesn't recognise.
    if (event.message_type != m_atoms.NET_WM_STATE)
        return;

    ManagedWindow* window = m_repository.Get(event.window);

    if (!window)
        return;

    bool concernsFullscreen =
        static_cast<Atom>(event.data.l[1]) == m_atoms.NET_WM_STATE_FULLSCREEN ||
        static_cast<Atom>(event.data.l[2]) == m_atoms.NET_WM_STATE_FULLSCREEN;

    if (!concernsFullscreen)
        return;

    WindowRuleEffect rules =
        ResolveWindowRules(window->ClassName(), window->InstanceName(), window->Title());

    if (rules.denyFullscreen)
        return; // this class is configured to never be allowed real fullscreen

    static constexpr long kNetWmStateRemove = 0;
    static constexpr long kNetWmStateAdd    = 1;
    static constexpr long kNetWmStateToggle = 2;

    bool shouldBeFullscreen = window->IsFullscreen();

    switch (event.data.l[0])
    {
        case kNetWmStateAdd:    shouldBeFullscreen = true;                    break;
        case kNetWmStateRemove: shouldBeFullscreen = false;                   break;
        case kNetWmStateToggle: shouldBeFullscreen = !window->IsFullscreen(); break;
        default: return;
    }

    if (shouldBeFullscreen == window->IsFullscreen())
        return;

    if (shouldBeFullscreen)
    {
        window->SetPreviousState(window->State());
        window->SetState(WindowState::Fullscreen);
    }
    else
    {
        window->SetState(window->PreviousState());
    }

    m_connection.SetNetWmState(m_atoms, window->Id(), shouldBeFullscreen);

    Arrange();
}

bool WindowManager::HandleModalKeyPress(const XKeyEvent& event)
{
    if (m_launcher.IsOpen())
    {
        switch (m_launcher.HandleKeyPress(event))
        {
            case LauncherResult::Confirmed: CloseLauncher(true);  break;
            case LauncherResult::Cancelled: CloseLauncher(false); break;
            case LauncherResult::Editing:                         break;
        }

        return true;
    }

    if (m_notepad.IsOpen())
    {
        if (m_notepad.HandleKeyPress(event) == NotepadResult::Closed)
            CloseNotepad();

        return true;
    }

    return false;
}

bool WindowManager::IsManaged(WindowID id) const
{
    return m_repository.Contains(id);
}

// --- Command execution -------------------------------------------------------

void WindowManager::Execute(const Command& command)
{
    switch (command.type)
    {
        case CommandType::Exec:
        {
            std::string resolved = m_config.GetString("exec." + command.stringArg);
            Process::Spawn(resolved.empty() ? command.stringArg : resolved, m_connection.DisplayName());
            break;
        }

        case CommandType::Close:            CloseFocused();                        break;
        case CommandType::ToggleFloating:   ToggleFloating();                      break;
        case CommandType::ToggleFullscreen: ToggleFullscreen();                    break;
        case CommandType::ScratchpadToggle: ToggleScratchpadForFocused();          break;
        case CommandType::Workspace:        SwitchWorkspace(command.intArg);       break;
        case CommandType::MoveToWorkspace:  MoveFocusedToWorkspace(command.intArg);break;
        case CommandType::FocusDirection:   FocusDirection(command.directionArg);  break;
        case CommandType::Rotate:           RotateFocused();                       break;
        case CommandType::Flip:             FlipFocused();                         break;
        case CommandType::Reload:           ReloadConfig();                        break;
        case CommandType::Quit:             m_running = false;                     break;
        case CommandType::LauncherToggle:   ToggleLauncher();                      break;
        case CommandType::NotepadToggle:    ToggleNotepad();                       break;

        case CommandType::LauncherReload:
            m_launcher.ReloadDesktopEntries();
            m_launcher.HandleExpose();
            break;

        case CommandType::NoCommand:
        default:
            break;
    }
}

// --- MouseManager-facing API -------------------------------------------------

ManagedWindow* WindowManager::WindowAt(const Point& point)
{
    return m_workspaces.Current().Tree().HitTest(point);
}

void WindowManager::SwapWindows(ManagedWindow* first, ManagedWindow* second)
{
    if (!first || !second)
        return;

    m_workspaces.Get(first->Workspace()).Tree().Swap(first, second);

    Arrange();
}

void WindowManager::ResizeWindow(ManagedWindow* window, int dx, int dy)
{
    if (!window)
        return;

    m_workspaces.Get(window->Workspace()).Tree().Resize(window, dx, dy);

    Arrange();
}

void WindowManager::SetResizingCursor(bool resizing)
{
    m_cursor.SetResizing(resizing);
}

void WindowManager::BeginSwapDrag(ManagedWindow* window, const Point& cursor)
{
    if (!window || !window->IsTiled())
        return;

    m_activeDragWindow = window;
    m_dragHoverTarget = nullptr;

    const Rect& geometry = window->Geometry();
    m_dragOffsetX = cursor.x - geometry.x;
    m_dragOffsetY = cursor.y - geometry.y;
    m_dragCurrentRect = geometry;

    m_connection.Raise(window->Id());
    RaiseModalWindows();
    m_cursor.SetDragging(true);
}

void WindowManager::UpdateSwapDrag(ManagedWindow* window, const Point& cursor)
{
    if (!window || window != m_activeDragWindow)
        return;

    // The window "breaks off and moves with the cursor" - only its
    // position changes, and only on screen; nothing else in the tree
    // is touched until the button is released.
    m_dragCurrentRect.x = cursor.x - m_dragOffsetX;
    m_dragCurrentRect.y = cursor.y - m_dragOffsetY;

    m_connection.MoveWindowTo(window->Id(), m_dragCurrentRect.x, m_dragCurrentRect.y);

    // Highlight whichever tiled window the cursor is currently over,
    // so - per the doc's "safety feedback loop" - it's clear exactly
    // what will be swapped with before that's committed by releasing
    // the button.
    ManagedWindow* hovered = m_workspaces.Get(window->Workspace()).Tree().HitTest(cursor);

    if (hovered == window)
        hovered = nullptr;

    if (hovered == m_dragHoverTarget)
        return;

    if (m_dragHoverTarget)
        RefreshBorderColor(m_dragHoverTarget);

    m_dragHoverTarget = hovered;

    if (m_dragHoverTarget && m_dragHoverTarget->BorderWidth() > 0)
        m_connection.SetBorderColor(m_dragHoverTarget->Id(), ParseColor("general.border_color_active", "0x89b4fa"));
}

void WindowManager::EndSwapDrag(ManagedWindow* window, const Point& cursor)
{
    if (!window || window != m_activeDragWindow)
        return;

    m_cursor.SetDragging(false);

    if (m_dragHoverTarget)
        RefreshBorderColor(m_dragHoverTarget);

    ManagedWindow* target = m_workspaces.Get(window->Workspace()).Tree().HitTest(cursor);

    if (target == window)
        target = nullptr;

    Rect fromRect = m_dragCurrentRect;

    // Clear the drag state *before* SwapWindows()/Arrange() run below,
    // so Arrange() is free to reposition this window like any other
    // tiled one again.
    m_activeDragWindow = nullptr;
    m_dragHoverTarget = nullptr;

    constexpr int kSwapAnimationMs = 160;

    if (target)
    {
        Rect targetFromRect = target->Geometry();

        // Arrange() (called inside SwapWindows()) immediately snaps
        // both windows to their new final rects - the two Animator
        // calls below then replay that transition visually, from
        // wherever each window actually was, instead of it happening
        // as an instant jump.
        SwapWindows(window, target);

        m_animator.Start(window, fromRect, window->Geometry(), kSwapAnimationMs);
        m_animator.Start(target, targetFromRect, target->Geometry(), kSwapAnimationMs);
    }
    else
    {
        // No valid drop target - snap back to exactly where it was.
        // The tree assignment was never touched during the drag, so
        // window->Geometry() still holds the original tiled rect.
        m_animator.Start(window, fromRect, window->Geometry(), kSwapAnimationMs);
    }
}

// --- IPCServer-facing API ---------------------------------------------------

std::string WindowManager::HandleIpcCommand(const std::string& request)
{
    std::istringstream stream(request);

    std::string verb;
    stream >> verb;
    verb = Utils::Lower(verb);

    if (verb == "dispatch")
    {
        std::string rest;
        std::getline(stream, rest);
        Execute(Command::Parse(Utils::Trim(rest)));
        return "ok";
    }

    if (verb == "clients")
        return DumpClientsJson();

    if (verb == "monitors")
        return DumpMonitorsJson();

    if (verb == "activewindow")
        return DumpActiveWindowJson();

    if (verb == "tree")
        return m_workspaces.Current().Tree().Serialize();

    if (verb == "reload")
    {
        ReloadConfig();
        return "ok";
    }

    if (verb == "reloadlauncher")
    {
        m_launcher.ReloadDesktopEntries();
        m_launcher.HandleExpose();
        return "ok";
    }

    if (verb == "quit")
    {
        m_running = false;
        return "ok";
    }

    return "error: unknown command";
}

// --- internals ----------------------------------------------------------------

void WindowManager::Manage(WindowID id)
{
    if (m_repository.Contains(id))
        return;

    // Never manage Kohiko's own utility windows. They're all created
    // override_redirect (so X never even offers them to us as a
    // MapRequest in the first place), but checking their IDs here too
    // is cheap, explicit insurance that Super+Q - or anything else
    // that walks m_repository - can *never* end up targeting the bar,
    // the launcher, or the notepad, no matter what changes elsewhere.
    // That guarantee is what "the panel should never close this
    // combination" means in practice: CloseFocused() only ever acts on
    // m_repository.Focused(), and this is what keeps that set free of
    // Kohiko's own popups by construction, not by convention.
    if (id == m_bar.WindowId() || id == m_launcher.WindowId() || id == m_notepad.WindowId())
        return;

    XWindowAttributes attrs{};

    if (m_connection.GetWindowAttributes(id, attrs) && attrs.override_redirect)
        return;

    ManagedWindow* window = m_repository.Add(id);

    window->SetTitle(m_connection.GetWindowTitle(id, m_atoms));

    std::string className;
    std::string instanceName;
    m_connection.GetWindowClass(id, className, instanceName);
    window->SetClassName(className);
    window->SetInstanceName(instanceName);
    window->SetRole(m_connection.GetWindowRole(id, m_atoms));
    window->SetPid(m_connection.GetWindowPid(id, m_atoms));

    window->SetWorkspace(m_workspaces.CurrentId());
    window->SetMonitor(0);
    window->SetBorderWidth(m_config.GetInt("general.border_size", 2));

    // Needed before TryTile() below - a client (TLauncher's real UI is
    // the reliable reproducer) that declares a minimum size larger
    // than the tile it ends up in will still get force-resized into
    // that tile regardless (MoveResizeWindow isn't bound by the hint -
    // only *interactive* resizing agents are expected to respect it),
    // and unlike a client that's small enough to just look cramped,
    // one whose layout can't actually fit below its own stated minimum
    // clips content outright - parts of the GUI become genuinely
    // inaccessible rather than just tight. TryTile()'s capacity check
    // needs this so it can recognise "no, this one specifically needs
    // more room than that" instead of only ever checking against the
    // one-size-fits-all general.min_tile_width/height.
    int minWidth = 0;
    int minHeight = 0;
    m_connection.GetMinSize(id, minWidth, minHeight);
    window->SetMinSize(minWidth, minHeight);

    m_connection.SelectInputFor(id, EnterWindowMask | PropertyChangeMask | FocusChangeMask);

    // Plain (no Super) click-to-focus: sync-grabbed so we always get
    // first refusal, then replayed so the client still sees the click.
    for (unsigned int variant : m_connection.LockVariants(0))
        m_connection.GrabButtonOnWindow(id, Button1, variant);

    WindowID transientOwner = 0;
    bool isTransient = m_connection.GetTransientFor(id, transientOwner);
    bool isFloatingType = m_connection.IsFloatingWindowType(id, m_atoms);

    // The managed window `id` is WM_TRANSIENT_FOR, if the hint
    // resolves to one - nullptr for a plain top-level window, for a
    // hint pointing at the root/an unmanaged window, and for a hint
    // that hasn't been set at all. Everything below that talks about
    // "the parent" means this, not the raw transientOwner id.
    ManagedWindow* parent = isTransient ? m_repository.Get(transientOwner) : nullptr;

    WindowRuleEffect rules = ResolveWindowRules(className, instanceName, window->Title());

    // A `windowrule=workspace:N` match ("open Telegram's media viewer
    // on its own dedicated workspace instead of cluttering whatever's
    // current" is the motivating example) has to land before the
    // tiling-capacity checks below, since which workspace this window
    // even competes for room on depends on it. An explicit rule like
    // that is a deliberate per-window override and wins over automatic
    // parent-attachment; absent one, a transient/dialog always belongs
    // on whichever workspace its parent actually lives on right now -
    // "the child always appears attached to its parent, never on the
    // wrong workspace" - rather than wherever happened to be current
    // when it was mapped.
    if (rules.forceWorkspace >= 1 && rules.forceWorkspace <= m_workspaces.Count())
        window->SetWorkspace(rules.forceWorkspace);
    else if (parent)
        window->SetWorkspace(parent->Workspace());

    // `windowrule=tile` is an explicit "no matter what this
    // application's own hints say, force it into the tiling layout"
    // override - a Java/Swing launcher that insists on floating at its
    // own fixed size (and fights the layout it's given) is the
    // motivating example. It skips the transient/dialog auto-float
    // path below entirely and falls straight through to the same
    // TryTile() every ordinary window goes through - which, unlike a
    // ConfigureRequest a client might send afterward asking to be its
    // "own size" again, Kohiko simply never honours for a tiled window
    // (see HandleConfigureRequest): once tiled, it stays exactly the
    // size Kohiko's layout says, strictly, for good.
    //
    // Absent that override, every dialog/utility/splash/toolbar/popup
    // window type and every window with a WM_TRANSIENT_FOR set at all
    // floats - only a plain window with none of those (EWMH's
    // definition of NORMAL) ever reaches TryTile() below, so only
    // NORMAL windows ever enter the BSP tree.
    bool wantsFloat = !rules.forceTile && (rules.forceFloat || isTransient || isFloatingType);

    if (wantsFloat)
    {
        window->SetState(WindowState::Floating);

        Rect geometry = CenteredFloatingRectForWindow(id, attrs, parent);
        window->SetGeometry(geometry);
        window->SetFloatingGeometry(geometry);

        m_connection.MoveResizeWindow(id, geometry);
        m_connection.SetBorderWidth(id, window->BorderWidth());
    }
    else if (TryTile(window, window->Workspace()))
    {
        // Tiled on the current workspace - the common case.
    }
    else
    {
        int currentWorkspace = window->Workspace();
        int alternate = FindWorkspaceWithRoom(window, currentWorkspace);

        if (alternate != 0 && TryTile(window, alternate))
        {
            Logger::Info(
                "Workspace " + std::to_string(currentWorkspace) +
                " has no room for another tile without dropping below the "
                "configured minimum size - opened window " +
                std::to_string(static_cast<unsigned long>(id)) +
                " on workspace " + std::to_string(alternate) + " instead.");
        }
        else
        {
            // Every workspace's tiling layout is genuinely full (down
            // to minWidth x minHeight everywhere) - what happens next
            // is governed by general.tiling_misbehavior_fallback, the
            // same knob ApplyTilingMisbehaviorFallback() uses. It's
            // really the same question either way - "this window can't
            // have fixed tiled geometry right now, where does it go
            // instead?" - just asked at open time here rather than
            // after the fact.
            std::string fallback = Utils::Lower(m_config.GetString("general.tiling_misbehavior_fallback", "floating"));

            bool placed = false;

            if (fallback == "new_workspace")
            {
                int target = FindWorkspaceWithFewestWindows(currentWorkspace);

                if (target != 0 && target != currentWorkspace)
                {
                    window->SetWorkspace(target);
                    placed = TryTile(window, target);

                    if (placed)
                    {
                        Logger::Info(
                            "No workspace has room for another tile without "
                            "dropping below the configured minimum size - moved "
                            "window " + std::to_string(static_cast<unsigned long>(id)) +
                            " to workspace " + std::to_string(target) +
                            " (fewest windows) instead.");
                    }
                    else
                    {
                        window->SetWorkspace(currentWorkspace);
                    }
                }
            }

            if (!placed)
            {
                // Either the fallback is "floating", or "new_workspace"
                // genuinely had nowhere better to go either - what
                // actually guarantees "no window ever goes beyond the
                // screen, or gets forced into an undersized tile" is
                // floating it instead.
                window->SetState(WindowState::Floating);

                Rect geometry = CenteredFloatingRectForWindow(id, attrs, parent);
                window->SetGeometry(geometry);
                window->SetFloatingGeometry(geometry);

                m_connection.MoveResizeWindow(id, geometry);
                m_connection.SetBorderWidth(id, window->BorderWidth());

                Logger::Warning(
                    "No workspace has room for another tile without dropping "
                    "below the configured minimum size - opened window " +
                    std::to_string(static_cast<unsigned long>(id)) + " floating instead.");
            }
        }
    }

    // A client's own EWMH fullscreen request, made before it was ever
    // mapped (some toolkits set _NET_WM_STATE directly instead of
    // sending a ClientMessage, since the message form is only
    // meaningful for an already-mapped window - see
    // HandleClientMessage() for that path) and `windowrule=fullscreen`
    // (flameshot's screenshot overlay - which needs to genuinely cover
    // the whole screen to work at all, not open at some fraction of it
    // - is the motivating example for both) mean the same thing here:
    // this window should already be fullscreen the instant it appears,
    // not tiled/floating-sized for one frame first. `nofullscreen`
    // wins over both if a rule says this class should never be allowed
    // real fullscreen at all.
    bool wantsFullscreen =
        !rules.denyFullscreen &&
        (rules.forceFullscreen || m_connection.HasNetWmStateFullscreen(id, m_atoms));

    if (wantsFullscreen)
    {
        window->SetPreviousState(window->State());
        window->SetState(WindowState::Fullscreen);
        m_connection.SetNetWmState(m_atoms, id, true);
    }

    // A transient/dialog child was pinned to its parent's workspace
    // above, whichever one that turns out to be - if that's not the
    // workspace currently on screen, switching to it now is what makes
    // "always appear attached to its parent" actually true, rather
    // than just true in the bookkeeping while the window itself sits
    // unmapped in the background until someone happens to switch over
    // manually. Deliberately gated on `parent` (not just "landed on a
    // background workspace" in general) - an ordinary `windowrule=
    // workspace:N` window with no parent is *supposed* to open quietly
    // in the background without yanking focus away from whatever the
    // user is doing (see the comment on that rule in ResolveWindowRules's
    // caller above); only a transient explicitly follows its parent
    // into view. Compares against window->Workspace() rather than
    // parent->Workspace() directly so an explicit `windowrule=
    // workspace:N` that disagreed with the parent's workspace (it wins,
    // above) is still respected here rather than switching to
    // wherever the parent happens to be instead.
    //
    // SwitchWorkspace() unmaps the old workspace, arranges and maps the
    // new one (which by now already includes this window - it was
    // added to m_repository, and had its workspace set, above), and
    // focuses whatever it finds first there; the block below then
    // takes over and re-focuses this window specifically, exactly the
    // same way it already corrects for SwitchWorkspace()'s "focus the
    // front visible window" default in the normal switch-workspace path.
    if (parent && window->Workspace() != m_workspaces.CurrentId())
        SwitchWorkspace(window->Workspace());

    // A window the fallback above redistributed onto a *different*
    // workspace must stay unmapped and unfocused, exactly like every
    // other window living on a workspace that isn't the current one -
    // SwitchWorkspace() is what maps it and gives it a chance at focus
    // once the user actually switches there. Mapping/focusing it here
    // regardless of which workspace it landed on would show a window
    // nobody asked to see right now and steal focus from whatever *is*
    // currently visible.
    if (window->Workspace() == m_workspaces.CurrentId())
    {
        // Resize into its real tile slot *before* mapping, not after.
        // A client (Java/Swing apps like TLauncher are the reliable
        // reproducer, but it's not Java-specific) that gets mapped at
        // its own small requested size and is then immediately
        // resized larger can end up with its content stuck in the
        // corner corresponding to that original size - the newly
        // exposed area never gets painted, because the resize raced
        // the client's own realization instead of being the size it
        // was realized at in the first place. Configuring geometry on
        // a not-yet-mapped window is perfectly legal (and is exactly
        // what the floating branch above, and every other
        // geometry-then-Map site in this file, already does) - Arrange()
        // just needs to run before MapWindow() here too.
        Arrange();
        m_connection.MapWindow(id);

        // A window opened while the Launcher/Notepad is up should
        // still tile in and become visible - just not take keyboard
        // focus away from it. The Launcher deliberately never uses an
        // XGrabKeyboard (see its class comment), so it depends
        // entirely on actually holding X input focus to receive
        // Enter/Escape/typing at all; losing focus here used to leave
        // it visibly open but completely unreachable from the
        // keyboard, with no way to even close it. Re-asserting focus
        // on the modal (rather than just skipping the Focus(id) call)
        // also covers apps that request focus for themselves as part
        // of mapping - HandleFocusIn() below is the second line of
        // defence for ones that grab it a moment later instead.
        if (m_launcher.IsOpen())
        {
            m_connection.SetInputFocus(m_launcher.WindowId());
            RefreshBorderColor(window);
        }
        else if (m_notepad.IsOpen())
        {
            m_connection.SetInputFocus(m_notepad.WindowId());
            RefreshBorderColor(window);
        }
        else
        {
            Focus(id);
        }
    }
    else
    {
        Arrange();
    }

    RefreshClientList();
}

void WindowManager::Unmanage(WindowID id)
{
    ManagedWindow* window = m_repository.Get(id);

    if (!window)
        return;

    bool wasFocused = window->Focused();
    int workspace = window->Workspace();

    if (window->OccupiesTreeSlot())
        m_workspaces.Get(workspace).Tree().Remove(window);

    m_scratchpad.Forget(id);
    m_repository.Remove(id);

    RefreshClientList();

    if (wasFocused)
        FocusNextAvailable();

    Arrange();
}

void WindowManager::RefreshClientList()
{
    std::vector<::Window> ids;
    ids.reserve(m_repository.All().size());

    for (ManagedWindow* window : m_repository.All())
        ids.push_back(window->Id());

    m_connection.SetClientList(m_atoms, ids);
}

void WindowManager::Focus(WindowID id)
{
    ManagedWindow* window = m_repository.Get(id);

    if (!window)
        return;

    m_repository.ClearFocus();
    window->SetFocused(true);

    m_workspaces.Get(window->Workspace()).Tree().Focus(window);

    m_connection.SetInputFocus(id);
    m_connection.SetActiveWindow(m_atoms, id);

    RefreshBorderColor(window);

    for (ManagedWindow* other : m_repository.Visible(m_workspaces.CurrentId()))
    {
        if (other->Id() != id)
            RefreshBorderColor(other);
    }

    m_bar.SetTitle(window->Title());
    m_bar.Redraw();
}

void WindowManager::FocusNextAvailable()
{
    auto visible = m_repository.Visible(m_workspaces.CurrentId());

    if (!visible.empty())
    {
        Focus(visible.front()->Id());
        return;
    }

    m_repository.ClearFocus();
    m_connection.SetInputFocus(m_connection.Root());
    m_connection.SetActiveWindow(m_atoms, None);
    m_bar.SetTitle("");
    m_bar.Redraw();
}

void WindowManager::Arrange()
{
    Workspace& workspace = m_workspaces.Current();
    const Rect& monitor = m_monitors.Primary().Geometry();
    Rect tilingArea = TilingArea();

    LayoutEngine::Params params;
    params.innerGap     = m_config.GetInt("general.inner_gap", 6);
    params.outerGap     = m_config.GetInt("general.outer_gap", 6);
    params.borderWidth  = m_config.GetInt("general.border_size", 2);
    params.smartGaps    = m_config.GetBool("general.smart_gaps", true);
    params.smartBorders  = m_config.GetBool("general.smart_borders", true);

    if (!workspace.Tree().Empty())
        m_layout.Apply(workspace.Tree().Root(), tilingArea, params);

    ManagedWindow* fullscreenWindow = nullptr;

    for (ManagedWindow* window : m_repository.Visible(m_workspaces.CurrentId()))
    {
        if (window == m_activeDragWindow)
            continue; // being driven directly by the Swap drag right now - don't snap it back

        if (window->IsFullscreen())
        {
            fullscreenWindow = window;
            continue;
        }

        if (window->IsFloating())
        {
            m_connection.MoveResizeWindow(window->Id(), window->Geometry());
            m_connection.SetBorderWidth(window->Id(), window->BorderWidth());
            m_connection.Raise(window->Id());
            continue;
        }

        m_connection.MoveResizeWindow(window->Id(), window->Geometry());
        m_connection.SetBorderWidth(window->Id(), window->BorderWidth());
    }

    if (fullscreenWindow)
    {
        m_connection.MoveResizeWindow(fullscreenWindow->Id(), monitor);
        m_connection.SetBorderWidth(fullscreenWindow->Id(), 0);
        m_connection.Raise(fullscreenWindow->Id());

        // Keep bookkeeping (IPC/`kohikoctl clients` reporting) in sync
        // with what's actually on screen - the tree still computed a
        // tiled-slot rect for this leaf above, but that's not where
        // the window really is right now.
        fullscreenWindow->SetGeometry(monitor);
    }

    m_bar.SetWorkspaces(m_workspaces.Count(), m_workspaces.CurrentId());
    m_bar.SetScratchpadActive(m_scratchpad.HasWindow() && m_scratchpad.IsVisible());
    m_bar.SetNotepadActive(m_notepad.HasContent());
    m_bar.Redraw();

    // Every Raise() above (a newly-mapped window, a floating window,
    // a fullscreen window) happened *after* the Launcher/Notepad were
    // last raised, which - left alone - would leave whichever one is
    // currently open stacked underneath something else even though it
    // still holds real input focus. Always give it the last word on
    // stacking order.
    RaiseModalWindows();

    m_connection.Flush();
}

Rect WindowManager::TilingArea()
{
    const Rect& monitor = m_monitors.Primary().Geometry();

    Rect area = monitor;
    area.y += m_bar.Height();
    area.height -= m_bar.Height();

    return area;
}

void WindowManager::RefreshWorkspaceGeometry(int workspaceId)
{
    Workspace& workspace = m_workspaces.Get(workspaceId);

    if (workspace.Tree().Empty())
        return;

    LayoutEngine::Params params;
    params.innerGap     = m_config.GetInt("general.inner_gap", 6);
    params.outerGap     = m_config.GetInt("general.outer_gap", 8);
    params.borderWidth  = m_config.GetInt("general.border_size", 2);
    params.smartGaps    = m_config.GetBool("general.smart_gaps", true);
    params.smartBorders = m_config.GetBool("general.smart_borders", true);

    // Same computation Arrange() would do for the current workspace -
    // just without the X11 MoveResizeWindow/MapWindow side that only
    // makes sense for whatever's actually on screen right now.
    m_layout.Apply(workspace.Tree().Root(), TilingArea(), params);
}

void WindowManager::SwitchWorkspace(int id)
{
    if (!m_workspaces.Switch(id))
        return;

    int previous = m_workspaces.PreviousId();

    for (ManagedWindow* window : m_repository.Visible(previous))
    {
        window->IgnoreNextUnmap();
        m_connection.UnmapWindow(window->Id());
    }

    // Arrange() before mapping, not after - see the matching comment
    // in Manage(). Windows on this workspace that were opened while it
    // was in the background never had MoveResizeWindow() applied to
    // their actual X window (only the tree's cached rect was kept
    // fresh, by RefreshWorkspaceGeometry()), so without this they'd be
    // mapped at whatever size they were created at and resized to the
    // real tile slot a moment later - the exact "content stuck in a
    // corner" symptom, just triggered by switching workspaces instead
    // of opening the window.
    Arrange();

    for (ManagedWindow* window : m_repository.Visible(m_workspaces.CurrentId()))
        m_connection.MapWindow(window->Id());

    auto visible = m_repository.Visible(m_workspaces.CurrentId());

    if (!visible.empty())
    {
        Focus(visible.front()->Id());
    }
    else
    {
        m_repository.ClearFocus();
        m_connection.SetInputFocus(m_connection.Root());
        m_bar.SetTitle("");
        m_bar.Redraw();
    }
}

void WindowManager::MoveFocusedToWorkspace(int id)
{
    ManagedWindow* window = m_repository.Focused();

    if (!window || id == window->Workspace() || id < 1 || id > m_workspaces.Count())
        return;

    int oldWorkspaceId = window->Workspace();

    bool wasTiled = window->IsTiled();
    bool wasFullscreenTile =
        window->IsFullscreen() && window->PreviousState() == WindowState::Tiled;

    if (wasTiled || wasFullscreenTile)
        m_workspaces.Get(oldWorkspaceId).Tree().Remove(window);

    window->SetWorkspace(id);

    if (wasTiled && !TryTile(window, id))
    {
        // TryTile() only fails the capacity check, so the window isn't
        // in any tree right now - land it floating instead of forcing
        // an undersized tile. Explicit user intent ("move THIS window
        // to workspace `id`") still wins: it lands on workspace `id`,
        // just not tiled there.
        window->SetState(WindowState::Floating);

        Rect geometry = window->FloatingGeometry();

        if (geometry.width <= 0 || geometry.height <= 0)
            geometry = CenteredFloatingRect(0.5f, 0.5f);

        window->SetGeometry(geometry);
        window->SetFloatingGeometry(geometry);

        Logger::Warning(
            "Workspace " + std::to_string(id) +
            " has no room for another tile without dropping below the "
            "configured minimum size - moved window " +
            std::to_string(static_cast<unsigned long>(window->Id())) +
            " there floating instead.");
    }
    else if (wasFullscreenTile)
    {
        // A window mid-fullscreen (Telegram's media viewer, a video
        // player, ...) should stay fullscreen right through a move to
        // another workspace, exactly as if it had just stayed put -
        // nothing above touched State() for this branch. All that's
        // still needed is a freshly reserved tiled slot on the *new*
        // workspace for it to restore into whenever it eventually
        // un-fullscreens, the same one TryTile() would hand a window
        // that opened there directly (see OccupiesTreeSlot()'s
        // comment for why this can't just reuse the old workspace's
        // now-removed leaf).
        if (TryTile(window, id))
        {
            // TryTile() always sets Tiled - it's still genuinely
            // fullscreen right now, so put that straight back.
            window->SetState(WindowState::Fullscreen);
        }
        else
        {
            // No room even for a restore slot on the target workspace -
            // the graceful fallback is exactly what un-fullscreening
            // onto an already-full workspace does elsewhere: land
            // floating instead, whenever it does eventually un-fullscreen.
            window->SetPreviousState(WindowState::Floating);
        }
    }

    window->IgnoreNextUnmap();
    m_connection.UnmapWindow(window->Id());

    Arrange();
    FocusNextAvailable();
}

void WindowManager::ToggleFloating()
{
    ManagedWindow* window = m_repository.Focused();

    if (!window || window->IsFullscreen())
        return;

    if (window->IsScratchpad())
    {
        // Release it from the scratchpad slot into an ordinary
        // floating window on the current workspace.
        m_scratchpad.Forget(window->Id());
        window->SetWorkspace(m_workspaces.CurrentId());
        window->SetState(WindowState::Floating);
        m_bar.SetScratchpadActive(false);
    }
    else if (window->IsTiled())
    {
        m_workspaces.Get(window->Workspace()).Tree().Remove(window);

        Rect geometry = window->FloatingGeometry();

        if (geometry.width <= 0 || geometry.height <= 0)
            geometry = CenteredFloatingRect(0.5f, 0.5f);

        window->SetGeometry(geometry);
        window->SetFloatingGeometry(geometry);
        window->SetState(WindowState::Floating);
    }
    else if (window->IsFloating())
    {
        window->SetFloatingGeometry(window->Geometry());

        if (!TryTile(window, window->Workspace()))
        {
            // No room to tile it right now (bug #4's capacity check) -
            // simplest safe behaviour is to just leave it floating
            // rather than forcing an undersized tile; the user can
            // try again once something else closes.
            Logger::Warning(
                "Workspace " + std::to_string(window->Workspace()) +
                " has no room for another tile right now - window " +
                std::to_string(static_cast<unsigned long>(window->Id())) +
                " stays floating.");
        }
    }

    Arrange();
}

void WindowManager::ToggleFullscreen()
{
    ManagedWindow* window = m_repository.Focused();

    if (!window)
        return;

    if (window->IsFullscreen())
        window->SetState(window->PreviousState());
    else
    {
        window->SetPreviousState(window->State());
        window->SetState(WindowState::Fullscreen);
    }

    Arrange();
}

void WindowManager::ToggleScratchpadForFocused()
{
    if (!m_scratchpad.HasWindow())
    {
        ManagedWindow* window = m_repository.Focused();

        if (!window || window->IsScratchpad())
            return;

        if (window->OccupiesTreeSlot())
            m_workspaces.Get(window->Workspace()).Tree().Remove(window);

        window->SetPreviousState(window->State());
        window->SetState(WindowState::Scratchpad);

        m_scratchpad.Assign(window->Id());

        window->IgnoreNextUnmap();
        m_connection.UnmapWindow(window->Id());

        Arrange();
        FocusNextAvailable();
        return;
    }

    bool nowVisible = m_scratchpad.Toggle();
    ManagedWindow* window = m_repository.Get(m_scratchpad.Window());

    if (!window)
        return;

    if (nowVisible)
    {
        float widthFraction  = m_config.GetPercent("scratchpad.width",  0.7f);
        float heightFraction = m_config.GetPercent("scratchpad.height", 0.7f);

        Rect geometry = CenteredFloatingRect(widthFraction, heightFraction);
        window->SetGeometry(geometry);

        m_connection.MoveResizeWindow(window->Id(), geometry);
        m_connection.SetBorderWidth(window->Id(), window->BorderWidth());
        m_connection.MapWindow(window->Id());
        m_connection.Raise(window->Id());
        RaiseModalWindows();

        Focus(window->Id());

        m_bar.SetScratchpadActive(true);
        m_bar.Redraw();
    }
    else
    {
        window->IgnoreNextUnmap();
        m_connection.UnmapWindow(window->Id());

        m_bar.SetScratchpadActive(false);

        if (window->Focused())
            FocusNextAvailable();

        m_bar.Redraw();
    }
}

void WindowManager::CloseFocused()
{
    ManagedWindow* window = m_repository.Focused();

    if (!window)
        return;

    m_connection.CloseWindow(window->Id(), m_atoms);
}

void WindowManager::FocusDirection(Direction direction)
{
    ManagedWindow* window = m_repository.Focused();

    if (!window || !window->IsTiled())
        return;

    ManagedWindow* neighbor =
        m_workspaces.Get(window->Workspace()).Tree().FindNeighbor(window, direction);

    if (neighbor)
        Focus(neighbor->Id());
}

void WindowManager::RotateFocused()
{
    ManagedWindow* window = m_repository.Focused();

    if (!window || !window->IsTiled())
        return;

    m_workspaces.Get(window->Workspace()).Tree().Rotate(window);
    Arrange();
}

void WindowManager::FlipFocused()
{
    ManagedWindow* window = m_repository.Focused();

    if (!window || !window->IsTiled())
        return;

    m_workspaces.Get(window->Workspace()).Tree().Flip(window);
    Arrange();
}

void WindowManager::ReloadConfig()
{
    if (!m_config.Load(m_configPath))
        return;

    m_keyboard.Configure(m_config);
    m_mouse.Configure(m_config);

    m_windowRules = LoadWindowRules(m_config);

    m_fileManager =
        m_config.GetString(
            "launcher.file_manager",
            "pcmanfm");

    // Re-applying layouts is harmless (setxkbmap just sets the same
    // thing again) so someone editing `keyboard.layouts` sees it take
    // effect on reload like everything else - RunAutostart() is
    // deliberately NOT called here, see its declaration in the header.
    ApplyKeyboardLayouts();

    Arrange();
}

void WindowManager::ApplyKeyboardLayouts()
{
    std::vector<std::string> layouts =
        Utils::SplitWhitespace(
            m_config.GetString("keyboard.layouts", "us"));

    if (layouts.empty())
        return;

    std::string joined;

    for (std::size_t i = 0; i < layouts.size(); ++i)
    {
        // `keyboard.layouts=` is meant to be comma-separated
        // (matching setxkbmap's own -layout syntax), but a stray
        // space after a comma shouldn't silently break the whole
        // list, so tokenize on whitespace first and then strip any
        // trailing/leading commas off each piece before rejoining.
        std::string token = layouts[i];

        while (!token.empty() && token.front() == ',')
            token.erase(token.begin());

        while (!token.empty() && token.back() == ',')
            token.pop_back();

        if (token.empty())
            continue;

        if (!joined.empty())
            joined += ",";

        joined += token;
    }

    if (joined.empty())
        return;

    std::string command = "setxkbmap -layout " + joined;

    // Only meaningful (and only valid setxkbmap syntax) once there's
    // more than one layout to switch between - with a single layout
    // there's nothing to toggle.
    if (joined.find(',') != std::string::npos)
    {
        std::string toggle =
            m_config.GetString(
                "keyboard.layout_toggle",
                "grp:alt_shift_toggle");

        if (!toggle.empty())
            command += " -option " + toggle;
    }

    Process::Spawn(command, m_connection.DisplayName());
}

void WindowManager::RunAutostart()
{
    for (const std::string& program :
         Utils::SplitWhitespace(
             m_config.GetString("auto_start_programs")))
    {
        Process::Spawn(program, m_connection.DisplayName());
    }
}

void WindowManager::ToggleLauncher()
{
    if (m_launcher.IsOpen())
    {
        CloseLauncher(false);
        return;
    }

    if (m_notepad.IsOpen())
        CloseNotepad();

    ManagedWindow* focused = m_repository.Focused();
    m_focusBeforeModal = focused ? focused->Id() : 0;

    m_launcher.Open();
    m_connection.SetInputFocus(m_launcher.WindowId());
}

void WindowManager::CloseLauncher(bool run)
{
    if (!m_launcher.IsOpen())
        return;

    bool isFile =
        m_launcher.SelectedIsFile();

    std::string command =
        m_launcher.SelectedCommand();

    std::string path =
        m_launcher.SelectedPath();

    m_launcher.Close();

    if (run)
    {
        if (isFile)
{
    namespace fs = std::filesystem;

    std::string folder =
        fs::path(path)
            .parent_path()
            .string();

    Process::Spawn(
        m_fileManager + " \"" + folder + "\"",
        m_connection.DisplayName());
}
        else if (!command.empty())
        {
            Process::Spawn(
                command,
                m_connection.DisplayName());
        }
    }

    RestoreFocusAfterModal();
}

void WindowManager::ToggleNotepad()
{
    if (m_notepad.IsOpen())
    {
        CloseNotepad();
        return;
    }

    if (m_launcher.IsOpen())
        CloseLauncher(false);

    ManagedWindow* focused = m_repository.Focused();
    m_focusBeforeModal = focused ? focused->Id() : 0;

    m_notepad.Open();
    m_bar.SetNotepadActive(true);
    m_bar.Redraw();
    m_connection.SetInputFocus(m_notepad.WindowId());
}

void WindowManager::CloseNotepad()
{
    if (!m_notepad.IsOpen())
        return;

    m_notepad.Close();
    m_bar.SetNotepadActive(m_notepad.HasContent());
    m_bar.Redraw();

    RestoreFocusAfterModal();
}

void WindowManager::RestoreFocusAfterModal()
{
    if (m_focusBeforeModal != 0 && m_repository.Contains(m_focusBeforeModal))
        Focus(m_focusBeforeModal);
    else
        FocusNextAvailable();

    m_focusBeforeModal = 0;
}

bool WindowManager::TryTile(ManagedWindow* window, int workspaceId)
{
    if (!window || workspaceId < 1 || workspaceId > m_workspaces.Count())
        return false;

    int innerGap  = m_config.GetInt("general.inner_gap", 6);
    int outerGap  = m_config.GetInt("general.outer_gap", 8);

    // MIN_USABLE_TILE_WIDTH/HEIGHT: the floor no *other*, already-
    // tiled window is ever allowed to shrink past on `window`'s
    // account. `window`'s own (possibly larger) declared minimum is
    // handled by BSPTree::Insert() itself, via ManagedWindow::
    // MinWidth()/MinHeight() - see BSPTree.h's EffectiveMinSize() for
    // why the two are deliberately kept separate.
    int floorWidth  = m_config.GetInt("general.min_tile_width", 100);
    int floorHeight = m_config.GetInt("general.min_tile_height", 60);

    // Matches LayoutEngine::Apply's own Shrunk(outerGap) exactly, so
    // this and the real layout can never disagree about what a
    // freshly-split leaf's rect would be.
    Rect tilingArea = TilingArea().Shrunk(outerGap);

    // BSPTree::Insert()'s placement-aware overload is the single
    // choke point for "Try Current Layout -> Try Alternative Layouts
    // -> Shrink Existing Tiles" - it only mutates the tree (and only
    // ever down to the floor, never further) if it actually finds a
    // legal placement, and returns false untouched otherwise.
    if (!m_workspaces.Get(workspaceId).Tree().Insert(window, tilingArea, innerGap, floorWidth, floorHeight))
        return false;

    window->SetWorkspace(workspaceId);
    window->SetState(WindowState::Tiled);

    // A freshly-tiled window starts this tile's "is it fighting its
    // geometry" count at zero, regardless of whatever it racked up in
    // a previous tile.
    window->ResetTilingMisbehavior();

    // Arrange() (called by every caller of TryTile() right after) only
    // ever lays out the *current* workspace - for any other, keep its
    // cached node geometry fresh ourselves so the next insertion's
    // AnchorLeaf()/DirectionForRect() has something real to work from
    // instead of a stale {0,0,0,0}.
    if (workspaceId != m_workspaces.CurrentId())
        RefreshWorkspaceGeometry(workspaceId);

    return true;
}

int WindowManager::FindWorkspaceWithRoom(ManagedWindow* window, int excludeId)
{
    int innerGap  = m_config.GetInt("general.inner_gap", 6);
    int outerGap  = m_config.GetInt("general.outer_gap", 8);
    int floorWidth  = m_config.GetInt("general.min_tile_width", 100);
    int floorHeight = m_config.GetInt("general.min_tile_height", 60);

    Rect tilingArea = TilingArea().Shrunk(outerGap);

    for (int id = 1; id <= m_workspaces.Count(); ++id)
    {
        if (id == excludeId)
            continue;

        if (m_workspaces.Get(id).Tree().HasSpaceForAnotherWindow(window, tilingArea, innerGap, floorWidth, floorHeight))
            return id;
    }

    return 0;
}

int WindowManager::FindWorkspaceWithFewestWindows(int excludeId)
{
    int best = 0;
    std::size_t bestCount = 0;

    // Ascending id order both gives the natural "first available one"
    // tie-break the spec asks for and makes the scan deterministic.
    for (int id = 1; id <= m_workspaces.Count(); ++id)
    {
        if (id == excludeId)
            continue;

        std::size_t count = m_repository.Workspace(id).size();

        if (best == 0 || count < bestCount)
        {
            best = id;
            bestCount = count;
        }
    }

    return best;
}

void WindowManager::ApplyTilingMisbehaviorFallback(ManagedWindow* window)
{
    if (!window)
        return;

    int oldWorkspace = window->Workspace();

    // Stop assigning fixed tiled geometry entirely - remove it from
    // the tree here and never put it back into any tile, on any
    // workspace, as part of this call. Simply moving it into a
    // *different* tile would leave it exactly as likely to fight that
    // one too; the whole point of this fallback is landing it
    // somewhere floating instead.
    if (window->OccupiesTreeSlot())
        m_workspaces.Get(oldWorkspace).Tree().Remove(window);

    window->ResetTilingMisbehavior();

    std::string fallback = Utils::Lower(m_config.GetString("general.tiling_misbehavior_fallback", "floating"));

    int targetWorkspace = oldWorkspace;

    if (fallback == "new_workspace")
    {
        int candidate = FindWorkspaceWithFewestWindows(oldWorkspace);

        if (candidate != 0)
            targetWorkspace = candidate;
    }

    window->SetWorkspace(targetWorkspace);
    window->SetState(WindowState::Floating);

    XWindowAttributes attrs{};
    m_connection.GetWindowAttributes(window->Id(), attrs);

    Rect geometry = CenteredFloatingRectForWindow(window->Id(), attrs);
    window->SetGeometry(geometry);
    window->SetFloatingGeometry(geometry);

    Logger::Warning(
        "Window " + std::to_string(static_cast<unsigned long>(window->Id())) +
        (window->ClassName().empty() ? "" : " (" + window->ClassName() + ")") +
        " repeatedly sent geometry requests conflicting with its tiled slot - "
        "switching it to floating" +
        (targetWorkspace != oldWorkspace
            ? " on workspace " + std::to_string(targetWorkspace)
            : "") +
        " instead of continuing to force it back into a tile.");

    // Moving off the workspace that's actually on screen right now
    // needs the same unmap-on-move handling MoveFocusedToWorkspace()
    // uses; staying on the same workspace, or moving off one that
    // isn't current anyway, needs no extra unmap.
    if (targetWorkspace != oldWorkspace && oldWorkspace == m_workspaces.CurrentId())
    {
        window->IgnoreNextUnmap();
        m_connection.UnmapWindow(window->Id());
    }

    Arrange();
}

void WindowManager::RefreshBorderColor(ManagedWindow* window)
{
    if (!window || window->BorderWidth() <= 0)
        return;

    unsigned long activeColor   = ParseColor("general.border_color_active",   "0x89b4fa");
    unsigned long inactiveColor = ParseColor("general.border_color_inactive", "0x45475a");

    m_connection.SetBorderColor(window->Id(), window->Focused() ? activeColor : inactiveColor);
}

Rect WindowManager::CenteredFloatingRect(float widthFraction, float heightFraction)
{
    const Rect& monitor = m_monitors.Primary().Geometry();
    int borderWidth = m_config.GetInt("general.border_size", 2);

    Rect rect;
    rect.width  = static_cast<int>(static_cast<float>(monitor.width)  * widthFraction);
    rect.height = static_cast<int>(static_cast<float>(monitor.height) * heightFraction);
    rect.x = monitor.x + (monitor.width  - rect.width)  / 2;
    rect.y = monitor.y + (monitor.height - rect.height) / 2;

    return rect.ClampedTo(monitor, borderWidth);
}

Rect WindowManager::CenteredFloatingRectForWindow(
    WindowID id,
    const XWindowAttributes& attrs,
    ManagedWindow* parent)
{
    int width = 0;
    int height = 0;

    // Prefer what the client itself actually asked for - WM_NORMAL_HINTS
    // where it set one, otherwise whatever size it already had when it
    // asked to be mapped (every toolkit creates its top-level window at
    // its real intended size before mapping it; attrs is read at that
    // exact moment by Manage()). A fixed-size dialog, an image viewer
    // sized to the picture it's showing, a settings window with a
    // sensible default size - all of them should open at *that* size,
    // centered, instead of being squashed or stretched into a flat 50%
    // of the screen regardless of what they actually contain.
    if (!m_connection.GetPreferredSize(id, width, height))
    {
        width = attrs.width;
        height = attrs.height;
    }

    const Rect& monitor = m_monitors.Primary().Geometry();
    int borderWidth = m_config.GetInt("general.border_size", 2);

    // A handful of windows genuinely don't have any usable size to go
    // on (0, or something too small to be more than a sliver) - rather
    // than ever placing an unusable window, fall back to the old
    // half-the-screen default exactly like before this existed. A
    // transient parent still gets to say *where* (centered over it
    // rather than the whole monitor) even when it can't say *how big*.
    if (width < 50 || height < 30)
    {
        if (!parent)
            return CenteredFloatingRect(0.5f, 0.5f);

        Rect rect;
        rect.width  = static_cast<int>(static_cast<float>(monitor.width)  * 0.5f);
        rect.height = static_cast<int>(static_cast<float>(monitor.height) * 0.5f);
        rect.x = parent->Geometry().CenterX() - rect.width  / 2;
        rect.y = parent->Geometry().CenterY() - rect.height / 2;

        return rect.ClampedTo(monitor, borderWidth);
    }

    // Never let a window's own idea of its size push it off-screen -
    // still centered, just clamped to comfortably fit (a soft 95% cap
    // first, so it visibly has room to breathe rather than butting
    // right up against the edge), then hard-clamped against the real
    // monitor bounds - accounting for the border ClampedTo() adds
    // outward from width/height - as the actual Geometry Rules
    // guarantee that must hold no matter what a misbehaving client's
    // own hints claimed.
    int maxWidth  = static_cast<int>(static_cast<float>(monitor.width)  * 0.95f);
    int maxHeight = static_cast<int>(static_cast<float>(monitor.height) * 0.95f);

    if (width  > maxWidth)  width  = maxWidth;
    if (height > maxHeight) height = maxHeight;

    Rect rect;
    rect.width  = width;
    rect.height = height;

    if (parent)
    {
        // Center on the parent's own current rect rather than the
        // monitor - see the header comment on this overload: a GIMP
        // color picker or a file-save dialog should appear right over
        // the window that spawned it, not wherever happens to be the
        // middle of the screen. Still clamped to the monitor below, so
        // a parent sitting near an edge can never push the child
        // partly off-screen.
        rect.x = parent->Geometry().CenterX() - width  / 2;
        rect.y = parent->Geometry().CenterY() - height / 2;
    }
    else
    {
        rect.x = monitor.x + (monitor.width  - width)  / 2;
        rect.y = monitor.y + (monitor.height - height) / 2;
    }

    return rect.ClampedTo(monitor, borderWidth);
}

WindowRuleEffect WindowManager::ResolveWindowRules(
    const std::string& className,
    const std::string& instanceName,
    const std::string& title) const
{
    WindowRuleEffect effect;

    for (const WindowRule& rule : m_windowRules)
    {
        if (!rule.Matches(className, instanceName, title))
            continue;

        switch (rule.action)
        {
            case WindowRule::Action::Float:       effect.forceFloat      = true;          break;
            case WindowRule::Action::Tile:        effect.forceTile       = true;          break;
            case WindowRule::Action::Fullscreen:  effect.forceFullscreen = true;           break;
            case WindowRule::Action::NoFullscreen: effect.denyFullscreen  = true;          break;
            case WindowRule::Action::Workspace:   effect.forceWorkspace  = rule.workspace; break;
        }
    }

    return effect;
}

void WindowManager::RaiseModalWindows()
{
    if (m_launcher.IsOpen())
        m_connection.Raise(m_launcher.WindowId());
    else if (m_notepad.IsOpen())
        m_connection.Raise(m_notepad.WindowId());
}

unsigned long WindowManager::ParseColor(const std::string& key, const std::string& fallback) const
{
    return std::strtoul(m_config.GetString(key, fallback).c_str(), nullptr, 0);
}

std::string WindowManager::DumpClientsJson() const
{
    std::ostringstream out;
    out << "[";

    bool first = true;

    for (ManagedWindow* window : m_repository.All())
    {
        if (!first)
            out << ",";

        first = false;

        const Rect& r = window->Geometry();

        out << "{"
            << "\"id\":" << static_cast<unsigned long>(window->Id()) << ","
            << "\"title\":" << Json::String(window->Title()) << ","
            << "\"class\":" << Json::String(window->ClassName()) << ","
            << "\"instance\":" << Json::String(window->InstanceName()) << ","
            << "\"pid\":" << window->Pid() << ","
            << "\"workspace\":" << window->Workspace() << ","
            << "\"monitor\":" << window->Monitor() << ","
            << "\"x\":" << r.x << ",\"y\":" << r.y
            << ",\"width\":" << r.width << ",\"height\":" << r.height << ","
            << "\"floating\":" << Json::Boolean(window->IsFloating()) << ","
            << "\"fullscreen\":" << Json::Boolean(window->IsFullscreen()) << ","
            << "\"scratchpad\":" << Json::Boolean(window->IsScratchpad()) << ","
            << "\"focused\":" << Json::Boolean(window->Focused())
            << "}";
    }

    out << "]";
    return out.str();
}

std::string WindowManager::DumpMonitorsJson() const
{
    std::ostringstream out;
    out << "[";

    bool first = true;

    for (const auto& monitor : m_monitors.All())
    {
        if (!first)
            out << ",";

        first = false;

        const Rect& r = monitor->Geometry();

        out << "{"
            << "\"id\":" << monitor->Id() << ","
            << "\"x\":" << r.x << ",\"y\":" << r.y
            << ",\"width\":" << r.width << ",\"height\":" << r.height
            << "}";
    }

    out << "]";
    return out.str();
}

std::string WindowManager::DumpActiveWindowJson() const
{
    ManagedWindow* window = m_repository.Focused();

    if (!window)
        return "null";

    const Rect& r = window->Geometry();

    std::ostringstream out;
    out << "{"
        << "\"id\":" << static_cast<unsigned long>(window->Id()) << ","
        << "\"title\":" << Json::String(window->Title()) << ","
        << "\"class\":" << Json::String(window->ClassName()) << ","
        << "\"workspace\":" << window->Workspace() << ","
        << "\"x\":" << r.x << ",\"y\":" << r.y
        << ",\"width\":" << r.width << ",\"height\":" << r.height
        << "}";

    return out.str();
}

::Window WindowManager::LauncherWindowId() const
{
    return m_launcher.WindowId();
}

}
