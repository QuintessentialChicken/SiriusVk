//
// Created by Leon on 19/09/2025.
//

#include "renderer.h"


namespace sirius {
SrsVkRenderer SrsRenderer::vkRenderer_ = SrsVkRenderer{};

void SrsRenderer::Init() {
    vkRenderer_.Init();
}

void SrsRenderer::Draw() {
    vkRenderer_.Draw();
}

void SrsRenderer::SpawnImguiWindow() {
    vkRenderer_.SpawnImguiWindow();
}
}
