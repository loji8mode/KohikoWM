#pragma once

#include "Config.h"

#include <string>

namespace Kohiko
{

class ConfigParser
{
public:

    bool Parse(
        const std::string& path,
        Config& config
    );

};

}