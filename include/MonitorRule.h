#pragma once

#include <string>
#include <vector>

namespace Kohiko
{

class Config;

// One `monitor=` directive from the config file - Kohiko's
// future-proof hook for monitor-specific rules (the spec's example:
// pinning which workspace an output starts on), modelled on the same
// `key=value,key=value,...` shape WindowRule's `windowrule=` uses so
// more per-monitor settings (position, scale, a preferred layout, ...)
// can be added here later without a new syntax:
//
//   monitor=HDMI-1,workspace=1
//   monitor=DP-1,workspace=2
//
// The first, bare token is always the XRandR output name
// (`kohikoctl monitors`, or plain `xrandr`, shows you the exact
// string to use); every token after the first comma is `key=value`.
// Only `workspace=` is recognised today - an unknown key is silently
// ignored rather than rejecting the whole rule, so a future Kohiko
// version can start understanding more of them without breaking a
// config file already using this line for the parts it knows about.
struct MonitorRule
{
    std::string outputName;

    // 0 = not set by this rule.
    int workspace = 0;

    static bool Parse(
        const std::string& text,
        MonitorRule& out
    );
};

// Parses every `monitor=` line out of `config`, in file order. A
// later line for the same output name overrides an earlier one for
// whichever fields it sets - see MonitorManager::WorkspaceForRule().
std::vector<MonitorRule> LoadMonitorRules(
    const Config& config
);

}
