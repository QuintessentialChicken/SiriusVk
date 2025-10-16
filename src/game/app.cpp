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

Fsm::FsmReturn app::UpdateState(signed short state) {
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

Fsm::FsmReturn app::init() {
    if (!hwndMain) {
        hwndMain = sirius::SrsWindow::CreateDeviceWindow();
    }
    sirius::SrsWindow::SetWindowTitle("Sirius Vulkan");
    sirius::SrsRenderer::Init();
    SetState(RUN_GAME);
    return CONTINUE;
}

Fsm::FsmReturn app::shutdown() {
    return EXIT;
}

Fsm::FsmReturn app::runGame() {
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
        SetState(SHUTDOWN_SYSTEM);
        return CONTINUE;
    }
    if (sirius::SrsRenderer::ResizeRequested()) sirius::SrsRenderer::ResizeViewport();
    sirius::SrsRenderer::SpawnImguiWindow();
    sirius::SrsRenderer::Draw();
    return CONTINUE;
}
