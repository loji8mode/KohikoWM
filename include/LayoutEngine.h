#pragma once

#include "Types.h"

namespace Kohiko
{

class BSPNode;

// Walks a BSPTree and turns ratios into pixels.
//
// Fullscreen and floating windows are handled by WindowManager
// directly (fullscreen bypasses the tree entirely; floating windows
// are simply never inserted into it) - this class only ever sees
// the tiled subtree, which keeps its job to exactly one thing: gaps,
// borders, and the smart-gaps/smart-borders rule.
class LayoutEngine
{
public:

    struct Params
    {
        int innerGap = 6;
        int outerGap = 6;
        int borderWidth = 2;
        bool smartGaps = true;
        bool smartBorders = true;
    };

    LayoutEngine();

    // Fills in every node's slot Geometry() (used for hit-testing and
    // Super+h/j/k/l neighbor search) and every leaf window's final
    // on-screen content rect + border width.
    void Apply(BSPNode* root, const Rect& area, const Params& params);

private:

    void Calculate(BSPNode* node, const Rect& area, int innerGap, int borderWidth);

    int CountLeaves(BSPNode* node) const;

};

}
