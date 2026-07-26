#include "BSPTree.h"

#include "ManagedWindow.h"

namespace Kohiko
{

namespace
{

SplitDirection NextDirection(
    SplitDirection direction)
{
    return
        direction ==
        SplitDirection::Vertical
        ?
        SplitDirection::Horizontal
        :
        SplitDirection::Vertical;
}

}

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
}

BSPNode* BSPTree::Root() const
{
    return m_root.get();
}

ManagedWindow* BSPTree::FocusedWindow() const
{
    auto* leaf =
        FindFocused(
            m_root.get());

    if(!leaf)
        return nullptr;

    return leaf->Window();
}

void BSPTree::Insert(
    ManagedWindow* window)
{
    if(!window)
        return;

    if(!m_root)
    {
        m_root =
            std::make_unique<BSPLeaf>(
                window);

        return;
    }

    auto* focused =
        FindFocused(
            m_root.get());

    if(!focused)
        focused =
            FindLeaf(
                m_root.get(),
                nullptr);

    auto parent =
        std::make_unique<BSPSplit>(
            SplitDirection::Vertical);

    auto oldLeaf =
        std::make_unique<BSPLeaf>(
            focused->Window());

    auto newLeaf =
        std::make_unique<BSPLeaf>(
            window);

    parent->SetLeft(
        std::move(oldLeaf));

    parent->SetRight(
        std::move(newLeaf));

    if(focused == m_root.get())
    {
        m_root =
            std::move(parent);

        return;
    }

    BSPSplit* splitParent =
        focused->Parent();

    if(!splitParent)
        return;

    if(splitParent->Left() == focused)
    {
        splitParent->SetLeft(
            std::move(parent));
    }
    else
    {
        splitParent->SetRight(
            std::move(parent));
    }
}

void BSPTree::Remove(
    ManagedWindow* window)
{
    if(!window)
        return;

    auto* leaf =
        FindLeaf(
            m_root.get(),
            window);

    if(!leaf)
        return;

    if(leaf == m_root.get())
    {
        m_root.reset();

        return;
    }

    BSPSplit* parent =
        leaf->Parent();

    if(!parent)
        return;

    std::unique_ptr<BSPNode> survivor;

    if(parent->Left() == leaf)
        survivor =
            parent->TakeRight();
    else
        survivor =
            parent->TakeLeft();

    BSPSplit* grand =
        parent->Parent();

    if(!grand)
    {
        m_root =
            std::move(survivor);

        if(m_root)
            m_root->SetParent(nullptr);

        return;
    }

    if(grand->Left() == parent)
        grand->SetLeft(
            std::move(survivor));
    else
        grand->SetRight(
            std::move(survivor));
}

void BSPTree::Focus(
    ManagedWindow* window)
{
    if(!window)
        return;

    auto leaves =
        FindLeaf(
            m_root.get(),
            nullptr);

    (void)leaves;

    window->SetFocused(
        true);
}

void BSPTree::Swap(
    ManagedWindow* first,
    ManagedWindow* second)
{
    if(!first || !second)
        return;

    Rect tmp =
        first->Geometry();

    first->SetGeometry(
        second->Geometry());

    second->SetGeometry(
        tmp);
}

void BSPTree::Resize(
    ManagedWindow* window,
    float delta)
{
    auto* leaf =
        FindLeaf(
            m_root.get(),
            window);

    if(!leaf)
        return;

    auto* parent =
        leaf->Parent();

    if(!parent)
        return;

    parent->SetRatio(
        parent->Ratio() + delta);
}

BSPLeaf* BSPTree::FindLeaf(
    BSPNode* node,
    ManagedWindow* window) const
{
    if(!node)
        return nullptr;

    if(node->IsLeaf())
    {
        auto* leaf =
            static_cast<BSPLeaf*>(node);

        if(!window)
            return leaf;

        if(leaf->Window() == window)
            return leaf;

        return nullptr;
    }

    auto* split =
        static_cast<BSPSplit*>(node);

    if(auto* left =
        FindLeaf(
            split->Left(),
            window))
        return left;

    return FindLeaf(
        split->Right(),
        window);
}

BSPLeaf* BSPTree::FindFocused(
    BSPNode* node) const
{
    if(!node)
        return nullptr;

    if(node->IsLeaf())
    {
        auto* leaf =
            static_cast<BSPLeaf*>(node);

        if(leaf->Window()->Focused())
            return leaf;

        return nullptr;
    }

    auto* split =
        static_cast<BSPSplit*>(node);

    if(auto* left =
        FindFocused(
            split->Left()))
        return left;

    return FindFocused(
        split->Right());
}

BSPNode* BSPTree::FindSibling(
    BSPLeaf* leaf) const
{
    if(!leaf)
        return nullptr;

    auto* parent =
        leaf->Parent();

    if(!parent)
        return nullptr;

    if(parent->Left() == leaf)
        return parent->Right();

    return parent->Left();
}

}