//
// Created by Leon on 29/10/2025.
//
#pragma once
#include <functional>
#include <variant>

#include "Keyboard.h"
#include "mouse.h"
#include "window/window.h"

namespace sirius {
LRESULT CALLBACK InputWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct KeyEvent {
    unsigned char key;
    bool pressed;
};

struct MouseMoveEvent {
    int x;
    int y;
};

struct MouseWheelEvent {
    float delta;
};

struct InputEvent {
    enum class Type {
        kKeyDown,
        kKeyUp,
        kMouseMove,
        kMouseDown,
        kMouseUp,
        kMouseWheel
    };
    Type type;
    std::variant<KeyEvent, MouseMoveEvent, MouseWheelEvent> data;
};

class InputManager {
public:
    static InputManager& Get() {
        static InputManager instance;
        return instance;
    }

    static void Init();

    static void Subscribe(const std::function<void(const InputEvent&)>& callback);

    LRESULT CALLBACK ProcessMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static std::pair<float, float> GetMouseCoords();

private:
    InputManager();

    Mouse GetActiveMouse();

    int activeMouse_{0};
    int activeKeyboard_{0};
    std::vector<Mouse> mice_;
    std::vector<Keyboard> keyboards_;

    std::vector<std::function<void(const InputEvent&)> > callbacks_;
};
} // sirius
