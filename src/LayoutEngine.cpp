#include "LayoutEngine.h"

#include "BSPLeaf.h"
#include "BSPSplit.h"
#include "ManagedWindow.h"

namespace Kohiko
{

LayoutEngine::LayoutEngine()
{
}

void LayoutEngine::Apply(
    BSPNode* root,
    const Rect& area)
{
    if (!root)
        return;

    Calculate(
        root,
        area
    );
}

void LayoutEngine::Calculate(
    BSPNode* node,
    const Rect& area)
{
    node->SetGeometry(area);

    if (node->IsLeaf())
    {
        auto* leaf =
            static_cast<BSPLeaf*>(node);

        leaf->Window()->SetGeometry(area);

        return;
    }

    auto* split =
        static_cast<BSPSplit*>(node);

    Rect left = area;
    Rect right = area;

    if (split->Direction() ==
        SplitDirection::Vertical)
    {
        left.width =
            static_cast<int>(
                area.width *
                split->Ratio());

        right.x =
            area.x +
            left.width;

        right.width =
            area.width -
            left.width;
    }
    else
    {
        left.height =
            static_cast<int>(
                area.height *
                split->Ratio());

        right.y =
            area.y +
            left.height;

        right.height =
            area.height -
            left.height;
    }

    Calculate(
        split->Left(),
        left
    );

    Calculate(
        split->Right(),
        right
    );
}

}