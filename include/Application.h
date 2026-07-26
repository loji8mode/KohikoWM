#pragma once

#include "EventLoop.h"
#include "WindowManager.h"
#include "XConnection.h"

namespace Kohiko
{

class Application
{
public:

    int Run();

private:

    XConnection m_connection;

    WindowManager* m_windowManager = nullptr;

    EventLoop* m_eventLoop = nullptr;

};

}