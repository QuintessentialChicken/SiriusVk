//
// Created by Leon on 19/09/2025.
//

#pragma once
#include "vkRenderer.h"

namespace sirius {
class srsRenderer {
public:
    static void init();
    static void draw();
private:
    static srsVkRenderer vkRenderer;
};
}
