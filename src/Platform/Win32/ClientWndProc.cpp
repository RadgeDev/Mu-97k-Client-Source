// WndProc.cpp — procedimiento de la ventana principal.
//
// Despacha los mensajes de la ventana; la red y el input viven en sus módulos:
//   gNetwork.OnSocketEvent      (Net/Network.cpp)  — WM_USER
//   Input_OnWindowMessage  (Input/Input_WndProc.cpp)   — mouse, IME y WM_CHAR

#include "stdafx.h"
#include "Net/Ping.h"
#include "Game/FrameLimiter.h"
#include "Platform/Win32/Win32Window.h"
#include "Input/Input.h"

void OpenGL_Release(void);        // WinMain.cpp
void GameGuard_TickCheck(void);   // WinMain.cpp
void Input_OnWindowMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        gPing.Reset();
        gFrameLimiter.Shutdown();
        DAT_055ca018 = 1;                                   // Destroy
        CWsctlc_Close((int)(uintptr_t)SocketClient);
        for (int buffer = 0; buffer < 420; ++buffer)
            Sound_ReleaseBuffer(buffer);
        OpenGL_Release();
        PostQuitMessage(0);
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_ACTIVATE:
    {
        const WORD activeState = LOWORD(wParam);
        const bool active = (activeState != WA_INACTIVE);
        const BOOL isMinimized = (BOOL)HIWORD(wParam);

        Platform::GetWin32Window().SetActive(active);
        Platform::GetWin32Window().SetMinimized(isMinimized != FALSE);

        if (!active || isMinimized) {
            Input_ResetAllStates();
            if (isMinimized && Platform::GetWin32Window().IsFullscreen()) {
                Platform::GetWin32Window().OnFullscreenMinimize();
            }
        } else {
            Input_ResetAllStates();
            if (Platform::GetWin32Window().IsFullscreen()) {
                Platform::GetWin32Window().OnFullscreenRestore();
            }
        }
        break;
    }

    case WM_ACTIVATEAPP:
    {
        const bool active = (wParam != FALSE);
        Platform::GetWin32Window().SetActive(active);
        if (!active) {
            Input_ResetAllStates();
        } else {
            Input_ResetAllStates();
        }
        break;
    }

    case WM_SETFOCUS:
        Platform::GetWin32Window().SetActive(true);
        Input_ResetAllStates();
        break;

    case WM_KILLFOCUS:
        Platform::GetWin32Window().SetActive(false);
        Input_ResetAllStates();
        break;

    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED) {
            Platform::GetWin32Window().SetMinimized(true);
            Platform::GetWin32Window().SetActive(false);
            Input_ResetAllStates();
            if (Platform::GetWin32Window().IsFullscreen()) {
                Platform::GetWin32Window().OnFullscreenMinimize();
            }
        } else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
            const bool wasMinimized = Platform::GetWin32Window().IsMinimized();
            Platform::GetWin32Window().SetMinimized(false);
            Platform::GetWin32Window().SetActive(true);
            Input_ResetAllStates();
            if (wasMinimized && Platform::GetWin32Window().IsFullscreen()) {
                Platform::GetWin32Window().OnFullscreenRestore();
            }
        }
        break;
    }

    case WM_SYSCOMMAND:
    {
        const WPARAM cmd = wParam & 0xFFF0;
        if (cmd == SC_MINIMIZE) {
            Input_ResetAllStates();
            Platform::GetWin32Window().SetMinimized(true);
            Platform::GetWin32Window().SetActive(false);
            if (Platform::GetWin32Window().IsFullscreen()) {
                Platform::GetWin32Window().OnFullscreenMinimize();
            }
        } else if (cmd == SC_RESTORE) {
            Input_ResetAllStates();
            Platform::GetWin32Window().SetMinimized(false);
            Platform::GetWin32Window().SetActive(true);
            if (Platform::GetWin32Window().IsFullscreen()) {
                Platform::GetWin32Window().OnFullscreenRestore();
            }
        }
        break;
    }

    case WM_SETCURSOR:
        if (Platform::GetWin32Window().IsActive()) {
            SetCursor(NULL);
            return TRUE;
        }
        break;

    case WM_TIMER:
        if (wParam == 1000)
            GameGuard_TickCheck();
        break;

    case WM_USER:          // 0x400 — eventos de WSAAsyncSelect
        gNetwork.OnSocketEvent(LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_USER + 1:      // 0x401 — shutdown
        OpenGL_Release();
        break;

    default:
        // Mouse, IME y WM_CHAR.
        Input_OnWindowMessage(uMsg, wParam, lParam);
        break;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}
