#include "LayoutEngine.h"

#include "BSPLeaf.h"
#include "BSPSplit.h"
#include "ManagedWindow.h"

#include <algorithm>

namespace Kohiko
{

LayoutEngine::LayoutEngine()
{
}

void LayoutEngine::Apply(
    BSPNode* root,
    const Rect& area,
    const Params& params)
{
    if (!root)
        return;

    bool single = CountLeaves(root) <= 1;

    int outerGap = (single && params.smartGaps) ? 0 : params.outerGap;
    int innerGap = (single && params.smartGaps) ? 0 : params.innerGap;
    int border   = (single && params.smartBorders) ? 0 : params.borderWidth;

    Rect tilingArea = area.Shrunk(outerGap);

    Calculate(root, tilingArea, innerGap, border);
}

void LayoutEngine::Calculate(
    BSPNode* node,
    const Rect& area,
    int innerGap,
    int borderWidth)
{
    // The slot rect: contiguous, gap-free, used for hit-testing and
    // directional neighbor search.
    node->SetGeometry(area);

    if (node->IsLeaf())
    {
        auto* leaf = static_cast<BSPLeaf*>(node);
        ManagedWindow* window = leaf->Window();

        if (window)
        {
            Rect content = area;
            content.width  = std::max(1, area.width  - borderWidth * 2);
            content.height = std::max(1, area.height - borderWidth * 2);

            window->SetGeometry(content);
            window->SetBorderWidth(borderWidth);
        }

        return;
    }

    auto* split = static_cast<BSPSplit*>(node);

    Rect first  = area;
    Rect second = area;

    if (split->Direction() == SplitDirection::Vertical)
    {
        int usable      = std::max(0, area.width - innerGap);
        int firstWidth  = static_cast<int>(static_cast<float>(usable) * split->Ratio());
        int secondWidth = usable - firstWidth;

        first.width   = firstWidth;
        second.x      = area.x + firstWidth + innerGap;
        second.width  = secondWidth;
    }
    else
    {
        int usable       = std::max(0, area.height - innerGap);
        int firstHeight  = static_cast<int>(static_cast<float>(usable) * split->Ratio());
        int secondHeight = usable - firstHeight;

        first.height  = firstHeight;
        second.y      = area.y + firstHeight + innerGap;
        second.height = secondHeight;
    }

    Calculate(split->Left(),  first,  innerGap, borderWidth);
    Calculate(split->Right(), second, innerGap, borderWidth);
}

int LayoutEngine::CountLeaves(BSPNode* node) const
{
    if (!node)
        return 0;

    if (node->IsLeaf())
        return 1;

    auto* split = static_cast<BSPSplit*>(node);

    return CountLeaves(split->Left()) + CountLeaves(split->Right());
}

}
