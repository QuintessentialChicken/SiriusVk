//
// Created by Leon on 29/10/2025.
//
#pragma once
#include "window/window.h"

namespace sirius {
LRESULT InputWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class InputManager {
public:
    static void Init();
};
} // sirius
