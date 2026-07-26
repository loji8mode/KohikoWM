#pragma once

#include "Types.h"

namespace Kohiko
{

class BSPNode;

class LayoutEngine
{
public:

    LayoutEngine();

    void Apply(
        BSPNode* root,
        const Rect& area
    );

private:

    void Calculate(
        BSPNode* node,
        const Rect& area
    );

};

}