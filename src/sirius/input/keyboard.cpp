#include "Keyboard.h"

bool Keyboard::autorepeatEnabled = false;
std::bitset<256> Keyboard::keyStates;
std::queue<char> Keyboard::charBuffer;

bool Keyboard::IsKeyPressed(unsigned char keycode) noexcept {
    return keyStates[keycode];
}

char Keyboard::ReadChar() noexcept {
    if (charBuffer.empty()) {
        return {};
    }
    const char charcode = charBuffer.front();
    charBuffer.pop();
    return charcode;
}

bool Keyboard::CharIsEmpty() noexcept {
    return charBuffer.empty();
}

void Keyboard::ClearChar() noexcept {
    charBuffer = std::queue<char>();
}

void Keyboard::Clear() noexcept {
    ClearChar();
}

void Keyboard::EnableAutorepeat() noexcept {
    autorepeatEnabled = true;
}

void Keyboard::DisableAutorepeat() noexcept {
    autorepeatEnabled = false;
}

bool Keyboard::IsAutorepeatEnabled() noexcept {
    return autorepeatEnabled;
}

void Keyboard::OnKeyPressed(unsigned char keycode) noexcept {
    keyStates[keycode] = true;
}

void Keyboard::OnKeyReleased(unsigned char keycode) noexcept {
    keyStates[keycode] = false;
}

void Keyboard::ClearState() noexcept {
    keyStates.reset();
}

void Keyboard::OnChar(char character) noexcept {
    charBuffer.emplace(character);
}