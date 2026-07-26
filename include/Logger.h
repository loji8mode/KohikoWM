#pragma once

#include <string>

namespace Kohiko
{

class Logger
{
public:

    static void Info(const std::string& text);

    static void Warning(const std::string& text);

    static void Error(const std::string& text);

    static void Fatal(const std::string& text);

};

}