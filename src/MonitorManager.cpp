#include "MonitorManager.h"

#include "WorkspaceManager.h"
#include "Workspace.h"
#include "XConnection.h"

#include <X11/Xlib.h>

#ifdef KOHIKO_HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

namespace Kohiko
{

MonitorManager::MonitorManager(
    XConnection& connection,
    WorkspaceManager& workspaces)
    :
    m_connection(connection),
    m_workspaces(workspaces)
{
}

void MonitorManager::Detect()
{
    m_monitors.clear();

#ifdef KOHIKO_HAVE_XRANDR

    int eventBase = 0;
    int errorBase = 0;

    if (XRRQueryExtension(m_connection.GetDisplay(), &eventBase, &errorBase))
    {
        XRRScreenResources* resources =
            XRRGetScreenResourcesCurrent(
                m_connection.GetDisplay(),
                m_connection.Root());

        if (resources)
        {
            for (int i = 0; i < resources->noutput; ++i)
            {
                XRROutputInfo* output =
                    XRRGetOutputInfo(m_connection.GetDisplay(), resources, resources->outputs[i]);

                if (!output)
                    continue;

                if (output->connection == RR_Connected && output->crtc)
                {
                    XRRCrtcInfo* crtc =
                        XRRGetCrtcInfo(m_connection.GetDisplay(), resources, output->crtc);

                    if (crtc)
                    {
                        auto monitor =
                            std::make_unique<Monitor>(static_cast<int>(m_monitors.size()));

                        Rect geometry;
                        geometry.x = crtc->x;
                        geometry.y = crtc->y;
                        geometry.width  = static_cast<int>(crtc->width);
                        geometry.height = static_cast<int>(crtc->height);

                        monitor->SetGeometry(geometry);
                        monitor->SetWorkspace(&m_workspaces.Current());

                        m_monitors.push_back(std::move(monitor));

                        XRRFreeCrtcInfo(crtc);
                    }
                }

                XRRFreeOutputInfo(output);
            }

            XRRFreeScreenResources(resources);
        }
    }

#endif

    if (m_monitors.empty())
    {
        // No XRandR at build time, or it reported nothing usable:
        // fall back to a single monitor spanning the whole display.
        auto monitor = std::make_unique<Monitor>(0);

        Rect geometry;
        geometry.x = 0;
        geometry.y = 0;
        geometry.width  = DisplayWidth(m_connection.GetDisplay(), m_connection.Screen());
        geometry.height = DisplayHeight(m_connection.GetDisplay(), m_connection.Screen());

        monitor->SetGeometry(geometry);
        monitor->SetWorkspace(&m_workspaces.Current());

        m_monitors.push_back(std::move(monitor));
    }
}

Monitor& MonitorManager::Primary()
{
    return *m_monitors.front();
}

const std::vector<std::unique_ptr<Monitor>>&
MonitorManager::All() const
{
    return m_monitors;
}

}
