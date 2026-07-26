#include "CursorManager.h"

#include "XConnection.h"

#include <X11/cursorfont.h>

namespace Kohiko
{

CursorManager::CursorManager(
    XConnection& connection)
    :
    m_connection(connection)
{
}

void CursorManager::Initialize()
{
    Display* display = m_connection.GetDisplay();

    m_normalCursor = XCreateFontCursor(display, XC_left_ptr);
    m_resizeCursor = XCreateFontCursor(display, XC_sizing);
    m_dragCursor = XCreateFontCursor(display, XC_fleur);

    XDefineCursor(display, m_connection.Root(), m_normalCursor);
}

void CursorManager::SetResizing(
    bool resizing)
{
    if (!m_normalCursor)
        return;

    XDefineCursor(
        m_connection.GetDisplay(),
        m_connection.Root(),
        resizing ? m_resizeCursor : m_normalCursor);
}

void CursorManager::SetDragging(
    bool dragging)
{
    if (!m_normalCursor)
        return;

    XDefineCursor(
        m_connection.GetDisplay(),
        m_connection.Root(),
        dragging ? m_dragCursor : m_normalCursor);
}

}
