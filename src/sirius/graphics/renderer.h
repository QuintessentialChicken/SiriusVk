//
// Created by Leon on 19/09/2025.
//

#pragma once
#include "vkRenderer.h"

namespace sirius {
class SrsRenderer {
public:
    static void Init();
    static void Draw();
    static void SpawnImguiWindow();
private:
    static SrsVkRenderer vkRenderer_;
};
}
