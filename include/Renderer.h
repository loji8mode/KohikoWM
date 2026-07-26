#pragma once

namespace Kohiko
{

class WindowManager;

class Renderer
{
public:

    explicit Renderer(
        WindowManager& wm
    );

    void Render();

private:

    WindowManager& m_windowManager;

};

}