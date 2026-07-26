#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Kohiko
{

// Holds every `key=value` line from the config file, in order, with
// duplicates preserved - unlike a plain map, this can represent
// repeated directives such as
//
//     bind=SUPER+Q close
//     bind=SUPER+RETURN exec terminal
//     bind=SUPER+D exec launcher
//
// GetString()/GetInt()/... answer "what's the (last) value for this
// unique setting"; GetAll() answers "every value given for this key",
// which is what repeatable directives like `bind=` need.
class Config
{
public:

    bool Load(
        const std::string& path
    );

    std::string GetString(
        const std::string& key,
        const std::string& fallback = ""
    ) const;

    int GetInt(
        const std::string& key,
        int fallback = 0
    ) const;

    float GetFloat(
        const std::string& key,
        float fallback = 0.0f
    ) const;

    bool GetBool(
        const std::string& key,
        bool fallback = false
    ) const;

    // Fraction parsed from a "70%" style value (0.70f). Falls back if
    // the key is missing or isn't a percentage.
    float GetPercent(
        const std::string& key,
        float fallback
    ) const;

    std::vector<std::string> GetAll(
        const std::string& key
    ) const;

    bool Contains(
        const std::string& key
    ) const;

private:

    std::vector<
        std::pair<std::string, std::string>
    > m_entries;

};

}
