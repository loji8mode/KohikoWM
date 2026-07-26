#pragma once

#include "BSPNode.h"

#include <memory>

namespace Kohiko
{

class BSPSplit final : public BSPNode
{
public:

    explicit BSPSplit(
        SplitDirection direction
    );

    bool IsLeaf() const override;

    SplitDirection Direction() const;

    void SetDirection(
        SplitDirection direction
    );

    float Ratio() const;

    void SetRatio(
        float ratio
    );

    BSPNode* Left() const;

    BSPNode* Right() const;

    void SetLeft(
        std::unique_ptr<BSPNode> node
    );

    void SetRight(
        std::unique_ptr<BSPNode> node
    );

    std::unique_ptr<BSPNode> TakeLeft();

    std::unique_ptr<BSPNode> TakeRight();

private:

    SplitDirection m_direction;

    float m_ratio = 0.5f;

    std::unique_ptr<BSPNode> m_left;

    std::unique_ptr<BSPNode> m_right;

};

}