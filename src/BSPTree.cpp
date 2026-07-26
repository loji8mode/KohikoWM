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
    if (m_lastFocused)
        return m_lastFocused->Window();

    // Fallback: nothing has ever been focused (e.g. right after the
    // very first window opened) - just take whatever leaf exists.
    std::vector<BSPLeaf*> leaves;
    CollectLeaves(m_root.get(), leaves);

    return leaves.empty() ? nullptr : leaves.back()->Window();
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

    BSPLeaf* anchor = m_lastFocused;

    if (!anchor)
    {
        std::vector<BSPLeaf*> leaves;
        CollectLeaves(m_root.get(), leaves);

        if (leaves.empty())
            return;

        anchor = leaves.back();
    }

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

    // Ratio is the *first* child's share. Dragging should grow
    // whichever window you actually grabbed in the direction you drag
    // it - so if `window` is the second child, a positive delta (drag
    // right/down) must shrink the first child's share instead of
    // growing it, i.e. the sign flips.
    if (parent->Right() == leaf)
        delta = -delta;

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
