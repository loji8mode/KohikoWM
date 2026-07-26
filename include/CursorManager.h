#pragma once

#include <X11/Xlib.h>

namespace Kohiko
{

class XConnection;

// Sets a normal pointer cursor on the root window at startup, and
// swaps to a resize cursor while a Super+RMB drag is in progress, or a
// move cursor while a Super+LMB swap-drag is in progress - a small
// nicety, not required for correctness.
class CursorManager
{
public:

    explicit CursorManager(
        XConnection& connection
    );

    void Initialize();

    void SetResizing(
        bool resizing
    );

    void SetDragging(
        bool dragging
    );

private:

    XConnection& m_connection;

    Cursor m_normalCursor = 0;
    Cursor m_resizeCursor = 0;
    Cursor m_dragCursor = 0;

};

}
