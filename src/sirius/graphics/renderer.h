//
// Created by Leon on 19/09/2025.
//

#pragma once
#include "vkRenderer.h"

namespace sirius {
class renderer {
public:
    static void init();
private:
    static srsVkRenderer vkRenderer;
};
}
