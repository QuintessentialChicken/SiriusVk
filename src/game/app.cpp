//
// Created by Leon on 16/09/2025.
//

#include "app.h"

#include <iostream>
#include <optional>

#include "graphics/renderer.h"
#include "third-party/imgui/imgui_impl_vulkan.h"
#include "third-party/imgui/imgui_impl_win32.h"
#include "window/window.h"
#include "window/wndProc.h"

fsm::fsm_return app::updateState(signed short state) {
    switch (state) {
        case INIT_SYSTEM:
            return init();
        case RUN_GAME:
            return runGame();
        case SHUTDOWN_SYSTEM:
            return shutdown();
        default:
            return UNHANDLED;
    }
}

fsm::fsm_return app::init() {
    if (!hwndMain) {
        hwndMain = sirius::srsWindow::createDeviceWindow();
    }
    sirius::srsWindow::setWindowTitle("Sirius Vulkan");
    sirius::srsRenderer::init();
    setState(RUN_GAME);
    return CONTINUE;
}

fsm::fsm_return app::shutdown() {
    return EXIT;
}

fsm::fsm_return app::runGame() {
    MSG message;
    std::optional<int> exitCode = {};
    if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            exitCode = static_cast<int>(message.wParam);
            std::cout << "Exiting" << std::endl;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    if (exitCode == 0) {
        setState(SHUTDOWN_SYSTEM);
        return CONTINUE;
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::Render();
    sirius::srsRenderer::draw();
    return CONTINUE;
}
