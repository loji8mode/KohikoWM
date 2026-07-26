#include "Animator.h"

#include "ManagedWindow.h"
#include "XConnection.h"

#include <algorithm>
#include <cmath>

namespace Kohiko
{

namespace
{

// Ease-out cubic: starts fast, settles gently into the target rect
// instead of arriving at a hard, linear stop. Cheap, and exactly the
// kind of restrained, purposeful motion the project's "Animation
// Rule" asks for.
float EaseOutCubic(float t)
{
    float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}

int Lerp(int from, int to, float t)
{
    return from + static_cast<int>(std::lround((to - from) * t));
}

}

void Animator::Start(
    ManagedWindow* window,
    const Rect& from,
    const Rect& to,
    int durationMs)
{
    if (!window)
        return;

    Cancel(window);

    Tween tween;
    tween.window = window;
    tween.from = from;
    tween.to = to;
    tween.start = std::chrono::steady_clock::now();
    tween.durationMs = durationMs > 0 ? durationMs : 1;

    m_tweens.push_back(tween);
}

void Animator::Cancel(
    ManagedWindow* window)
{
    m_tweens.erase(
        std::remove_if(
            m_tweens.begin(),
            m_tweens.end(),
            [window](const Tween& tween)
            {
                return tween.window == window;
            }),
        m_tweens.end());
}

void Animator::Step(
    XConnection& connection)
{
    if (m_tweens.empty())
        return;

    auto now = std::chrono::steady_clock::now();

    for (auto it = m_tweens.begin(); it != m_tweens.end(); )
    {
        auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - it->start).count();

        float t = static_cast<float>(elapsedMs) / static_cast<float>(it->durationMs);
        bool finished = t >= 1.0f;

        if (finished)
            t = 1.0f;

        float eased = EaseOutCubic(t);

        Rect current;
        current.x      = Lerp(it->from.x,      it->to.x,      eased);
        current.y      = Lerp(it->from.y,      it->to.y,      eased);
        current.width  = Lerp(it->from.width,  it->to.width,  eased);
        current.height = Lerp(it->from.height, it->to.height, eased);

        // Every tween here targets a window's *own* already-committed
        // Geometry() (see WindowManager::EndSwapDrag) - this only ever
        // walks the real, on-screen pixels towards it, it never writes
        // back to ManagedWindow's bookkeeping.
        connection.MoveResizeWindow(it->window->Id(), current);

        if (finished)
            it = m_tweens.erase(it);
        else
            ++it;
    }
}

bool Animator::Active() const
{
    return !m_tweens.empty();
}

}
