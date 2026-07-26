#pragma once

#include "Types.h"

#include <string>
#include <vector>

namespace Kohiko
{

class Workspace;

// One physical display, as reported by XRandR (or the single
// whole-display fallback when XRandR isn't available - see
// MonitorManager). Kept deliberately dumb: everything here is either
// plain geometry or a pointer to whichever Workspace this monitor is
// currently showing - MonitorManager owns the actual detection/
// reconciliation logic, WindowManager owns everything about *how*
// windows get placed onto/moved between monitors.
class Monitor
{
public:

    explicit Monitor(
        int id
    );

    // Stable for as long as this Monitor object exists, but NOT
    // guaranteed stable across a hotplug event - MonitorManager
    // renumbers every connected monitor by on-screen left-to-right,
    // top-to-bottom position every time the monitor set changes, so
    // "monitor 1" always means "the leftmost one" the way the spec's
    // examples (and `movetomonitor <N>`) expect, rather than an
    // opaque id that would otherwise happily skip around/collide
    // after a monitor is unplugged and replugged. Anything that needs
    // to survive a hotplug (which workspace belongs where, which
    // monitor a window is on) is keyed off Name() instead - see
    // MonitorManager::Detect().
    int Id() const;

    void SetId(
        int id
    );

    // The XRandR output name ("HDMI-1", "eDP-1", "DP-2", ...) - empty
    // for the single-monitor fallback (see MonitorManager::Detect()).
    // This - not Id() - is what MonitorManager matches monitors
    // against across a hotplug, and what `monitor=<name>,workspace=N`
    // rules (see MonitorRule.h) key off.
    const std::string& Name() const;

    void SetName(
        const std::string& name
    );

    // Whether the underlying output is currently connected/active.
    // MonitorManager never keeps a disconnected Monitor object around
    // (see Detect()) - this exists mainly so a Monitor* handed out
    // mid-reconciliation (the "about to be removed" callback) can
    // still answer the question honestly for whatever's watching.
    bool Connected() const;

    void SetConnected(
        bool connected
    );

    // XRandR's notion of the primary output, if any was set
    // (`xrandr --output X --primary`). MonitorManager::Primary()
    // falls back to the leftmost connected monitor when nothing
    // claims this.
    bool IsPrimary() const;

    void SetPrimary(
        bool primary
    );

    void SetGeometry(
        const Rect& rect
    );

    // Absolute (x, y, width, height) in the root window's coordinate
    // space - the full physical output, exactly as XRandR reports it.
    const Rect& Geometry() const;

    // The area actually available for tiling/floating placement on
    // this monitor: Geometry() minus whatever screen-edge furniture
    // WindowManager has reserved here (currently just its own bar,
    // when it's the monitor hosting it) - see
    // WindowManager::RefreshMonitorWorkAreas(). Defaults to Geometry()
    // until that's run at least once.
    void SetWorkArea(
        const Rect& rect
    );

    const Rect& WorkArea() const;

    // The workspace this monitor is currently displaying - never null
    // once MonitorManager has assigned one (every monitor owns
    // exactly one active workspace, and no two monitors ever share
    // one - see MonitorManager::Detect()/WindowManager::
    // SwitchWorkspaceOnMonitor()).
    void SetWorkspace(
        Workspace* workspace
    );

    Workspace* ActiveWorkspace() const;

private:

    int m_id;

    std::string m_name;

    bool m_connected = true;
    bool m_primary = false;

    Rect m_geometry;
    Rect m_workArea;

    Workspace* m_workspace = nullptr;

};

}
