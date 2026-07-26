// Correctness test for MonitorManager/Monitor/MonitorRule - unlike
// test_bsptree, this one needs a real X11 connection (MonitorManager
// talks to XRandR/the root window directly), so it connects to
// whatever $DISPLAY points at. If none is set (a genuinely headless
// CI box with no Xvfb either), it prints a note and exits 0 rather
// than failing the build - every assertion here is about
// MonitorManager's own reconciliation/query logic, not about there
// being any particular real monitor hardware to find.
//
// Run against a plain Xvfb (a single fallback "monitor") this still
// exercises the exact same Detect()/Neighbor()/ByIndex()/rule-parsing
// code paths a real multi-monitor machine would - just with N=1 - so
// it's a meaningful regression check even without real hardware to
// hotplug.

#include "Config.h"
#include "Monitor.h"
#include "MonitorManager.h"
#include "MonitorRule.h"
#include "WorkspaceManager.h"
#include "XConnection.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace Kohiko;

namespace
{

int g_pass = 0;

void Check(bool condition, const char* what)
{
    if (condition)
    {
        ++g_pass;
        std::printf("  PASS: %s\n", what);
    }
    else
    {
        std::printf("  FAIL: %s\n", what);
        std::exit(1);
    }
}

}

int main()
{
    // --- MonitorRule::Parse()/LoadMonitorRules() - pure parsing, no X needed ---

    std::printf("-- MonitorRule::Parse --\n");
    {
        MonitorRule rule;

        Check(MonitorRule::Parse("HDMI-1,workspace=2", rule), "\"HDMI-1,workspace=2\" parses");
        Check(rule.outputName == "HDMI-1", "output name is HDMI-1");
        Check(rule.workspace == 2, "workspace is 2");

        MonitorRule bare;
        Check(MonitorRule::Parse("DP-1", bare), "a bare name with no workspace= still parses");
        Check(bare.outputName == "DP-1", "output name is DP-1");
        Check(bare.workspace == 0, "workspace stays unset (0) with no workspace= token");

        MonitorRule empty;
        Check(!MonitorRule::Parse("", empty), "an empty rule is rejected");
        Check(!MonitorRule::Parse(",workspace=1", empty), "a rule with no output name is rejected");

        MonitorRule unknownKey;
        Check(MonitorRule::Parse("eDP-1,scale=2,workspace=4", unknownKey),
            "an unrecognised key (scale=) doesn't reject the whole rule");
        Check(unknownKey.workspace == 4, "...and the recognised key (workspace=) still took effect");

        MonitorRule lastWins;
        Check(MonitorRule::Parse("eDP-1,workspace=1,workspace=5", lastWins), "a rule with workspace= repeated parses");
        Check(lastWins.workspace == 5, "...and the last workspace= value wins");
    }

    // Everything below needs a real X connection - MonitorManager
    // talks to XRandR/the root window directly.
    XConnection connection;

    if (!connection.Connect())
    {
        std::printf(
            "\nNo X display available ($DISPLAY unset or unreachable) - "
            "skipping the MonitorManager tests that need a real X11 "
            "connection. %d parsing check(s) passed.\n",
            g_pass);

        return 0;
    }

    // A temp config with a `monitor=` rule for whatever single-monitor
    // fallback name MonitorManager uses when nothing claims to be a
    // real XRandR output ("default" - see MonitorManager::QueryOutputs()) -
    // exercising the exact same rule-matching path a real
    // `monitor=HDMI-1,workspace=N` line would take on real hardware,
    // just against a name guaranteed to exist under Xvfb.
    std::string configPath = "/tmp/kohiko_test_monitor_rules.conf";

    {
        std::ofstream out(configPath);
        out << "monitor=default,workspace=3\n";
    }

    Config config;
    Check(config.Load(configPath), "temp config file loads");

    auto rules = LoadMonitorRules(config);
    Check(rules.size() == 1, "LoadMonitorRules() found exactly the one monitor= line");
    Check(!rules.empty() && rules[0].outputName == "default", "...for output \"default\"");
    Check(!rules.empty() && rules[0].workspace == 3, "...pinning workspace 3");

    WorkspaceManager workspaces(10);
    MonitorManager monitors(connection, workspaces);

    std::printf("\n-- MonitorManager::Initialize (first Detect()) --\n");

    monitors.Initialize(config);

    Check(!monitors.All().empty(), "at least one monitor was detected");
    Check(monitors.Focused() != nullptr, "a monitor is focused after Initialize()");
    Check(&monitors.Primary() == monitors.Focused(), "Focused() starts out as Primary()");

    Monitor* named = monitors.FindByName("default");
    Monitor* ruledMonitor = named ? named : &monitors.Primary();

    // Whichever monitor XRandR/the fallback actually named "default"
    // (or, on a real multi-output XRandR setup where nothing happens
    // to be named exactly that, just the primary one - the rule simply
    // won't have matched anything, which is a legitimate outcome this
    // still shouldn't crash on) should have picked up workspace 3 from
    // the rule.
    if (named)
    {
        Check(
            named->ActiveWorkspace() != nullptr && named->ActiveWorkspace()->Id() == 3,
            "the monitor named \"default\" picked up workspace 3 from the monitor= rule");
    }
    else
    {
        std::printf("  (skip: no monitor named \"default\" was detected - real multi-output XRandR system)\n");
    }

    Check(ruledMonitor->WorkArea().width > 0 && ruledMonitor->WorkArea().height > 0,
        "WorkArea() defaults to something non-empty before RefreshMonitorWorkAreas() has ever run");

    std::printf("\n-- Detect() idempotency (nothing changed between two calls) --\n");

    int firstId = ruledMonitor->Id();
    std::string firstName = ruledMonitor->Name();
    int firstWorkspace = ruledMonitor->ActiveWorkspace() ? ruledMonitor->ActiveWorkspace()->Id() : -1;
    std::size_t countBefore = monitors.All().size();

    bool changed = monitors.Detect();

    Check(!changed, "Detect() reports no topology change when nothing was plugged/unplugged");
    Check(monitors.All().size() == countBefore, "monitor count is unchanged");

    Monitor* again = monitors.FindByName(firstName);
    Check(again != nullptr, "the same-named monitor is still found after re-Detect()");
    Check(again && again->Id() == firstId, "...with the same Id()");
    Check(again && again->ActiveWorkspace() && again->ActiveWorkspace()->Id() == firstWorkspace,
        "...and the same ActiveWorkspace() - a mere re-Detect() must never reassign an untouched monitor's workspace");

    std::printf("\n-- Neighbor()/ByIndex()/Find() --\n");

    Monitor& only = monitors.Primary();

    Check(monitors.ByIndex(1) == &only, "ByIndex(1) is the (leftmost/only) monitor");
    Check(monitors.ByIndex(0) == nullptr, "ByIndex(0) is out of range");
    Check(monitors.ByIndex(static_cast<int>(monitors.All().size()) + 1) == nullptr, "ByIndex() past the end is out of range");

    Check(monitors.Find(only.Id()) == &only, "Find(id) round-trips");
    Check(monitors.Find(-12345) == nullptr, "Find() on a bogus id is null");

    Check(monitors.FindByName(only.Name()) == &only, "FindByName(name) round-trips");
    Check(monitors.FindByName("this-output-does-not-exist") == nullptr, "FindByName() on a bogus name is null");

    if (monitors.All().size() == 1)
    {
        // Single-monitor: Neighbor() has nowhere real to go in any
        // direction, so it should return the monitor itself rather
        // than null - "there's only one, so it's already the nearest
        // one in every direction" is the documented behaviour.
        Check(monitors.Neighbor(only, Direction::Left)  == &only, "Neighbor(Left) on a single monitor returns itself");
        Check(monitors.Neighbor(only, Direction::Right) == &only, "Neighbor(Right) on a single monitor returns itself");
        Check(monitors.Neighbor(only, Direction::Up)    == &only, "Neighbor(Up) on a single monitor returns itself");
        Check(monitors.Neighbor(only, Direction::Down)  == &only, "Neighbor(Down) on a single monitor returns itself");
    }
    else
    {
        std::printf("  (skip: real multi-output system detected (%zu monitors) - "
            "single-monitor Neighbor() self-return case doesn't apply)\n",
            monitors.All().size());
    }

    std::printf("\n-- Containing() --\n");

    const Rect& geo = only.Geometry();
    Point inside(geo.x + geo.width / 2, geo.y + geo.height / 2);
    Point wayOutside(-999999, -999999);

    Check(monitors.Containing(inside) == &only, "Containing() finds the monitor a point sits inside");
    Check(monitors.Containing(wayOutside) == nullptr, "Containing() is null for a point on no connected monitor");

    std::printf("\n-- SetFocused()/Focused() --\n");

    monitors.SetFocused(&only);
    Check(monitors.Focused() == &only, "SetFocused()/Focused() round-trips");

    monitors.SetFocused(nullptr);
    Check(monitors.Focused() == &only, "SetFocused(nullptr) is a no-op, never clears focus to null");

    std::printf("\n-- IsRandrEvent()/HandleXEvent() on a non-RandR event --\n");

    XEvent bogus{};
    bogus.type = ClientMessage; // a real, but definitely-not-RandR, core event type

    Check(!monitors.IsRandrEvent(bogus), "an ordinary ClientMessage is never mistaken for a RandR event");
    Check(!monitors.HandleXEvent(bogus), "HandleXEvent() on a non-RandR event is a safe no-op");

    connection.Disconnect();

    std::printf("\nALL %d CHECKS PASSED.\n", g_pass);
    return 0;
}
