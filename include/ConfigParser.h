#pragma once

#include "Config.h"

#include <string>

namespace Kohiko
{

// Thin wrapper kept around so config *loading* and config *reading*
// are separate concerns even though, today, loading is just
// Config::Load(). Reload (kohikoctl / Super+Shift+R-style binds, if
// added later) always goes through here.
class ConfigParser
{
public:

    bool Parse(
        const std::string& path,
        Config& config
    );

};

}
