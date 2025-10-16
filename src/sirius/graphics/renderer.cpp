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

void SrsRenderer::ResizeViewport() {
    vkRenderer_.ResizeSwapChain();
}

bool SrsRenderer::ResizeRequested() {
    return vkRenderer_.ResizeRequested();
}
}
