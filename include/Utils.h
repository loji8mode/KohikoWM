#pragma once

#include <string>
#include <vector>

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

// Splits `value` on runs of whitespace, dropping empty tokens - used
// for space-separated config lists such as `auto_start_programs=`
// (e.g. "telegram-desktop discord zen-browser" -> 3 tokens).
std::vector<std::string> SplitWhitespace(
    const std::string& value
);

// Byte offset of the start of the UTF-8 codepoint immediately before
// `pos` (clamped to 0 if `pos` is already at/before the start). Lets
// cursor movement, backspace, and delete act on whole characters
// instead of individual UTF-8 continuation bytes.
std::size_t Utf8PrevBoundary(
    const std::string& value,
    std::size_t pos
);

// Byte offset of the start of the UTF-8 codepoint immediately after
// `pos` (clamped to value.size()).
std::size_t Utf8NextBoundary(
    const std::string& value,
    std::size_t pos
);

// Clamps `pos` to value.size() and then, if that lands inside a
// multi-byte UTF-8 character (e.g. a column carried over from a
// different line of different content), backs up to that character's
// start. Used wherever a byte offset from one string is reused
// against another string that may be encoded differently at that
// offset.
std::size_t Utf8ClampToBoundary(
    const std::string& value,
    std::size_t pos
);

}
