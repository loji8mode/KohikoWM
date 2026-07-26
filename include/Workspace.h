#pragma once

#include "BSPTree.h"

namespace Kohiko
{

class Workspace
{
public:

    explicit Workspace(
        int id
    );

    int Id() const;

    BSPTree& Tree();

    const BSPTree& Tree() const;

private:

    int m_id;

    BSPTree m_tree;

};

}