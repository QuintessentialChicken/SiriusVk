#pragma once
#include <queue>
#include <bitset>

class Keyboard {
public:
    [[nodiscard]] static bool IsKeyPressed(unsigned char keycode) noexcept;

    // char event stuff
    static char ReadChar() noexcept;

    [[nodiscard]] static bool CharIsEmpty() noexcept;

    static void ClearChar() noexcept;

    static void Clear() noexcept;

    // autorepeat control
    static void EnableAutorepeat() noexcept;

    static void DisableAutorepeat() noexcept;

    [[nodiscard]] static bool IsAutorepeatEnabled() noexcept;

    static void OnKeyPressed(unsigned char keycode) noexcept;

    static void OnKeyReleased(unsigned char keycode) noexcept;

    static void OnChar(char character) noexcept;

    static void ClearState() noexcept;

private:
    static bool autorepeatEnabled;
    static constexpr unsigned int nKeys = 256u;
    static constexpr unsigned int bufferSize = 16u;
    static std::bitset<nKeys> keyStates;
    static std::queue<char> charBuffer;
};
