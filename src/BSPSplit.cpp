#include "BSPSplit.h"

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
    if (ratio < 0.10f)
        ratio = 0.10f;

    if (ratio > 0.90f)
        ratio = 0.90f;

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

}