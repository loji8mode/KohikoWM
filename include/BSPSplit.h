#pragma once

#include "BSPNode.h"

#include <memory>

namespace Kohiko
{

// An internal node: exactly two children (First/Second) divided by
// `direction` at proportion `ratio` (the fraction First receives).
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

    // Flip(): mirrors the two panes without touching the split axis.
    void SwapChildren();

    // Splits `area` into the two child rects this split would produce
    // given its own direction/ratio and `innerGap` between them.
    // Shared by LayoutEngine (real layout) and BSPTree (the "would
    // this leaf still be a legal size after one more split?" capacity
    // check) so the two pieces of math can never drift apart.
    void Subdivide(
        const Rect& area,
        int innerGap,
        Rect& first,
        Rect& second
    ) const;

private:

    SplitDirection m_direction;

    float m_ratio = 0.5f;

    std::unique_ptr<BSPNode> m_left;

    std::unique_ptr<BSPNode> m_right;

};

}
