#include "Logger.h"

#include <iostream>

namespace Kohiko
{

// Kohiko runs as a long-lived daemon, so stdout is flushed after every
// message - otherwise, once it's redirected to a log file instead of a
// terminal, libc's full buffering means these might never actually hit
// disk before the process is killed.

void Logger::Info(const std::string& text)
{
    std::cout << "[INFO] " << text << std::endl;
}

void Logger::Warning(const std::string& text)
{
    std::cout << "[WARNING] " << text << std::endl;
}

void Logger::Error(const std::string& text)
{
    std::cerr << "[ERROR] " << text << std::endl;
}

void Logger::Fatal(const std::string& text)
{
    std::cerr << "[FATAL] " << text << std::endl;
}

}
