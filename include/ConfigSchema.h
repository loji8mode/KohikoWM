#pragma once

#include <string>
#include <vector>

namespace Kohiko
{

// What kind of value a setting expects - purely descriptive. Kohiko
// Settings (see SettingsWindow.h - the "GUI" this schema was
// originally kept ready for) uses this to pick the right editor
// widget for each key (a checkbox for Bool, a color swatch for Color,
// a dropdown for Enum, ...) and to validate input before ever writing
// it back to the config file.
//
// Config itself (see Config.h) doesn't use this at all and stays
// exactly the simple, untyped key=value store it always was - this is
// purely metadata *about* keys, kept in its own file specifically so
// adding it never touched anything about how config actually loads or
// gets read at runtime today.
enum class ConfigValueType
{
    String,
    Int,
    Float,
    Boolean,
    Percent,   // "70%" style, e.g. notepad.width
    Color,     // "0xRRGGBB" style, e.g. general.border_color_active
    Enum,      // one of a fixed set - see ConfigOption::enumValues
};

// One entry a config GUI renders: which category/group it belongs
// under, its key, type, default, a human description doubling as an
// "info icon" popover (what it does, plus any recommendation - the
// default value itself is `defaultValue`, so it doesn't need
// repeating in prose), and, for Enum, the allowed values.
struct ConfigOption
{
    std::string category;      // e.g. "General", "Lock Screen" - which sidebar entry a GUI groups this under
    std::string group;         // e.g. "Session Restore" - a sub-heading within `category`; "" means ungrouped
    std::string key;           // e.g. "lockscreen.after"
    ConfigValueType type = ConfigValueType::String;
    std::string defaultValue;  // as it would literally appear in config/default.conf
    std::string description;  // what it does + any recommendation
    std::vector<std::string> enumValues; // only meaningful when type == Enum
};

// The registry itself - see this header's own comment for what it's
// for. Covers every single-value (non-repeatable) key in
// config/default.conf. Deliberately does NOT cover the repeatable
// directives (`bind=`, `exec.<name>=`, `windowrule=`, `monitor=`) or
// the dynamically-numbered `workspace<N>=` family - those don't fit
// "one key, one typed value" at all (the first four repeat the same
// key arbitrarily many times; the last is a different key per
// workspace, and there can be any number of workspaces). Kohiko
// Settings edits those directly as their own raw config-syntax text
// rather than pretending they're scalar settings - see
// SettingsWindow.cpp's RawBlockPanel/BuildWorkspaceFields for exactly
// how each is handled instead.
class ConfigSchema
{
public:

    static const std::vector<ConfigOption>& All();

    // Every distinct ConfigOption::category, in first-seen (i.e.
    // registration) order - the single source of truth for a GUI's
    // category sidebar, so the category *list* never has to be
    // hand-copied anywhere a category is added to All().
    static std::vector<std::string> Categories();

};

}
