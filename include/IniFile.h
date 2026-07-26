#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Kohiko
{

// Minimal reader for the `[Group]\nkey=value` format shared by three
// unrelated freedesktop file types Kohiko now needs to read:
//
//   - .desktop application entries    ([Desktop Entry], [Desktop Action X])
//   - icon theme index.theme files    ([Icon Theme], [$size directory])
//   - GTK's settings.ini              ([Settings])
//
// Deliberately not a general-purpose INI library: no writing, no
// nested includes, no interpolation - just "read groups of key=value
// pairs in file order" - which is all any of the above need. This
// keeps that one bit of parsing logic in one place instead of
// duplicated three times (see DesktopEntry.cpp, IconResolver.cpp).
//
// Localized keys (`Name[uk]=...`) are stored under their full
// bracketed key rather than being special-cased - callers that only
// ever ask for the bare key ("Name") naturally get the C/untranslated
// value, which is all Kohiko's launcher needs.
class IniFile
{
public:

    bool Load(
        const std::filesystem::path& path
    );

    // Empty fallback for a missing group/key.
    std::string Get(
        const std::string& group,
        const std::string& key,
        const std::string& fallback = ""
    ) const;

    bool GetBool(
        const std::string& group,
        const std::string& key,
        bool fallback = false
    ) const;

    // Splits a `Value=a;b;c;` style list on `separator` (default the
    // freedesktop-standard ';'), trimming whitespace and dropping the
    // empty trailing token the standard's mandatory trailing separator
    // would otherwise leave behind.
    std::vector<std::string> GetList(
        const std::string& group,
        const std::string& key,
        char separator = ';'
    ) const;

    bool HasGroup(
        const std::string& group
    ) const;

    // Every group name, in the order it appeared in the file - used
    // to walk all `[36x36/apps]`-style icon-directory sections in an
    // index.theme without knowing their names ahead of time.
    std::vector<std::string> Groups() const;

private:

    struct Entry
    {
        std::string key;
        std::string value;
    };

    struct Group
    {
        std::string name;
        std::vector<Entry> entries;
    };

    std::vector<Group> m_groups;

    const Group* FindGroup(const std::string& name) const;
};

}
