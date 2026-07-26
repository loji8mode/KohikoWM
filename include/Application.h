#pragma once

#include "Config.h"
#include "XConnection.h"

#include <string>

namespace Kohiko
{

// Thin bootstrap: connect to X11, load the config, then build and run
// a WindowManager + EventLoop as local objects in Run() - nothing
// about them needs to outlive the run, so there's no ownership puzzle
// to solve here.
class Application
{
public:

    int Run(
        const std::string& configPath
    );

private:

    XConnection m_connection;
    Config m_config;

};

}
