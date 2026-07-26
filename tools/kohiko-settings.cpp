#include "SettingsWindow.h"

int main()
{
    Kohiko::SettingsWindow window;

    if (!window.Initialize())
        return 1;

    window.Run();

    return 0;
}
