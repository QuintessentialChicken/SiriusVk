//
// Created by Leon on 29/10/2025.
//

#include "input_manager.h"

#include <iostream>

#include "graphics/renderer.h"
#include "window/wndProc.h"

namespace sirius {
void InputManager::Init() {
    windowProc = InputWindowProc;
}

LRESULT CALLBACK InputWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    std::pair<float, float> keyInput = std::make_pair(0.0f, 0.0f);
    std::pair<float, float> mouseInput = std::make_pair(0.0f, 0.0f);
    switch (msg) {
        case WM_KEYDOWN: {
            switch (wParam) {
                case 'W':
                    keyInput.first = -1.0f;
                    break;
                case 'S':
                    keyInput.first = 1.0f;
                    break;
                case 'A':
                    keyInput.second = -1.0f;
                    break;
                case 'D':
                    keyInput.second = 1.0f;
                    break;
            }
            SrsRenderer::UpdateCamera(keyInput, mouseInput);
            break;
        }
        case WM_KEYUP: {
            keyInput = std::make_pair(0.0f, 0.0f);
        }
        case WM_MOUSEMOVE: {
        }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
} // sirius
