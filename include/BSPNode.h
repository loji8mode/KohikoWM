#pragma once

#include "Types.h"

namespace Kohiko
{

class BSPSplit;

// Base of every node in a BSPTree.
//
// Geometry() is the node's *slot* rect: the raw rectangle the BSP
// partition assigned it, with no gaps or borders subtracted. It is
// recomputed by LayoutEngine on every Arrange() and is what mouse
// hit-testing and Super+h/j/k/l neighbor-finding use, since slots
// tile the workspace with no gaps between them. The final on-screen
// window rect (slot shrunk for gaps/border) lives on ManagedWindow
// instead.
class BSPNode
{
public:

    virtual ~BSPNode() = default;

    virtual bool IsLeaf() const = 0;

    const Rect& Geometry() const
    {
        return m_geometry;
    }

    void SetGeometry(
        const Rect& rect)
    {
        m_geometry = rect;
    }

    BSPSplit* Parent() const
    {
        return m_parent;
    }

    void SetParent(
        BSPSplit* parent)
    {
        m_parent = parent;
    }

private:

    Rect m_geometry;

    BSPSplit* m_parent = nullptr;

};

}
