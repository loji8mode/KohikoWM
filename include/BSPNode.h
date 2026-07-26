#pragma once

#include "Types.h"

namespace Kohiko
{

class BSPSplit;

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