//
// Created by Leon on 17/09/2025.
//

#include "window.h"

#include <iostream>
#include <ostream>

#include "wndProc.h"
#include "resources.h"
#include <vulkan/vulkan_core.h>

namespace sirius {
HWND gfxWindow::createDeviceWindow() {
    if (!winClass) {
        hInstance = GetModuleHandle(nullptr);
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = handleMessages;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0));
        wc.hCursor = nullptr;
        wc.hbrBackground = nullptr;
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = "GfxWindow";
        wc.hIconSm = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_HAND), IMAGE_ICON, 16, 16, 0));;

        RegisterClassEx(&wc);
    }
    RECT wr;
    wr.left = 100;
    wr.right = wr.left + windowWidth;
    wr.top = 100;
    wr.bottom = wr.top + windowHeight;

    if (AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE) == 0) {
        return nullptr;
    }

    HWND result = CreateWindow(
        "GfxWindow",
        windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(result, SW_SHOWDEFAULT);

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::cout << extensionCount << " extensions supported" << std::endl;

    return result;
}

LRESULT gfxWindow::handleMessages(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
    switch (msg) {
        case WM_CLOSE:
            PostQuitMessage(EXIT_SUCCESS);
            break;
        case WM_KILLFOCUS:
            // kbd.ClearState();
            break;
        case WM_SIZE:
            windowWidth = LOWORD(lParam);
            windowHeight = HIWORD(lParam);
        default: {
        }
    }
    return g_WindowProc(hWnd, msg, wParam, lParam);
}

void gfxWindow::setWindowTitle(const std::string& title) {
}

void gfxWindow::setBackgroundColor(float r, float g, float b, float a) {
}
}
