#pragma once

#include <X11/Xlib.h>

#include <cstdint>
#include <string>

namespace Kohiko
{

// Raw X11 window handle. Kept as the native Xlib type so most of the
// code base can pass windows around without including Xlib.h itself.
using WindowID = ::Window;

struct Point
{
    int x = 0;
    int y = 0;
};

struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    int Right() const  { return x + width; }
    int Bottom() const { return y + height; }
    int CenterX() const { return x + width / 2; }
    int CenterY() const { return y + height / 2; }

    bool Contains(const Point& p) const
    {
        return p.x >= x && p.x < Right() &&
               p.y >= y && p.y < Bottom();
    }

    Rect Shrunk(int amount) const
    {
        Rect r = *this;

        r.x += amount;
        r.y += amount;
        r.width  -= amount * 2;
        r.height -= amount * 2;

        if (r.width  < 1) r.width  = 1;
        if (r.height < 1) r.height = 1;

        return r;
    }

    bool operator==(const Rect& other) const
    {
        return x == other.x && y == other.y &&
               width == other.width && height == other.height;
    }

    bool operator!=(const Rect& other) const
    {
        return !(*this == other);
    }
};

// Orientation of a BSP split, named after the dividing line between
// the two children - NOT after how the panes are arranged:
//
//   Vertical    -> a *vertical* divider   -> panes side by side (left | right)
//   Horizontal  -> a *horizontal* divider -> panes stacked      (top / bottom)
enum class SplitDirection
{
    Vertical,
    Horizontal
};

// Used for directional focus movement (Super+h/j/k/l).
enum class Direction
{
    Left,
    Right,
    Up,
    Down
};

enum class WindowState
{
    Tiled,
    Floating,
    Fullscreen,
    Scratchpad
};

}
