#pragma once

#include "BSPNode.h"

namespace Kohiko
{

class ManagedWindow;

class BSPLeaf
    :
    public BSPNode
{
public:

    explicit BSPLeaf(
        ManagedWindow* window);

    bool IsLeaf() const override;

    ManagedWindow* Window() const;

private:

    ManagedWindow* m_window;

};

}