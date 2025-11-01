//
// Created by Leon on 29/10/2025.
//

#include "input_manager.h"

#include "Keyboard.h"
#include "mouse.h"
#include "graphics/renderer.h"
#include "window/wndProc.h"

namespace sirius {
LRESULT CALLBACK InputWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) { return InputManager::Get().ProcessMessage(hWnd, msg, wParam, lParam); }

void InputManager::Init() {
    windowProc = InputWindowProc;
}

std::pair<float, float> InputManager::GetMouseCoords() {
    auto mouse = Get().GetActiveMouse();
    return std::make_pair(mouse.GetX(), mouse.GetY());
}

void InputManager::Subscribe(const std::function<void(const InputEvent&)>& callback) {
    Get().callbacks_.push_back(callback);
}

LRESULT CALLBACK InputManager::ProcessMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    InputEvent event;
    switch (msg) {
        /*********** KEYBOARD MESSAGES ***********/
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            event.type = InputEvent::Type::kKeyDown;
            event.data = KeyEvent{static_cast<unsigned char>(wParam), true};
            break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
            event.type = InputEvent::Type::kKeyUp;
            event.data = KeyEvent{static_cast<unsigned char>(wParam), false};
            break;
        case WM_CHAR:

            break;
        /*********** END KEYBOARD MESSAGES ***********/
        /************** MOUSE MESSAGES ***************/
        case WM_INPUT: {
            UINT size;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
            LPBYTE buffer = new BYTE[size];

            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size)
            {
                RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
                if (raw->header.dwType == RIM_TYPEMOUSE) {
                    LONG dx = raw->data.mouse.lLastX;
                    LONG dy = -raw->data.mouse.lLastY;
                    event.type = InputEvent::Type::kMouseMove;
                    event.data = MouseMoveEvent(dx, dy);
                }
            }
            break;
        }
        case WM_MOUSEMOVE: {
            // if (const auto [x, y] = MAKEPOINTS(lParam); x >= 0 && x < windowWidth && y >= 0 && y < windowHeight) {
            //     GetActiveMouse().SetPos(x, y);
            //     if (!GetActiveMouse().IsInWindow()) {
            //         SetCapture(hWnd);
            //         GetActiveMouse().SetIsInWindow(true);
            //     }
            //     event.type = InputEvent::Type::kMouseMove;
            //     event.data = MouseMoveEvent{x, y};
            // }
            // // not in client -> log move / maintain capture if button down
            // else {
            //     if (wParam & (MK_LBUTTON | MK_RBUTTON)) {
            //         GetActiveMouse().SetPos(x, y);
            //     }
            //     // button up -> release capture / log event for leaving
            //     else {
            //         ReleaseCapture();
            //         GetActiveMouse().SetIsInWindow(false);
            //     }
            // }
            break;
        }
        case WM_LBUTTONDOWN: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            GetActiveMouse().SetLeftIsPressed(true);
            break;
        }
        case WM_LBUTTONUP: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            GetActiveMouse().SetLeftIsPressed(false);
            break;
        }
        case WM_RBUTTONDOWN: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            GetActiveMouse().SetRightIsPressed(true);
            break;
        }
        case WM_RBUTTONUP: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            GetActiveMouse().SetRightIsPressed(false);
            break;
        }
        case WM_MOUSEWHEEL: {
            const auto [x, y] = MAKEPOINTS(lParam);
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            GetActiveMouse().SetWheelDelta(delta);
            break;
        }
        /************ END MOUSE MESSAGES *************/
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    for (auto& callback : callbacks_) {
        callback(event);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

InputManager::InputManager() {
    mice_.emplace_back();
    keyboards_.emplace_back();
}

Mouse InputManager::GetActiveMouse() {
    return mice_.at(activeMouse_);
}
} // sirius
