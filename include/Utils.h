#pragma once

#include <string>

namespace Kohiko::Utils
{

std::string Trim(
    const std::string& value
);

std::string Lower(
    const std::string& value
);

// Parses a "70%" style string into a 0..1 fraction. Returns `fallback`
// if `value` is empty or doesn't end in '%'.
float ParsePercent(
    const std::string& value,
    float fallback
);

}
