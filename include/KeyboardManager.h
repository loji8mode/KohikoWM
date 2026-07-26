#pragma once

#include "Command.h"

#include <X11/Xlib.h>

#include <string>
#include <vector>

namespace Kohiko
{

class XConnection;
class WindowManager;
class Config;

// Turns Config's `bind=` entries into grabbed keys, and grabbed
// KeyPress events back into Commands - no other logic lives here, as
// the spec asks for ("ніякої логіки").
class KeyboardManager
{
public:

    KeyboardManager(
        XConnection& connection,
        WindowManager& windowManager
    );

    // Parses every `bind=` entry and grabs each one (with NumLock/
    // CapsLock variants) on the root window. Safe to call again later
    // (e.g. on `reload`) - it ungrabs everything first.
    void Configure(
        const Config& config
    );

    void HandleKeyPress(
        const XKeyEvent& event
    );

private:

    struct Binding
    {
        unsigned int modifiers = 0;
        KeyCode keycode = 0;
        Command command;
    };

    KeySym ResolveKeysym(
        const std::string& token
    ) const;

private:

    XConnection& m_connection;
    WindowManager& m_windowManager;

    std::vector<Binding> m_bindings;

};

}
