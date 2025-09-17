//
// Created by Leon on 17/09/2025.
//

#ifndef SIRIUSVK_WINDOW_H
#define SIRIUSVK_WINDOW_H
#define WIN32_LEAN_AND_MEAN
#include <string>
#include <Windows.h>

namespace sirius {
    class gfxWindow {
    public:
        static HWND createDeviceWindow();
        static void setWindowTitle(const std::string& title);
        static void setBackgroundColor(float r, float g, float b, float a);
    private:
        static LRESULT handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept;
    };
}

#endif //SIRIUSVK_WINDOW_H