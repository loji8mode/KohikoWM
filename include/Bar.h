#pragma once

#include <string>
#include <vector>

namespace Kohiko
{

class XConnection;

class Bar
{
public:

    explicit Bar(
        XConnection& connection
    );

    void Create();

    void Destroy();

    void Show();

    void Hide();

    void Redraw();

    void SetWorkspace(
        int workspace
    );

    void SetTitle(
        const std::string& title
    );

private:

    XConnection& m_connection;

    unsigned long m_window = 0;

    bool m_visible = true;

    int m_workspace = 1;

    std::string m_title;

};

}