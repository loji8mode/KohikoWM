#pragma once

#include "Monitor.h"

#include <memory>
#include <vector>

namespace Kohiko
{

class XConnection;
class WorkspaceManager;

// Detects physical outputs via XRandR when it was available at build
// time (KOHIKO_HAVE_XRANDR) and falls back to treating the whole X
// display as a single monitor otherwise. Geometry is correct either
// way, which is what the bar / fullscreen / gap math need; truly
// independent per-monitor tiling is basic for now (see README).
class MonitorManager
{
public:

    MonitorManager(
        XConnection& connection,
        WorkspaceManager& workspaces
    );

    void Detect();

    Monitor& Primary();

    const std::vector<
        std::unique_ptr<Monitor>>&
    All() const;

private:

    XConnection& m_connection;

    WorkspaceManager& m_workspaces;

    std::vector<
        std::unique_ptr<Monitor>>
    m_monitors;

};

}
