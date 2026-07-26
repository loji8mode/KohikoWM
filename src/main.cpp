#include "Application.h"

#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    std::string configPath;

    if (argc > 1)
    {
        configPath = argv[1];
    }
    else
    {
        const char* home = std::getenv("HOME");
        configPath = (home ? std::string(home) : std::string(".")) + "/.config/kohiko/kohiko.conf";
    }

    Kohiko::Application application;

    return application.Run(configPath);
}
