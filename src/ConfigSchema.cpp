#include "ConfigSchema.h"

namespace Kohiko
{

const std::vector<ConfigOption>& ConfigSchema::All()
{
    static const std::vector<ConfigOption> options =
    {
        // --- General (a couple of the most commonly-tuned keys, as an
        // example of documenting a pre-existing setting alongside new
        // ones - not an audit of every general.* key) ---
        {
            "General", "general.focus_follows_mouse", ConfigValueType::Bool, "true",
            "Moving the mouse onto a window focuses it automatically, without "
            "needing to click. Recommended on for a tiling-style workflow; "
            "turn off if you prefer click-to-focus.",
            {}
        },
        {
            "General", "general.inner_gap", ConfigValueType::Int, "6",
            "Pixel gap between tiled windows. 0 disables gaps entirely.",
            {}
        },
        {
            "General", "general.tiling_misbehavior_fallback", ConfigValueType::Enum, "floating",
            "Where a window that keeps fighting its assigned tile geometry "
            "(see general.tiling_misbehavior_threshold) gets moved to instead. "
            "'floating' keeps it visible alongside everything else; "
            "'new_workspace' moves it out of the way entirely. Recommended: "
            "floating, unless the misbehaving app is something you'd rather "
            "not see at all until you switch to it.",
            {"floating", "new_workspace"}
        },

        // --- Session Restore ---
        {
            "Session Restore", "session.restore_priority", ConfigValueType::Enum, "config",
            "When a window's saved session state and a matching windowrule= "
            "disagree (e.g. it was on workspace 1 last time Kohiko exited, "
            "but a windowrule now pins it to workspace 3), which one wins. "
            "'config' keeps your windowrule=s authoritative; 'session' prefers "
            "whatever was actually true last time you used it. Recommended: "
            "config, unless you're actively relying on session restore to "
            "remember placements you're not ready to write permanent rules "
            "for yet.",
            {"config", "session"}
        },

        // --- Power Menu ---
        {
            "Power Menu", "power.shutdown_command", ConfigValueType::String, "systemctl poweroff",
            "Shell command run for the power menu's Shutdown row. Swap in "
            "whatever your init system/session actually uses if it's not "
            "systemd (e.g. loginctl poweroff).",
            {}
        },
        {
            "Power Menu", "power.restart_command", ConfigValueType::String, "systemctl reboot",
            "Shell command run for the power menu's Restart row.",
            {}
        },
        {
            "Power Menu", "power.suspend_command", ConfigValueType::String, "systemctl suspend",
            "Shell command run for the power menu's Suspend row. Kohiko locks "
            "the screen right before running this if lockscreen.lock_on_suspend "
            "is enabled.",
            {}
        },

        // --- Lock Screen ---
        {
            "Lock Screen", "lockscreen.background_color", ConfigValueType::Color, "0x1e1e2e",
            "Solid background color, used wherever lockscreen.background_image "
            "isn't set (or fails to load).",
            {}
        },
        {
            "Lock Screen", "lockscreen.foreground", ConfigValueType::Color, "0xcdd6f4",
            "Text and password-dot color.",
            {}
        },
        {
            "Lock Screen", "lockscreen.field_color", ConfigValueType::Color, "0x313244",
            "The password field's own background color.",
            {}
        },
        {
            "Lock Screen", "lockscreen.error_color", ConfigValueType::Color, "0xf38ba8",
            "Color for the brief \"Wrong password\" message and field border "
            "shown after a failed attempt.",
            {}
        },
        {
            "Lock Screen", "lockscreen.background_image", ConfigValueType::String, "",
            "Optional path to an image, stretched to fill each monitor. Leave "
            "unset to just use lockscreen.background_color.",
            {}
        },
        {
            "Lock Screen", "lockscreen.logo", ConfigValueType::String, "",
            "Optional path to a logo image, drawn at a fixed size centered "
            "above the password field on every monitor. Leave unset for no "
            "logo.",
            {}
        },
        {
            "Lock Screen", "lockscreen.lock_on_suspend", ConfigValueType::Bool, "true",
            "Whether Suspend (from the power menu, or power.suspend_command "
            "run any other way through Kohiko) locks the screen first. "
            "Recommended on, unless another locker is already handling this.",
            {}
        },
    };

    return options;
}

}
