//
// Created by Leon on 17/09/2025.
//

#ifndef SIRIUSVK_WNDPROC_H
#define SIRIUSVK_WNDPROC_H

#define WIN32_LEAN_AND_MEAN
#include <string>
#include <Windows.h>
inline LRESULT (__stdcall * g_WindowProc)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) = DefWindowProc;

inline HWND hwndMain = nullptr;
inline HINSTANCE hInstance = nullptr;
inline unsigned winClass;

inline std::string windowTitle;
inline int windowWidth = 800;
inline int windowHeight = 600;

#endif //SIRIUSVK_WNDPROC_H