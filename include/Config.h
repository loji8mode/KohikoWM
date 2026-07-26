#pragma once

#include <string>
#include <unordered_map>

namespace Kohiko
{

class Config
{
public:

    bool Load(
        const std::string& path
    );

    std::string GetString(
        const std::string& key
    ) const;

    int GetInt(
        const std::string& key
    ) const;

    bool Contains(
        const std::string& key
    ) const;

private:

    std::unordered_map<
        std::string,
        std::string
    > m_values;

};

}