#include "SessionStore.h"

#include "ManagedWindow.h"
#include "Monitor.h"
#include "MonitorManager.h"
#include "Xdg.h"

#include <fstream>

namespace Kohiko
{

namespace
{

std::filesystem::path SessionFilePath()
{
    return Xdg::DataDir() / "session";
}

// A blank XRandr output name isn't representable as a whitespace-
// delimited token on its own, so it's written/read as this sentinel
// instead - the same reasoning HistoryStore's own flat format relies
// on for "these tokens never contain whitespace in practice", just
// with an explicit stand-in for the one field that can legitimately
// be empty.
const char* const kNoMonitorName = "-";

}

SessionStore::SessionStore()
{
    Load();
}

void SessionStore::Load()
{
    m_records.clear();

    std::ifstream file(SessionFilePath());

    WindowID id;
    int workspace;
    int floating;
    int fullscreen;
    Rect geometry;
    std::string monitorName;

    while (file >> id >> workspace >> floating >> fullscreen >>
                   geometry.x >> geometry.y >> geometry.width >> geometry.height >>
                   monitorName)
    {
        SessionWindowState state;
        state.workspace = workspace;
        state.floating = floating != 0;
        state.fullscreen = fullscreen != 0;
        state.floatingGeometry = geometry;
        state.monitorName = (monitorName == kNoMonitorName) ? std::string() : monitorName;

        m_records[id] = state;
    }
}

const SessionWindowState* SessionStore::Find(
    WindowID id) const
{
    auto it = m_records.find(id);
    return (it != m_records.end()) ? &it->second : nullptr;
}

void SessionStore::Save(
    const std::vector<ManagedWindow*>& windows,
    const MonitorManager& monitors)
{
    std::ofstream file(SessionFilePath(), std::ios::trunc);

    for (ManagedWindow* window : windows)
    {
        // The scratchpad's own single-slot state (which window, shown
        // or hidden) isn't part of what session restore covers - see
        // this class's header comment. A scratchpad-assigned window
        // just gets adopted back as an ordinary window on restart.
        if (window->IsScratchpad())
            continue;

        Monitor* monitor = monitors.Find(window->Monitor());
        const std::string& monitorName = monitor ? monitor->Name() : std::string();

        file << static_cast<unsigned long>(window->Id()) << ' '
             << window->Workspace() << ' '
             << (window->IsFloating() ? 1 : 0) << ' '
             << (window->IsFullscreen() ? 1 : 0) << ' '
             << window->FloatingGeometry().x << ' '
             << window->FloatingGeometry().y << ' '
             << window->FloatingGeometry().width << ' '
             << window->FloatingGeometry().height << ' '
             << (monitorName.empty() ? kNoMonitorName : monitorName.c_str()) << '\n';
    }
}

}
