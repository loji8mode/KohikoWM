#include "WindowManager.h"

#include "Command.h"
#include "Config.h"
#include "IpcPath.h"
#include "Json.h"
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
    m_bar(connection)
{
}

void WindowManager::Initialize()
{
    m_atoms.Initialize();
    m_cursor.Initialize();
    m_monitors.Detect();

    m_bar.Configure(m_config, m_monitors.Primary().Geometry());
    m_bar.Show();

    m_keyboard.Configure(m_config);
    m_mouse.Configure(m_config);

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
    m_bar.Redraw();
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
            Process::Spawn(resolved.empty() ? command.stringArg : resolved);
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
    else
    {
        window->SetState(WindowState::Tiled);
        m_workspaces.Get(window->Workspace()).Tree().Insert(window);
    }

    m_connection.MapWindow(id);

    Arrange();
    Focus(id);
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

    unsigned long activeColor   = ParseColor("general.border_color_active",   "0x89b4fa");
    unsigned long inactiveColor = ParseColor("general.border_color_inactive", "0x45475a");

    if (window->BorderWidth() > 0)
        m_connection.SetBorderColor(id, activeColor);

    for (ManagedWindow* other : m_repository.Visible(m_workspaces.CurrentId()))
    {
        if (other->Id() != id && other->BorderWidth() > 0)
            m_connection.SetBorderColor(other->Id(), inactiveColor);
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

    Rect tilingArea = monitor;
    tilingArea.y += m_bar.Height();
    tilingArea.height -= m_bar.Height();

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
    m_bar.Redraw();

    m_connection.Flush();
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

    if (window->IsTiled())
        m_workspaces.Get(oldWorkspaceId).Tree().Remove(window);

    window->SetWorkspace(id);

    if (window->IsTiled())
        m_workspaces.Get(id).Tree().Insert(window);

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
        window->SetState(WindowState::Tiled);
        m_workspaces.Get(window->Workspace()).Tree().Insert(window);
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

    Arrange();
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

}
