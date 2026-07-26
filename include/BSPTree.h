#pragma once

#include "BSPLeaf.h"
#include "BSPSplit.h"

#include <memory>

namespace Kohiko
{

class ManagedWindow;

class BSPTree
{
public:

    BSPTree();

    bool Empty() const;

    void Clear();

    BSPNode* Root() const;

    ManagedWindow* FocusedWindow() const;

    void Insert(
        ManagedWindow* window
    );

    void Remove(
        ManagedWindow* window
    );

    void Focus(
        ManagedWindow* window
    );

    void Swap(
        ManagedWindow* first,
        ManagedWindow* second
    );

    void Resize(
        ManagedWindow* window,
        float delta
    );

private:

    BSPLeaf* FindLeaf(
        BSPNode* node,
        ManagedWindow* window
    ) const;

    BSPLeaf* FindFocused(
        BSPNode* node
    ) const;

    BSPNode* FindSibling(
        BSPLeaf* leaf
    ) const;

private:

    std::unique_ptr<BSPNode> m_root;

};

}