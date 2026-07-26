#pragma once

#include <X11/Xlib.h>

#include <cstdint>
#include <string>

namespace Kohiko
{

using WindowID = ::Window;

struct Point
{
    int x = 0;
    int y = 0;
};

struct Size
{
    int width = 0;
    int height = 0;
};

struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

enum class Layout
{
    BSP
};

enum class SplitDirection
{
    Vertical,
    Horizontal
};

enum class WindowState
{
    Tiled,
    Floating,
    Fullscreen,
    Scratchpad
};

}