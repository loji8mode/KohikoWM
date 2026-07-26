#pragma once

#include <string>
#include <vector>

namespace Kohiko
{

class Config;

// One `windowrule=` directive from the config file.
//
// No amount of automatic transient/dialog-type detection can cover
// how every application in the world actually chooses to behave -
// some (Tlauncher) insist on floating at their own fixed size when
// what you actually want is for them to tile like everything else;
// others (flameshot's screenshot overlay) need to be genuinely
// fullscreen and nothing less; others still (a chat client's media
// viewer) might need any one of "let it go fullscreen", "never let it
// go fullscreen", or "banish it to its own workspace" depending on
// taste. `windowrule=` is Kohiko's escape hatch for exactly that,
// modelled on the same idea as i3's `for_window` / Hyprland's
// `windowrule`: match a window by class/instance/title, then pin down
// exactly how it's allowed to behave, overriding whatever the
// application itself asks for.
//
//   windowrule=tile class:tlauncher          # never let it float at its own size
//   windowrule=fullscreen class:flameshot    # always open already fullscreen
//   windowrule=nofullscreen class:mpv        # never honour its own fullscreen requests
//   windowrule=float class:pavucontrol
//   windowrule=workspace:8 class:telegram title:photo
//
// Every rule whose selector matches applies (a window can be both
// `tile` and `workspace:8` from two separate lines, for instance) -
// see WindowManager::ResolveWindowRules().
struct WindowRule
{
    enum class Action
    {
        Float,
        Tile,
        Fullscreen,
        NoFullscreen,
        Workspace
    };

    Action action = Action::Float;

    // Empty means "don't check this field" - every non-empty one
    // present must match (an implicit AND) for the rule to apply.
    // Compared case-insensitively as a substring, the same convention
    // dwm/i3-style matching typically uses, and forgiving enough that
    // "class:telegram" still matches an exact class of
    // "TelegramDesktop" without the user having to know the precise
    // casing/spelling a given toolkit happens to report.
    std::string classPattern;
    std::string instancePattern;
    std::string titlePattern;

    // Only meaningful when action == Workspace.
    int workspace = 0;

    // Whether this rule applies to a window with this class/instance/
    // title (already Kohiko's own values from ManagedWindow - not
    // re-read from X here).
    bool Matches(
        const std::string& className,
        const std::string& instanceName,
        const std::string& title) const;

    // Parses one `windowrule=` value (the part after the `=`, e.g.
    // "tile class:tlauncher"). Returns false (and leaves `out`
    // untouched) if the line doesn't parse - an unrecognised action
    // name, or one with no selector at all (a bare `windowrule=tile`
    // would otherwise silently apply to every window in existence,
    // which is never what that line meant).
    static bool Parse(
        const std::string& text,
        WindowRule& out);
};

// The combined effect of every rule that matches one window - what
// WindowManager::Manage() actually acts on, rather than walking
// m_windowRules itself at every call site.
struct WindowRuleEffect
{
    bool forceFloat = false;
    bool forceTile = false;
    bool forceFullscreen = false;
    bool denyFullscreen = false;

    // 0 = no override; otherwise a 1-based workspace id.
    int forceWorkspace = 0;
};

// Parses every `windowrule=` line out of `config`, in file order.
std::vector<WindowRule> LoadWindowRules(
    const Config& config);

}
