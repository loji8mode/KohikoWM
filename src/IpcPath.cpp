#include "IpcPath.h"

#include <cstdlib>

namespace Kohiko
{

std::string IpcSocketPath()
{
    const char* display = std::getenv("DISPLAY");

    std::string tag = display ? display : ":0";

    for (char& c : tag)
    {
        if (c == ':' || c == '.')
            c = '_';
    }

    return "/tmp/kohiko" + tag + ".sock";
}

}
