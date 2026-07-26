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

    m_bar.Configure(m_config, m_monitors.Primary().Geometry());
    m_bar.Show();

    m_tray.Initialize(m_connection, m_bar.WindowId());
    m_bar.AttachSystemTray(&m_tray);

    m_launcher.Configure(m_config, m_monitors.Primary().Geometry());
    m_notepad.Configure(m_config, m_monitors.Primary().Geometry());
    m_bar.SetNotepadActive(m_notepad.HasContent());

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
        XWindowChanges changes{};
        changes.x = event.x;
        changes.y = event.y;
        changes.width = event.width;
        changes.height = event.height;
        changes.border_width = event.border_width;
        changes.sibling = event.above;
        changes.stack_mode = event.detail;

        m_connection.ConfigureWindowRaw(event.window, changes, static_cast<unsigned int>(event.value_mask));

        if (window)
        {
            Rect rect{event.x, event.y, event.width, event.height};
            window->SetGeometry(rect);
            window->SetFloatingGeometry(rect);
        }

        return;
    }

    // Tiled: we own the geometry - acknowledge with what it actually
    // is so the client doesn't sit waiting for a reply.
    m_connection.SendConfigureNotify(event.window, window->Geometry(), window->BorderWidth());
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

    m_connection.SelectInputFor(id, EnterWindowMask | PropertyChangeMask | FocusChangeMask);

    // Plain (no Super) click-to-focus: sync-grabbed so we always get
    // first refusal, then replayed so the client still sees the click.
    for (unsigned int variant : m_connection.LockVariants(0))
        m_connection.GrabButtonOnWindow(id, Button1, variant);

    WindowID transientOwner = 0;
    bool isTransient = m_connection.GetTransientFor(id, transientOwner);
    bool isDialog = m_connection.IsDialog(id, m_atoms);

    if (isTransient || isDialog)
    {
        window->SetState(WindowState::Floating);

        Rect geometry = CenteredFloatingRect(0.5f, 0.5f);
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
        int alternate = FindWorkspaceWithRoom(currentWorkspace);

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
            // Every workspace is full. This is the designated
            // extension point for something smarter later (auto-
            // creating a new workspace, prompting the user, etc) -
            // for now, the fallback that actually guarantees "no
            // window ever goes beyond the screen" is to open it
            // floating instead of forcing an undersized tile.
            window->SetState(WindowState::Floating);

            Rect geometry = CenteredFloatingRect(0.5f, 0.5f);
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
        m_connection.MapWindow(id);
        Arrange();
        Focus(id);
    }
    else
    {
        Arrange();
    }
}

void WindowManager::Unmanage(WindowID id)
{
    ManagedWindow* window = m_repository.Get(id);

    if (!window)
        return;

    bool wasFocused = window->Focused();
    int workspace = window->Workspace();

    if (window->IsTiled())
        m_workspaces.Get(workspace).Tree().Remove(window);

    m_scratchpad.Forget(id);
    m_repository.Remove(id);

    if (wasFocused)
        FocusNextAvailable();

    Arrange();
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

    for (ManagedWindow* window : m_repository.Visible(m_workspaces.CurrentId()))
        m_connection.MapWindow(window->Id());

    Arrange();

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

    if (wasTiled)
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

        if (window->IsTiled())
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

    m_fileManager =
        m_config.GetString(
            "launcher.file_manager",
            "pcmanfm");

    Arrange();
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
    int minWidth  = m_config.GetInt("general.min_tile_width", 100);
    int minHeight = m_config.GetInt("general.min_tile_height", 60);

    // Matches LayoutEngine::Apply's own Shrunk(outerGap) exactly, so
    // this check and the real layout can never disagree about what a
    // freshly-split leaf's rect would be.
    Rect tilingArea = TilingArea().Shrunk(outerGap);

    if (!m_workspaces.Get(workspaceId).Tree().HasSpaceForAnotherWindow(tilingArea, innerGap, minWidth, minHeight))
        return false;

    window->SetWorkspace(workspaceId);
    window->SetState(WindowState::Tiled);
    m_workspaces.Get(workspaceId).Tree().Insert(window);

    // Arrange() (called by every caller of TryTile() right after) only
    // ever lays out the *current* workspace - for any other, keep its
    // cached node geometry fresh ourselves so the next insertion's
    // AnchorLeaf()/DirectionForRect() has something real to work from
    // instead of a stale {0,0,0,0}.
    if (workspaceId != m_workspaces.CurrentId())
        RefreshWorkspaceGeometry(workspaceId);

    return true;
}

int WindowManager::FindWorkspaceWithRoom(int excludeId)
{
    int innerGap  = m_config.GetInt("general.inner_gap", 6);
    int outerGap  = m_config.GetInt("general.outer_gap", 8);
    int minWidth  = m_config.GetInt("general.min_tile_width", 100);
    int minHeight = m_config.GetInt("general.min_tile_height", 60);

    Rect tilingArea = TilingArea().Shrunk(outerGap);

    for (int id = 1; id <= m_workspaces.Count(); ++id)
    {
        if (id == excludeId)
            continue;

        if (m_workspaces.Get(id).Tree().HasSpaceForAnotherWindow(tilingArea, innerGap, minWidth, minHeight))
            return id;
    }

    return 0;
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

    Rect rect;
    rect.width  = static_cast<int>(static_cast<float>(monitor.width)  * widthFraction);
    rect.height = static_cast<int>(static_cast<float>(monitor.height) * heightFraction);
    rect.x = monitor.x + (monitor.width  - rect.width)  / 2;
    rect.y = monitor.y + (monitor.height - rect.height) / 2;

    return rect;
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
