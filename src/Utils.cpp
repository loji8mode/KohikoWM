#include "Utils.h"

#include <algorithm>
#include <cctype>

namespace Kohiko::Utils
{

std::string Trim(
    const std::string& value)
{
    std::size_t first =
        value.find_first_not_of(" \t");

    if(first == std::string::npos)
        return "";

    std::size_t last =
        value.find_last_not_of(" \t");

    return value.substr(
        first,
        last - first + 1);
}

std::string Lower(
    const std::string& value)
{
    std::string result =
        value;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(
                std::tolower(c));
        });

    return result;
}

}