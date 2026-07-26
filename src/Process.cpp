#include "Process.h"

#include <cstdlib>

namespace Kohiko
{

void Process::Spawn(
    const std::string& command)
{
    if(command.empty())
        return;

    std::string cmd =
        command + " &";

    std::system(
        cmd.c_str());
}

}