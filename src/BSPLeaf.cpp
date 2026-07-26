#include "BSPLeaf.h"

#include "ManagedWindow.h"

namespace Kohiko
{

BSPLeaf::BSPLeaf(
    ManagedWindow* window)
    :
    m_window(window)
{
}

bool BSPLeaf::IsLeaf() const
{
    return true;
}

ManagedWindow* BSPLeaf::Window() const
{
    return m_window;
}

}