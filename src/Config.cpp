#include "Config.h"

#include "Utils.h"

#include <fstream>

namespace Kohiko
{

bool Config::Load(
    const std::string& path)
{
    std::ifstream file(path);

    if (!file.is_open())
        return false;

    m_entries.clear();

    std::string line;

    while (std::getline(file, line))
    {
        line = Utils::Trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        auto pos = line.find('=');

        if (pos == std::string::npos)
            continue;

        std::string key   = Utils::Trim(line.substr(0, pos));
        std::string value = Utils::Trim(line.substr(pos + 1));

        if (key.empty())
            continue;

        m_entries.emplace_back(std::move(key), std::move(value));
    }

    return true;
}

std::string Config::GetString(
    const std::string& key,
    const std::string& fallback) const
{
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
    {
        if (it->first == key)
            return it->second;
    }

    return fallback;
}

int Config::GetInt(
    const std::string& key,
    int fallback) const
{
    std::string value = GetString(key);

    if (value.empty())
        return fallback;

    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return fallback;
    }
}

float Config::GetFloat(
    const std::string& key,
    float fallback) const
{
    std::string value = GetString(key);

    if (value.empty())
        return fallback;

    try
    {
        return std::stof(value);
    }
    catch (...)
    {
        return fallback;
    }
}

bool Config::GetBool(
    const std::string& key,
    bool fallback) const
{
    std::string value = Utils::Lower(GetString(key));

    if (value.empty())
        return fallback;

    if (value == "true" || value == "1" || value == "yes" || value == "on")
        return true;

    if (value == "false" || value == "0" || value == "no" || value == "off")
        return false;

    return fallback;
}

float Config::GetPercent(
    const std::string& key,
    float fallback) const
{
    std::string value = GetString(key);
    return Utils::ParsePercent(value, fallback);
}

std::vector<std::string> Config::GetAll(
    const std::string& key) const
{
    std::vector<std::string> result;

    for (const auto& [entryKey, entryValue] : m_entries)
    {
        if (entryKey == key)
            result.push_back(entryValue);
    }

    return result;
}

bool Config::Contains(
    const std::string& key) const
{
    for (const auto& [entryKey, entryValue] : m_entries)
    {
        if (entryKey == key)
            return true;
    }

    return false;
}

}
