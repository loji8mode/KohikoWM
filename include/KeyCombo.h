#pragma once

#include <string>

namespace Kohiko
{

struct ParsedCombo
{
    unsigned int modifiers = 0;
    std::string token;
    bool valid = false;
};

// Splits combo strings like "SUPER+SHIFT+Q" or "SUPER+BTN1" into a
// modifier mask (SUPER/SHIFT/CTRL|CONTROL/ALT, case-insensitive) plus
// the trailing token ("Q" / "BTN1"). Shared by KeyboardManager (which
// resolves the token to a keysym) and MouseManager (which resolves it
// to a button number).
ParsedCombo ParseCombo(
    const std::string& text
);

}
