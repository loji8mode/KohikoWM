#pragma once

#include "Types.h"

#include <vector>

namespace Kohiko
{

class Workspace;

class Monitor
{
public:

    explicit Monitor(
        int id
    );

    int Id() const;

    void SetGeometry(
        const Rect& rect
    );

    const Rect& Geometry() const;

    void SetWorkspace(
        Workspace* workspace
    );

    Workspace* ActiveWorkspace() const;

private:

    int m_id;

    Rect m_geometry;

    Workspace* m_workspace = nullptr;

};

}