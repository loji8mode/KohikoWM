#include "Bar.h"

#include "XConnection.h"

namespace Kohiko
{

Bar::Bar(
    XConnection& connection)
    :
    m_connection(connection)
{
}

void Bar::Create()
{
}

void Bar::Destroy()
{
}

void Bar::Show()
{
    m_visible = true;
}

void Bar::Hide()
{
    m_visible = false;
}

void Bar::Redraw()
{
}

void Bar::SetWorkspace(
    int workspace)
{
    m_workspace = workspace;
}

void Bar::SetTitle(
    const std::string& title)
{
    m_title = title;
}

}