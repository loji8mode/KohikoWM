#include "KeyCombo.h"

#include "Utils.h"

#include <X11/Xlib.h>

#include <vector>

namespace Kohiko
{

ParsedCombo ParseCombo(
    const std::string& text)
{
    ParsedCombo result;

    std::vector<std::string> tokens;
    std::string current;

    for (char c : text)
    {
        if (c == '+')
        {
            tokens.push_back(current);
            current.clear();
        }
        else
        {
            current += c;
        }
    }

    tokens.push_back(current);

    if (tokens.empty())
        return result;

    result.token = Utils::Trim(tokens.back());
    tokens.pop_back();

    for (const auto& raw : tokens)
    {
        std::string mod = Utils::Lower(Utils::Trim(raw));

        if (mod == "super")
            result.modifiers |= Mod4Mask;
        else if (mod == "shift")
            result.modifiers |= ShiftMask;
        else if (mod == "ctrl" || mod == "control")
            result.modifiers |= ControlMask;
        else if (mod == "alt")
            result.modifiers |= Mod1Mask;
    }

    result.valid = !result.token.empty();

    return result;
}

}
