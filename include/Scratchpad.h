#pragma once

#include "Types.h"

namespace Kohiko
{

class WindowRepository;

class Scratchpad
{
public:

    explicit Scratchpad(
        WindowRepository& repository
    );

    void Toggle();

    void Add(
        WindowID window
    );

    bool Contains(
        WindowID window
    ) const;

private:

    WindowRepository& m_repository;

    WindowID m_window = 0;

    bool m_visible = false;

};

}