//
// Created by Leon on 16/09/2025.
//

#include "app.h"

#include <iostream>
#include <optional>

#include "graphics/renderer.h"
#include "input/input_manager.h"
#include "third-party/imgui/imgui_impl_win32.h"
#include "window/window.h"
#include "window/wndProc.h"

Fsm::FsmReturn App::UpdateState(signed short state) {
    switch (state) {
        case kInitSystem:
            return Init();
        case kRunGame:
            return RunGame();
        case kShutdownSystem:
            return Shutdown();
        default:
            return kUnhandled;
    }
}

Fsm::FsmReturn App::Init() {
    if (!hwndMain) {
        hwndMain = sirius::SrsWindow::CreateDeviceWindow();
    }
    sirius::SrsWindow::SetWindowTitle("Sirius Vulkan");
    sirius::SrsRenderer::Init();
    sirius::InputManager::Init();
    SetState(kRunGame);
    return kContinue;
}

Fsm::FsmReturn App::Shutdown() {
    return kExit;
}

Fsm::FsmReturn App::RunGame() {
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
        SetState(kShutdownSystem);
        return kContinue;
    }
    if (sirius::SrsRenderer::ResizeRequested()) sirius::SrsRenderer::ResizeViewport();
    sirius::SrsRenderer::SpawnImguiWindow();
    sirius::SrsRenderer::Draw();
    return kContinue;
}
