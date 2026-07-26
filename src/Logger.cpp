#include "Logger.h"

#include <iostream>

namespace Kohiko
{

void Logger::Info(const std::string& text)
{
    std::cout
        << "[INFO] "
        << text
        << '\n';
}

void Logger::Warning(const std::string& text)
{
    std::cout
        << "[WARNING] "
        << text
        << '\n';
}

void Logger::Error(const std::string& text)
{
    std::cerr
        << "[ERROR] "
        << text
        << '\n';
}

void Logger::Fatal(const std::string& text)
{
    std::cerr
        << "[FATAL] "
        << text
        << '\n';
}

}