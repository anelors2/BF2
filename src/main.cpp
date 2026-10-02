#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <tchar.h>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "settings.h"
#include "memory.h"
#include "game_memory.h"
#include "combat.h"
#include "overlay.h"
#include "prbf2.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND g_hTargetWnd = nullptr;
static int g_OverlayWidth = 1920;
static int g_OverlayHeight = 1080;

HWND FindGameWindow(const Memory& mem) {
    HWND h = mem.FindMainWindow();
    if (h && IsWindow(h)) return h;
    h = FindWindowW(nullptr, L"Project Reality: BF2");
    if (!h) h = FindWindowW(L"BF2", nullptr);
    if (!h) h = FindWindowW(nullptr, L"Battlefield 2");
    return h;
}

void ForceForegroundWindow(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return;
    HWND hForeground = GetForegroundWindow();
    if (hForeground == hWnd) return;

    DWORD targetThread = hForeground ? GetWindowThreadProcessId(hForeground, nullptr) : 0;
    DWORD currentThread = GetCurrentThreadId();

    if (targetThread && targetThread != currentThread) {
        AttachThreadInput(currentThread, targetThread, TRUE);
    }

    keybd_event(VK_MENU, 0, 0, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hWnd);
    SetForegroundWindow(hWnd);
    SetActiveWindow(hWnd);
    SetFocus(hWnd);

    if (targetThread && targetThread != currentThread) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
}

