#include "IniFile.h"

#include "Utils.h"

#include <fstream>

namespace Kohiko
{

bool IniFile::Load(
    const std::filesystem::path& path)
{
    std::ifstream file(path);

    if (!file.is_open())
        return false;

    m_groups.clear();

    Group* current = nullptr;
    std::string line;

    while (std::getline(file, line))
    {
        // .desktop files (unlike most INI dialects) allow trailing
        // '\r' when they've been copied from a DOS-authored archive,
        // and Trim() already strips ordinary surrounding whitespace
        // for everything else read here.
        line = Utils::Trim(line);

        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            m_groups.push_back(Group{line.substr(1, line.size() - 2), {}});
            current = &m_groups.back();
            continue;
        }

        if (!current)
            continue; // stray key=value before any [Group] header - ignore

        std::size_t pos = line.find('=');

        if (pos == std::string::npos)
            continue;

        std::string key = Utils::Trim(line.substr(0, pos));
        std::string value = Utils::Trim(line.substr(pos + 1));

        if (key.empty())
            continue;

        current->entries.push_back(Entry{std::move(key), std::move(value)});
    }

    return true;
}

const IniFile::Group* IniFile::FindGroup(
    const std::string& name) const
{
    for (const auto& group : m_groups)
    {
        if (group.name == name)
            return &group;
    }

    return nullptr;
}

std::string IniFile::Get(
    const std::string& group,
    const std::string& key,
    const std::string& fallback) const
{
    const Group* g = FindGroup(group);

    if (!g)
        return fallback;

    for (const auto& entry : g->entries)
    {
        if (entry.key == key)
            return entry.value;
    }

    return fallback;
}

bool IniFile::GetBool(
    const std::string& group,
    const std::string& key,
    bool fallback) const
{
    std::string value = Utils::Lower(Get(group, key, ""));

    if (value.empty())
        return fallback;

    return value == "true" || value == "1";
}

std::vector<std::string> IniFile::GetList(
    const std::string& group,
    const std::string& key,
    char separator) const
{
    return Utils::Split(Get(group, key, ""), separator);
}

bool IniFile::HasGroup(
    const std::string& group) const
{
    return FindGroup(group) != nullptr;
}

std::vector<std::string> IniFile::Groups() const
{
    std::vector<std::string> names;
    names.reserve(m_groups.size());

    for (const auto& group : m_groups)
        names.push_back(group.name);

    return names;
}

}
