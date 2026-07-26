#pragma once

#include <X11/Xlib.h>

#include <algorithm>
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

    // Clamps this rect so it never extends past `bounds` - the last
    // line of defence for the Geometry Rules (x >= bounds.x, y >=
    // bounds.y, x+width <= bounds.Right(), y+height <= bounds.Bottom())
    // no matter what upstream arithmetic produced, applied right
    // before any Rect is actually handed to X11.
    //
    // `borderWidth` accounts for Kohiko's own border convention (see
    // LayoutEngine/HandleConfigureRequest): x/y are the outer corner
    // of the window *including* its border, and width/height are the
    // content size only, so the true on-screen footprint is
    // width + 2*borderWidth by height + 2*borderWidth. Pass 0 for a
    // rect that's already border-inclusive (e.g. a fullscreen rect).
    //
    // Oversized rects are shrunk (down to a 1px floor) before x/y are
    // ever moved, so a window that's simply too big to fit gets sized
    // down rather than yanked away from wherever it was asked to be.
    Rect ClampedTo(const Rect& bounds, int borderWidth = 0) const
    {
        Rect r = *this;

        int footprint = borderWidth * 2;

        int maxWidth  = std::max(1, bounds.width  - footprint);
        int maxHeight = std::max(1, bounds.height - footprint);

        if (r.width  > maxWidth)  r.width  = maxWidth;
        if (r.height > maxHeight) r.height = maxHeight;
        if (r.width  < 1) r.width  = 1;
        if (r.height < 1) r.height = 1;

        int maxX = bounds.Right()  - r.width  - footprint;
        int maxY = bounds.Bottom() - r.height - footprint;

        if (r.x > maxX) r.x = maxX;
        if (r.y > maxY) r.y = maxY;
        if (r.x < bounds.x) r.x = bounds.x;
        if (r.y < bounds.y) r.y = bounds.y;

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
