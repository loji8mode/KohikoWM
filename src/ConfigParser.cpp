#include "ConfigParser.h"

namespace Kohiko
{

bool ConfigParser::Parse(
    const std::string& path,
    Config& config)
{
    return config.Load(path);
}

}
