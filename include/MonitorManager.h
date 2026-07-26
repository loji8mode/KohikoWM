#pragma once

#include "Monitor.h"

#include <memory>
#include <vector>

namespace Kohiko
{

class XConnection;
class WorkspaceManager;

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