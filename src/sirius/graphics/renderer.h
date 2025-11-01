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

    static void ResizeViewport();

    static bool ResizeRequested();
private:
    static SrsVkRenderer vkRenderer_;
};
}
