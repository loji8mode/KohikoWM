#pragma once

#include "BSPNode.h"

namespace Kohiko
{

class ManagedWindow;

// A leaf holds exactly one tiled window. It does not own the window
// (WindowRepository does); it just points at it, so Swap() can
// re-point two leaves at each other's windows without touching the
// tree shape or recomputing any geometry.
class BSPLeaf
    :
    public BSPNode
{
public:

    explicit BSPLeaf(
        ManagedWindow* window);

    bool IsLeaf() const override;

    ManagedWindow* Window() const;

    void SetWindow(
        ManagedWindow* window);

private:

    ManagedWindow* m_window;

};

}
