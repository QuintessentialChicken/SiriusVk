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

LRESULT CALLBACK InputManager::ProcessMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        /*********** KEYBOARD MESSAGES ***********/
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            Keyboard::OnKeyPressed(wParam);
            break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
            Keyboard::OnKeyReleased(wParam);
            break;
        case WM_CHAR:
            Keyboard::OnChar(static_cast<char>(wParam));
            break;
        /*********** END KEYBOARD MESSAGES ***********/
        /************** MOUSE MESSAGES ***************/
        case WM_MOUSEMOVE: {
            if (const auto [x, y] = MAKEPOINTS(lParam); x >= 0 && x < windowWidth && y >= 0 && y < windowHeight) {
                mice_.at(activeMouse_).SetPos(x, y);
                if (!mice_.at(activeMouse_).IsInWindow()) {
                    SetCapture(hWnd);
                    mice_.at(activeMouse_).SetIsInWindow(true);
                }
            }
            // not in client -> log move / maintain capture if button down
            else {
                if (wParam & (MK_LBUTTON | MK_RBUTTON)) {
                    mice_.at(activeMouse_).SetPos(x, y);
                }
                // button up -> release capture / log event for leaving
                else {
                    ReleaseCapture();
                    mice_.at(activeMouse_).SetIsInWindow(false);
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            mice_.at(activeMouse_).SetLeftIsPressed(true);
            break;
        }
        case WM_LBUTTONUP: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            mice_.at(activeMouse_).SetLeftIsPressed(false);
            break;
        }
        case WM_RBUTTONDOWN: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            mice_.at(activeMouse_).SetRightIsPressed(true);
            break;
        }
        case WM_RBUTTONUP: {
            const auto [x, y]{MAKEPOINTS(lParam)};
            mice_.at(activeMouse_).SetRightIsPressed(false);
            break;
        }
        case WM_MOUSEWHEEL: {
            const auto [x, y] = MAKEPOINTS(lParam);
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            mice_.at(activeMouse_).SetWheelDelta(delta);
            break;
        }
        /************ END MOUSE MESSAGES *************/
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

InputManager::InputManager() {
    mice_.emplace_back();
    keyboards_.emplace_back();
}
} // sirius
