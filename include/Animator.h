#pragma once

#include "Types.h"

#include <chrono>
#include <vector>

namespace Kohiko
{

class XConnection;
class ManagedWindow;

// Kohiko's philosophy rules out a compositor, so "animation" here just
// means: for a short window of time, keep moving/resizing a plain X11
// window a little further towards its destination on every Tick().
// There is nothing else in Kohiko that needs continuous motion, so a
// small special-purpose tweener is enough - no need for a generic
// timeline/easing library.
//
// Used exclusively by the Swap gesture (Super+LMB): when a dragged
// window is dropped, both it and whatever it swapped with should
// "smoothly take a new place" rather than snap there instantly - the
// doc calls this out explicitly as a safety feedback loop, not
// decoration, so it stays within the "Animation Rule": it only ever
// plays for an action the user just took, and only ever moves the
// exact window(s) involved.
class Animator
{
public:

    // Starts (or replaces) the tween for `window`, animating its
    // on-screen rect from `from` to `to` over `durationMs`.
    void Start(
        ManagedWindow* window,
        const Rect& from,
        const Rect& to,
        int durationMs
    );

    // Cancels any in-flight tween for `window` without moving it -
    // used when a window is closed/unmanaged mid-animation.
    void Cancel(
        ManagedWindow* window
    );

    // Advances every in-flight tween by one frame, applying the
    // interpolated rect directly via `connection`. Removes tweens that
    // have completed (snapping them exactly to their target rect first
    // so rounding error never leaves a window a pixel off).
    void Step(
        XConnection& connection
    );

    bool Active() const;

private:

    struct Tween
    {
        ManagedWindow* window = nullptr;
        Rect from;
        Rect to;
        std::chrono::steady_clock::time_point start;
        int durationMs = 0;
    };

    std::vector<Tween> m_tweens;

};

}
