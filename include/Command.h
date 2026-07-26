#pragma once

#include "Types.h"

#include <string>

namespace Kohiko
{

enum class CommandType
{
    NoCommand,
    Exec,
    Close,
    ToggleFloating,
    ToggleFullscreen,
    ScratchpadToggle,
    Workspace,
    MoveToWorkspace,
    FocusDirection,
    Rotate,
    Flip,
    Reload,
    Quit,
    LauncherToggle,
    NotepadToggle
};

// A parsed, ready-to-run action.
//
// Config turns raw config/IPC text such as
//
//     bind = SUPER+Q close
//     bind = SUPER+RETURN exec terminal
//     bind = SUPER+SHIFT+1 movetoworkspace 1
//
// into one of these instead of passing strings all the way down to
// the input layer, so nothing downstream of parsing ever has to
// tokenize text again. The same parser backs `kohikoctl dispatch ...`.
struct Command
{
    CommandType type = CommandType::NoCommand;
    std::string stringArg;
    int intArg = 0;
    Direction directionArg = Direction::Left;

    static Command Parse(const std::string& text);
};

}
