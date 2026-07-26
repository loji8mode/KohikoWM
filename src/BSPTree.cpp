#include "BSPTree.h"

#include "ManagedWindow.h"

#include <algorithm>
#include <sstream>

namespace Kohiko
{

BSPTree::BSPTree()
{
}

bool BSPTree::Empty() const
{
    return m_root == nullptr;
}

void BSPTree::Clear()
{
    m_root.reset();
    m_lastFocused = nullptr;
}

BSPNode* BSPTree::Root() const
{
    return m_root.get();
}

int BSPTree::Count() const
{
    std::vector<BSPLeaf*> leaves;
    CollectLeaves(m_root.get(), leaves);
    return static_cast<int>(leaves.size());
}

ManagedWindow* BSPTree::FocusedWindow() const
{
    BSPLeaf* anchor = AnchorLeaf();
    return anchor ? anchor->Window() : nullptr;
}

BSPLeaf* BSPTree::AnchorLeaf() const
{
    if (m_lastFocused)
        return m_lastFocused;

    // Fallback: nothing has ever been focused (e.g. right after the
    // very first window opened) - just take whatever leaf exists.
    std::vector<BSPLeaf*> leaves;
    CollectLeaves(m_root.get(), leaves);

    return leaves.empty() ? nullptr : leaves.back();
}

void BSPTree::Insert(ManagedWindow* window)
{
    if (!window)
        return;

    if (!m_root)
    {
        auto leaf = std::make_unique<BSPLeaf>(window);
        m_lastFocused = leaf.get();
        m_root = std::move(leaf);
        return;
    }

    BSPLeaf* anchor = AnchorLeaf();

    if (!anchor)
        return;

    SplitDirection direction = DirectionForRect(anchor->Geometry());

    auto newSplit = std::make_unique<BSPSplit>(direction);
    auto newLeaf  = std::make_unique<BSPLeaf>(window);

    BSPSplit* parent = anchor->Parent();

    // Detach the anchor from wherever it currently lives so it can be
    // re-parented under the new split, then put the new split back in
    // exactly the slot the anchor used to occupy.
    std::unique_ptr<BSPNode> anchorOwned =
        parent
            ? ((parent->Left() == anchor) ? parent->TakeLeft() : parent->TakeRight())
            : std::move(m_root);

    newSplit->SetLeft(std::move(anchorOwned));
    newSplit->SetRight(std::move(newLeaf));

    BSPLeaf* newLeafPtr = static_cast<BSPLeaf*>(newSplit->Right());

    if (parent)
    {
        if (parent->Left() == nullptr)
            parent->SetLeft(std::move(newSplit));
        else
            parent->SetRight(std::move(newSplit));
    }
    else
    {
        m_root = std::move(newSplit);
    }

    m_lastFocused = newLeafPtr;
}

void BSPTree::Remove(ManagedWindow* window)
{
    if (!window)
        return;

    BSPLeaf* leaf = FindLeaf(m_root.get(), window);

    if (!leaf)
        return;

    if (leaf == m_lastFocused)
        m_lastFocused = nullptr;

    if (leaf == m_root.get())
    {
        m_root.reset();
        return;
    }

    BSPSplit* parent = leaf->Parent();

    if (!parent)
        return;

    std::unique_ptr<BSPNode> survivor =
        (parent->Left() == leaf) ? parent->TakeRight() : parent->TakeLeft();

    BSPSplit* grandparent = parent->Parent();

    if (!grandparent)
    {
        m_root = std::move(survivor);

        if (m_root)
            m_root->SetParent(nullptr);

        return;
    }

    if (grandparent->Left() == parent)
        grandparent->SetLeft(std::move(survivor));
    else
        grandparent->SetRight(std::move(survivor));
}

void BSPTree::Focus(ManagedWindow* window)
{
    if (!window)
        return;

    BSPLeaf* leaf = FindLeaf(m_root.get(), window);

    if (leaf)
        m_lastFocused = leaf;
}

void BSPTree::Swap(ManagedWindow* first, ManagedWindow* second)
{
    if (!first || !second || first == second)
        return;

    BSPLeaf* leafA = FindLeaf(m_root.get(), first);
    BSPLeaf* leafB = FindLeaf(m_root.get(), second);

    if (!leafA || !leafB)
        return;

    // The actual fix versus the old implementation: re-point the
    // leaves at each other's windows instead of copying Geometry()
    // rects around. Geometry is untouched, exactly as the spec asks
    // ("не міняти геометрію"); only the window <-> leaf assignment
    // changes, so it survives the very next LayoutEngine pass instead
    // of being silently overwritten by it.
    leafA->SetWindow(second);
    leafB->SetWindow(first);

    if (m_lastFocused == leafA || m_lastFocused == leafB)
    {
        if (first->Focused())
            m_lastFocused = leafB;
        else if (second->Focused())
            m_lastFocused = leafA;
    }
}

void BSPTree::Resize(ManagedWindow* window, int dx, int dy)
{
    if (!window)
        return;

    BSPLeaf* leaf = FindLeaf(m_root.get(), window);

    if (!leaf)
        return;

    BSPSplit* parent = leaf->Parent();

    if (!parent)
        return; // only window on the workspace - nothing to resize against

    const Rect& area = parent->Geometry();

    float delta = 0.0f;

    if (parent->Direction() == SplitDirection::Vertical)
    {
        if (area.width > 0)
            delta = static_cast<float>(dx) / static_cast<float>(area.width);
    }
    else
    {
        if (area.height > 0)
            delta = static_cast<float>(dy) / static_cast<float>(area.height);
    }

    // Ratio is the *first* child's share, and the divider between the
    // two children sits at a screen coordinate of area.(x|y) +
    // first-child-size - so adding delta straight to it moves that
    // divider in the same screen direction you're actually dragging
    // the mouse (right/down = the divider's own coordinate increases),
    // for both children, not just the first one.
    //
    // An earlier version flipped delta's sign whenever `window` was
    // the *second* child, on the theory that "the grabbed window
    // should always grow toward wherever you drag it". In practice
    // that makes the divider itself walk backwards relative to the
    // mouse whenever you grab the right/bottom window: e.g. dragging
    // right with the cursor over a window docked on the right half of
    // the screen would shrink it - its divider sliding left while the
    // cursor moves right - which reads as the window moving inverted
    // to the mouse. Applying delta unconditionally keeps the divider
    // tracking the mouse 1:1 regardless of which side of it you
    // grabbed, matching ordinary drag-to-resize behaviour; which
    // window grows vs. shrinks is then just a consequence of which
    // side of the divider it's on, exactly like dragging a border
    // anywhere else.
    parent->SetRatio(parent->Ratio() + delta);
}

void BSPTree::Rotate(ManagedWindow* window)
{
    if (!window)
        return;

    BSPLeaf* leaf = FindLeaf(m_root.get(), window);

    if (!leaf)
        return;

    BSPSplit* parent = leaf->Parent();

    if (!parent)
        return;

    parent->SetDirection(
        parent->Direction() == SplitDirection::Vertical
            ? SplitDirection::Horizontal
            : SplitDirection::Vertical);
}

void BSPTree::Flip(ManagedWindow* window)
{
    if (!window)
        return;

    BSPLeaf* leaf = FindLeaf(m_root.get(), window);

    if (!leaf)
        return;

    BSPSplit* parent = leaf->Parent();

    if (!parent)
        return;

    parent->SwapChildren();
}

ManagedWindow* BSPTree::FindNeighbor(ManagedWindow* window, Direction direction) const
{
    if (!window)
        return nullptr;

    BSPLeaf* leaf = FindLeaf(m_root.get(), window);

    if (!leaf)
        return nullptr;

    std::vector<BSPLeaf*> leaves;
    CollectLeaves(m_root.get(), leaves);

    const Rect& from = leaf->Geometry();

    BSPLeaf* best = nullptr;
    int bestGap = 0;
    int bestOverlap = 0;

    for (BSPLeaf* candidate : leaves)
    {
        if (candidate == leaf)
            continue;

        const Rect& to = candidate->Geometry();

        bool matches = false;
        int gap = 0;
        int overlap = 0;

        switch (direction)
        {
            case Direction::Right:

                matches = to.x >= from.Right() - 1;
                gap     = to.x - from.Right();
                overlap = std::min(from.Bottom(), to.Bottom()) - std::max(from.y, to.y);
                break;

            case Direction::Left:

                matches = to.Right() <= from.x + 1;
                gap     = from.x - to.Right();
                overlap = std::min(from.Bottom(), to.Bottom()) - std::max(from.y, to.y);
                break;

            case Direction::Down:

                matches = to.y >= from.Bottom() - 1;
                gap     = to.y - from.Bottom();
                overlap = std::min(from.Right(), to.Right()) - std::max(from.x, to.x);
                break;

            case Direction::Up:

                matches = to.Bottom() <= from.y + 1;
                gap     = from.y - to.Bottom();
                overlap = std::min(from.Right(), to.Right()) - std::max(from.x, to.x);
                break;
        }

        if (!matches || overlap <= 0)
            continue;

        if (gap < 0)
            gap = 0;

        if (!best || gap < bestGap || (gap == bestGap && overlap > bestOverlap))
        {
            best = candidate;
            bestGap = gap;
            bestOverlap = overlap;
        }
    }

    return best ? best->Window() : nullptr;
}

ManagedWindow* BSPTree::HitTest(const Point& p) const
{
    std::vector<BSPLeaf*> leaves;
    CollectLeaves(m_root.get(), leaves);

    for (BSPLeaf* leaf : leaves)
    {
        if (leaf->Geometry().Contains(p))
            return leaf->Window();
    }

    return nullptr;
}

bool BSPTree::HasSpaceForAnotherWindow(
    const Rect& tilingArea,
    int innerGap,
    int minWidth,
    int minHeight) const
{
    if (!m_root)
        return true; // first window on this workspace always fits

    BSPLeaf* anchor = AnchorLeaf();

    if (!anchor)
        return true; // unreachable in practice (m_root implies >=1 leaf), but harmless

    Rect anchorRect;

    if (!ComputeLeafGeometry(m_root.get(), anchor, tilingArea, innerGap, anchorRect))
        return true; // couldn't locate the anchor - don't block insertion over it

    // Mirrors exactly what Insert() is about to do: wrap the anchor in
    // a brand-new 50/50 split, oriented from its current aspect ratio.
    BSPSplit probe(DirectionForRect(anchorRect));

    Rect first;
    Rect second;
    probe.Subdivide(anchorRect, innerGap, first, second);

    return first.width  >= minWidth  && first.height  >= minHeight &&
           second.width >= minWidth  && second.height >= minHeight;
}

std::string BSPTree::Serialize() const
{
    return m_root ? SerializeNode(m_root.get()) : "null";
}

BSPLeaf* BSPTree::FindLeaf(BSPNode* node, ManagedWindow* window) const
{
    if (!node)
        return nullptr;

    if (node->IsLeaf())
    {
        auto* leaf = static_cast<BSPLeaf*>(node);
        return (leaf->Window() == window) ? leaf : nullptr;
    }

    auto* split = static_cast<BSPSplit*>(node);

    if (auto* found = FindLeaf(split->Left(), window))
        return found;

    return FindLeaf(split->Right(), window);
}

void BSPTree::CollectLeaves(BSPNode* node, std::vector<BSPLeaf*>& out) const
{
    if (!node)
        return;

    if (node->IsLeaf())
    {
        out.push_back(static_cast<BSPLeaf*>(node));
        return;
    }

    auto* split = static_cast<BSPSplit*>(node);

    CollectLeaves(split->Left(), out);
    CollectLeaves(split->Right(), out);
}

bool BSPTree::ComputeLeafGeometry(
    BSPNode* node,
    BSPLeaf* target,
    const Rect& area,
    int innerGap,
    Rect& out) const
{
    if (!node)
        return false;

    if (node->IsLeaf())
    {
        if (node != target)
            return false;

        out = area;
        return true;
    }

    auto* split = static_cast<BSPSplit*>(node);

    Rect first;
    Rect second;
    split->Subdivide(area, innerGap, first, second);

    if (ComputeLeafGeometry(split->Left(), target, first, innerGap, out))
        return true;

    return ComputeLeafGeometry(split->Right(), target, second, innerGap, out);
}

SplitDirection BSPTree::DirectionForRect(const Rect& rect)
{
    // Wide slot -> split it left/right; tall slot -> split it top/bottom.
    // Keeps automatically-tiled layouts from degenerating into thin strips.
    return (rect.width >= rect.height)
        ? SplitDirection::Vertical
        : SplitDirection::Horizontal;
}

std::string BSPTree::SerializeNode(const BSPNode* node) const
{
    if (!node)
        return "null";

    std::ostringstream out;

    if (node->IsLeaf())
    {
        const auto* leaf = static_cast<const BSPLeaf*>(node);
        const Rect& r = leaf->Geometry();

        out << "{\"type\":\"leaf\",\"window\":"
            << (leaf->Window() ? static_cast<unsigned long>(leaf->Window()->Id()) : 0UL)
            << ",\"x\":" << r.x << ",\"y\":" << r.y
            << ",\"width\":" << r.width << ",\"height\":" << r.height << "}";
    }
    else
    {
        const auto* split = static_cast<const BSPSplit*>(node);

        out << "{\"type\":\"split\",\"direction\":\""
            << (split->Direction() == SplitDirection::Vertical ? "vertical" : "horizontal")
            << "\",\"ratio\":" << split->Ratio()
            << ",\"first\":" << SerializeNode(split->Left())
            << ",\"second\":" << SerializeNode(split->Right())
            << "}";
    }

    return out.str();
}

}
