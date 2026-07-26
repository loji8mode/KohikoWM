#pragma once

#include <string>

namespace Kohiko
{

// The Unix socket path Kohiko's IPCServer listens on and kohikoctl
// connects to. Tied to $DISPLAY so a nested Xephyr test session and a
// real session don't collide.
std::string IpcSocketPath();

}
