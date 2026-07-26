#pragma once

#include <string>

namespace Kohiko
{

class Process
{
public:

    static void Spawn(
        const std::string& command
    );

};

}