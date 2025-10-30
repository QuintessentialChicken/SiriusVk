//
// Created by Leon on 17/01/2025.
//
#include <windows.h>

#include "Mouse.h"

namespace sirius {

int Mouse::GetX() noexcept {
    return x_;
}

int Mouse::GetY() noexcept {
    return y_;
}

bool Mouse::IsInWindow() noexcept {
    return isInWindow_;
}

bool Mouse::LeftIsPressed() noexcept {
    return leftIsPressed_;
}

bool Mouse::RightIsPressed() noexcept {
    return rightIsPressed_;
}

void Mouse::SetPos(int x, int y) noexcept {
    x_ = x;
    y_ = y;
}

void Mouse::SetIsInWindow(bool isInWindow) noexcept {
    isInWindow_ = isInWindow;
}

void Mouse::SetLeftIsPressed(bool leftIsPressed) noexcept {
    leftIsPressed_ = leftIsPressed;
}

void Mouse::SetRightIsPressed(bool rightIsPressed) noexcept {
    rightIsPressed_ = rightIsPressed;
}

void Mouse::SetWheelDelta(int wheelDelta) noexcept {
    while (wheelDeltaCarry_ >= WHEEL_DELTA) {
        wheelDeltaCarry_ -= WHEEL_DELTA;
    }
    while (wheelDeltaCarry_ <= -WHEEL_DELTA) {
        wheelDeltaCarry_ += WHEEL_DELTA;
    }
}

void Mouse::SetRawEnabled(bool rawEnabled) noexcept {
}
}
