#include "Config.h"

#include <fstream>
#include <sstream>

namespace Kohiko
{

bool Config::Load(
    const std::string& path)
{
    std::ifstream file(path);

    if (!file.is_open())
        return false;

    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        if (line[0] == '#')
            continue;

        auto pos =
            line.find('=');

        if (pos == std::string::npos)
            continue;

        auto key =
            line.substr(0, pos);

        auto value =
            line.substr(pos + 1);

        m_values[key] = value;
    }

    return true;
}

std::string Config::GetString(
    const std::string& key) const
{
    auto it =
        m_values.find(key);

    if (it == m_values.end())
        return {};

    return it->second;
}

int Config::GetInt(
    const std::string& key) const
{
    auto value =
        GetString(key);

    if (value.empty())
        return 0;

    return std::stoi(value);
}

bool Config::Contains(
    const std::string& key) const
{
    return
        m_values.find(key)
        !=
        m_values.end();
}

}