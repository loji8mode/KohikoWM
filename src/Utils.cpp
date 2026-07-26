#include "Utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace Kohiko::Utils
{

std::string Trim(
    const std::string& value)
{
    std::size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
        return "";

    std::size_t last = value.find_last_not_of(" \t\r\n");

    return value.substr(first, last - first + 1);
}

std::string Lower(
    const std::string& value)
{
    std::string result = value;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    return result;
}

float ParsePercent(
    const std::string& value,
    float fallback)
{
    if (value.empty() || value.back() != '%')
        return fallback;

    try
    {
        float percent = std::stof(value.substr(0, value.size() - 1));
        return percent / 100.0f;
    }
    catch (...)
    {
        return fallback;
    }
}

namespace
{

// UTF-8 continuation bytes all match binary 10xxxxxx - everything
// else (ASCII bytes and multi-byte lead bytes) starts a new codepoint.
bool IsUtf8Continuation(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

}

std::size_t Utf8PrevBoundary(
    const std::string& value,
    std::size_t pos)
{
    if (pos == 0)
        return 0;

    --pos;

    while (pos > 0 && IsUtf8Continuation(static_cast<unsigned char>(value[pos])))
        --pos;

    return pos;
}

std::size_t Utf8NextBoundary(
    const std::string& value,
    std::size_t pos)
{
    if (pos >= value.size())
        return value.size();

    ++pos;

    while (pos < value.size() && IsUtf8Continuation(static_cast<unsigned char>(value[pos])))
        ++pos;

    return pos;
}

std::size_t Utf8ClampToBoundary(
    const std::string& value,
    std::size_t pos)
{
    pos = std::min(pos, value.size());

    while (pos > 0 && IsUtf8Continuation(static_cast<unsigned char>(value[pos])))
        --pos;

    return pos;
}

}
