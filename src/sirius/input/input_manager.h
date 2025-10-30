//
// Created by Leon on 29/10/2025.
//
#pragma once
#include "Keyboard.h"
#include "mouse.h"
#include "window/window.h"

namespace sirius {

LRESULT CALLBACK InputWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class InputManager {
public:
    static InputManager& Get() {
        static InputManager instance;
        return instance;
    }
    static void Init();

    LRESULT CALLBACK ProcessMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
private:
    InputManager();

    int activeMouse_{0};
    int activeKeyboard_{0};
    std::vector<Mouse> mice_;
    std::vector<Keyboard> keyboards_;
};
} // sirius