void ReturnForegroundToGame(HWND hGame) {
    if (!hGame || !IsWindow(hGame)) return;
    HWND hForeground = GetForegroundWindow();
    DWORD targetThread = GetWindowThreadProcessId(hGame, nullptr);
    DWORD currentThread = GetCurrentThreadId();

    if (targetThread && targetThread != currentThread) {
        AttachThreadInput(currentThread, targetThread, TRUE);
    }

    keybd_event(VK_MENU, 0, 0, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

    SetForegroundWindow(hGame);
    SetActiveWindow(hGame);
    SetFocus(hGame);

    if (targetThread && targetThread != currentThread) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
}

void SetWindowClickThrough(HWND hwnd, bool clickThrough) {
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (clickThrough) {
        exStyle |= (WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE);
    } else {
        exStyle &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        exStyle |= WS_EX_LAYERED;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}


void SyncOverlayWithGame(HWND hOverlay, HWND hGame) {
    if (!hGame || !IsWindow(hGame)) return;

    RECT rect;
    if (GetClientRect(hGame, &rect)) {
        POINT pt = { rect.left, rect.top };
        ClientToScreen(hGame, &pt);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        if (width > 0 && height > 0) {
            static int s_LastX = -1, s_LastY = -1, s_LastW = -1, s_LastH = -1;
            if (pt.x != s_LastX || pt.y != s_LastY || width != s_LastW || height != s_LastH) {
                s_LastX = pt.x;
                s_LastY = pt.y;
                s_LastW = width;
                s_LastH = height;
                g_OverlayWidth = width;
                g_OverlayHeight = height;
                SetWindowPos(hOverlay, HWND_TOPMOST, pt.x, pt.y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"PRBF2_ImGui_Overlay", nullptr };
    ::RegisterClassExW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    g_OverlayWidth = screenWidth;
    g_OverlayHeight = screenHeight;

    DWORD dwStyle = WS_POPUP;
    DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_LAYERED;
    if (!g_Settings.menuOpen) {
        dwExStyle |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }

    HWND hwnd = ::CreateWindowExW(
        dwExStyle,
        wc.lpszClassName,
        L"PRBF2 External Toolkit",
        dwStyle,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    SetWindowClickThrough(hwnd, !g_Settings.menuOpen);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ApplyModernTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    g_Settings.LoadConfig();

    PRBF2Interface ticketsManager;
    Memory& mem = ticketsManager.mem;

    HANDLE hCombatThread = CreateThread(nullptr, 0, AntiRecoilWorker, nullptr, 0, nullptr);
    HANDLE hReaderThread = CreateThread(nullptr, 0, MemoryReaderWorker, &ticketsManager, 0, nullptr);

    DWORD lastTrackTick = 0;
    bool done = false;

    while (!done && !g_Settings.shouldQuit)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done || g_Settings.shouldQuit)
            break;

        static bool insertWasDown = false;
        SHORT insKey = GetAsyncKeyState(VK_INSERT);
        bool insDown = (insKey & 0x8000) != 0;
        if (insDown && !insertWasDown) {
            g_Settings.menuOpen = !g_Settings.menuOpen;
        }
        insertWasDown = insDown;

        static bool f11WasDown = false;
        SHORT f11Key = GetAsyncKeyState(VK_F11);
        bool f11Down = (f11Key & 0x8000) != 0;
        if (f11Down && !f11WasDown) {
            g_Settings.menuOpen = !g_Settings.menuOpen;
        }
        f11WasDown = f11Down;

        static bool s_GameEverDetected = false;
        static HANDLE s_hGameProcess = nullptr;

        if (!s_hGameProcess) {
            DWORD pid = mem.processId;
            if (pid == 0 && g_hTargetWnd && IsWindow(g_hTargetWnd)) {
                GetWindowThreadProcessId(g_hTargetWnd, &pid);
            }
            if (pid != 0) {
                s_hGameProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
                if (s_hGameProcess) {
                    s_GameEverDetected = true;
                }
            }
        }

        if (s_GameEverDetected) {
            bool gameExited = false;
            if (s_hGameProcess) {
                if (WaitForSingleObject(s_hGameProcess, 0) == WAIT_OBJECT_0) {
                    gameExited = true;
                }
            }
            if (g_hTargetWnd && !IsWindow(g_hTargetWnd)) {
                gameExited = true;
            }

            if (gameExited) {
                if (s_hGameProcess) {
                    CloseHandle(s_hGameProcess);
                    s_hGameProcess = nullptr;
                }
                g_Settings.shouldQuit = true;
                done = true;
                break;
            }
        }

        if (GetAsyncKeyState(VK_END) & 0x8000) {
            g_Settings.shouldQuit = true;
            done = true;
            break;
        }

        static bool s_LastMenuOpen = g_Settings.menuOpen;
        if (g_Settings.menuOpen != s_LastMenuOpen) {
            s_LastMenuOpen = g_Settings.menuOpen;
            SetWindowClickThrough(hwnd, !g_Settings.menuOpen);
            if (g_Settings.menuOpen) {
                ForceForegroundWindow(hwnd);
            } else {
                if (g_hTargetWnd && IsWindow(g_hTargetWnd)) {
                    ReturnForegroundToGame(g_hTargetWnd);
                }
            }
        }

        DWORD now = GetTickCount();
        if (now - lastTrackTick > 250) {
            lastTrackTick = now;
            if (!g_hTargetWnd || !IsWindow(g_hTargetWnd)) {
                g_hTargetWnd = FindGameWindow(mem);
                g_hGameWnd = g_hTargetWnd;
            }
            if (g_hTargetWnd && IsWindow(g_hTargetWnd)) {
                SyncOverlayWithGame(hwnd, g_hTargetWnd);
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        if (g_Settings.menuOpen) {
            POINT pt;
            if (GetCursorPos(&pt)) {
                ScreenToClient(hwnd, &pt);
                io.AddMousePosEvent((float)pt.x, (float)pt.y);
            }
            io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
            io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        }

        ImGui::NewFrame();

        io.DisplaySize = ImVec2((float)g_OverlayWidth, (float)g_OverlayHeight);
        io.MouseDrawCursor = g_Settings.menuOpen;

        UpdateFrameDataSnapshot();

        RenderWatermark();

        RenderVisualESP(g_OverlayWidth, g_OverlayHeight);

        ImDrawList* bgDraw = ImGui::GetBackgroundDrawList();
        float cx = (float)g_OverlayWidth * 0.5f;
        float cy = (float)g_OverlayHeight * 0.5f;

        if (g_Settings.espCrosshair) {
            float s = 8.0f;
            ImU32 c = IM_COL32(0, 255, 128, 220);
            bgDraw->AddLine(ImVec2(cx - s, cy), ImVec2(cx + s, cy), c, 1.5f);
            bgDraw->AddLine(ImVec2(cx, cy - s), ImVec2(cx, cy + s), c, 1.5f);
            bgDraw->AddCircle(ImVec2(cx, cy), 2.0f, IM_COL32(255, 255, 255, 255));
        }

        if (g_Settings.espFOVRadius) {
            bgDraw->AddCircle(ImVec2(cx, cy), g_Settings.espAimbotFov, IM_COL32(255, 255, 255, 100), 64, 1.2f);
        }

        RenderRadar();
        RenderAmmoHud(g_OverlayWidth, g_OverlayHeight);

        RenderMenu();

        ImGui::Render();
        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    g_Running.store(false);
    if (hReaderThread) {
        WaitForSingleObject(hReaderThread, 500);
        CloseHandle(hReaderThread);
    }
    if (hCombatThread) {
        WaitForSingleObject(hCombatThread, 500);
        CloseHandle(hCombatThread);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    ExitProcess(0);
    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCHITTEST)
    {
        if (!g_Settings.menuOpen)
            return HTTRANSPARENT;

        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (ctx)
        {
            ImGuiIO& io = ImGui::GetIO();
            if (!io.WantCaptureMouse)
                return HTTRANSPARENT;
        }
        return HTCLIENT;
    }

    if (msg == WM_MOUSEACTIVATE)
    {
        return MA_ACTIVATE;
    }

    if (g_Settings.menuOpen)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
    }

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
