#include "BSPSplit.h"

#include <algorithm>

namespace Kohiko
{

BSPSplit::BSPSplit(
    SplitDirection direction)
    :
    m_direction(direction)
{
}

bool BSPSplit::IsLeaf() const
{
    return false;
}

SplitDirection BSPSplit::Direction() const
{
    return m_direction;
}

void BSPSplit::SetDirection(
    SplitDirection direction)
{
    m_direction = direction;
}

float BSPSplit::Ratio() const
{
    return m_ratio;
}

void BSPSplit::SetRatio(
    float ratio)
{
    if (ratio < 0.05f)
        ratio = 0.05f;

    if (ratio > 0.95f)
        ratio = 0.95f;

    m_ratio = ratio;
}

BSPNode* BSPSplit::Left() const
{
    return m_left.get();
}

BSPNode* BSPSplit::Right() const
{
    return m_right.get();
}

void BSPSplit::SetLeft(
    std::unique_ptr<BSPNode> node)
{
    if (node)
        node->SetParent(this);

    m_left = std::move(node);
}

void BSPSplit::SetRight(
    std::unique_ptr<BSPNode> node)
{
    if (node)
        node->SetParent(this);

    m_right = std::move(node);
}

std::unique_ptr<BSPNode> BSPSplit::TakeLeft()
{
    return std::move(m_left);
}

std::unique_ptr<BSPNode> BSPSplit::TakeRight()
{
    return std::move(m_right);
}

void BSPSplit::SwapChildren()
{
    std::swap(m_left, m_right);
}

void BSPSplit::Subdivide(
    const Rect& area,
    int innerGap,
    Rect& first,
    Rect& second) const
{
    first  = area;
    second = area;

    if (m_direction == SplitDirection::Vertical)
    {
        int usable      = std::max(0, area.width - innerGap);
        int firstWidth  = static_cast<int>(static_cast<float>(usable) * m_ratio);
        int secondWidth = usable - firstWidth;

        first.width  = firstWidth;
        second.x     = area.x + firstWidth + innerGap;
        second.width = secondWidth;
    }
    else
    {
        int usable       = std::max(0, area.height - innerGap);
        int firstHeight  = static_cast<int>(static_cast<float>(usable) * m_ratio);
        int secondHeight = usable - firstHeight;

        first.height  = firstHeight;
        second.y      = area.y + firstHeight + innerGap;
        second.height = secondHeight;
    }
}

}
