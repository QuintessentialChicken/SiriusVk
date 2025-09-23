//
// Created by Leon on 19/09/2025.
//

#include "renderer.h"


namespace sirius {
srsVkRenderer srsRenderer::vkRenderer = srsVkRenderer{};

void srsRenderer::init() {
    vkRenderer.init();
}

void srsRenderer::draw() {
    vkRenderer.draw();
}
}
