#pragma once

#include "Types.h"

namespace Kohiko
{

// Tracks which single window (if any) currently occupies the
// scratchpad slot and whether it is shown. All the actual X11 work
// (unmap/center/map/raise/focus) is WindowManager's job - this class
// is deliberately just the state machine behind Super+F1:
//
//   empty slot + toggle    -> banish the focused window here (hidden)
//   occupied + toggle      -> show it centered / hide it again
class Scratchpad
{
public:

    bool HasWindow() const;

    WindowID Window() const;

    bool IsVisible() const;

    // Claims the (empty) slot for `window`, starting hidden.
    void Assign(
        WindowID window
    );

    // Frees the slot if `window` was the occupant (e.g. it closed).
    void Forget(
        WindowID window
    );

    // Flips visibility and returns the new value. No-op (returns
    // false) if the slot is empty.
    bool Toggle();

private:

    WindowID m_window = 0;

    bool m_visible = false;

};

}
