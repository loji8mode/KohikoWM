#include "Workspace.h"

namespace Kohiko
{

Workspace::Workspace(
    int id)
    :
    m_id(id)
{
}

int Workspace::Id() const
{
    return m_id;
}

BSPTree&
Workspace::Tree()
{
    return m_tree;
}

const BSPTree&
Workspace::Tree() const
{
    return m_tree;
}

}