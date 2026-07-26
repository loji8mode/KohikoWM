// kohikoctl - the command-line client for Kohiko's IPCServer, in the
// spirit of hyprctl: connect, send one line, print whatever comes
// back, disconnect.
//
//   kohikoctl dispatch workspace 3
//   kohikoctl clients | monitors | activewindow | tree
//   kohikoctl reload
//   kohikoctl quit

#include "IpcPath.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <string>

namespace
{

void PrintUsage()
{
    std::fprintf(
        stderr,
        "usage: kohikoctl <command> [args...]\n"
        "\n"
        "  kohikoctl dispatch <action>    e.g. kohikoctl dispatch workspace 3\n"
        "  kohikoctl clients              JSON list of every managed window\n"
        "  kohikoctl monitors             JSON list of detected monitors\n"
        "  kohikoctl activewindow         JSON info for the focused window\n"
        "  kohikoctl tree                 JSON dump of the current workspace's BSP tree\n"
        "  kohikoctl reload               re-read the config file\n"
        "  kohikoctl quit                 ask kohiko to exit\n"
    );
}

}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    std::string request;

    for (int i = 1; i < argc; ++i)
    {
        if (i > 1)
            request += ' ';

        request += argv[i];
    }

    std::string path = Kohiko::IpcSocketPath();

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
    {
        std::perror("kohikoctl: socket");
        return 1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        std::fprintf(stderr, "kohikoctl: could not connect to %s (is kohiko running?)\n", path.c_str());
        close(fd);
        return 1;
    }

    request += '\n';
    ssize_t sent = write(fd, request.data(), request.size());
    (void)sent;

    shutdown(fd, SHUT_WR);

    std::string response;
    char buffer[4096];
    ssize_t received;

    while ((received = read(fd, buffer, sizeof(buffer))) > 0)
        response.append(buffer, static_cast<std::size_t>(received));

    close(fd);

    std::fputs(response.c_str(), stdout);

    return 0;
}
