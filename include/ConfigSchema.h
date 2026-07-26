#pragma once

#include <string>
#include <vector>

namespace Kohiko
{

// What kind of value a setting expects - purely descriptive. A future
// config GUI (see the spec this is preparing for: categories, grouped
// settings, search, and an inline info icon per setting - not being
// built yet, just kept possible) would use this to pick the right
// editor widget (a checkbox for Bool, a color swatch for Color, a
// dropdown for Enum, ...) and to validate input before ever writing it
// back to the config file.
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
    Bool,
    Percent,   // "70%" style, e.g. notepad.width
    Color,     // "0xRRGGBB" style, e.g. general.border_color_active
    Enum,      // one of a fixed set - see ConfigOption::enumValues
};

// One entry a future config GUI would render: which category/group it
// belongs under, its key, type, default, a human description doubling
// as that GUI's "info icon" text (what it does, plus any
// recommendation - the default value itself is `defaultValue`, so it
// doesn't need repeating in prose), and, for Enum, the allowed values.
struct ConfigOption
{
    std::string category;      // e.g. "General", "Lock Screen" - what a GUI would group/tab by
    std::string key;           // e.g. "lockscreen.lock_on_suspend"
    ConfigValueType type = ConfigValueType::String;
    std::string defaultValue;  // as it would literally appear in config/default.conf
    std::string description;  // what it does + any recommendation
    std::vector<std::string> enumValues; // only meaningful when type == Enum
};

// The registry itself - see this header's own comment for what it's
// for. Deliberately NOT exhaustive over every key in
// config/default.conf (see the README's Configuration section for the
// convention this is meant to start): populated here for every
// setting this project's most recent round of work added (lock
// screen, power menu, session restore) as a live, concrete example of
// the pattern, plus a couple of the most commonly-tuned pre-existing
// ones. Extending this alongside any *future* config addition -
// rather than only writing a comment in config/default.conf - is what
// actually keeps a future GUI buildable without a separate audit pass
// over the whole config surface first; that's the entire point of
// this class existing before there's anything to consume it yet.
class ConfigSchema
{
public:

    static const std::vector<ConfigOption>& All();

};

}
