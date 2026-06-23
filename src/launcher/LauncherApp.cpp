#include <Windows.h>
#include <TlHelp32.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wincodec.h>
#include <windowsx.h>
#include <wrl/client.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace tane::launcher {

int RunLauncher(HINSTANCE instance, int showCommand);

}  // namespace tane::launcher

namespace {

using Microsoft::WRL::ComPtr;

constexpr wchar_t kWindowClass[] = L"TaneClientLauncherWindow";
constexpr wchar_t kTargetProcess[] = L"Minecraft.Windows.exe";
constexpr int kWindowWidth = 420;
constexpr int kWindowHeight = 360;
constexpr int kDragAreaHeight = 72;
constexpr float kCornerRadius = 28.0f;
constexpr int kResourcePayload = 101;
constexpr int kResourceLogoPng = 102;
constexpr int kResourceFont = 103;
constexpr int kResourceIcon = 104;

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_deviceContext;
ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_renderTargetView;
ComPtr<ID3D11ShaderResourceView> g_logoTextureView;

std::vector<std::uint8_t> g_fontBytes;
ImFont* g_launcherFont = nullptr;
ImFont* g_compactFont = nullptr;
ImFont* g_titleFont = nullptr;

std::mutex g_statusMutex;
std::wstring g_status = L"Ready";
std::atomic_bool g_injectButtonEnabled = true;
bool g_running = true;

#include "LauncherInjection.cpp"

#include "LauncherGraphics.cpp"

#include "LauncherUi.cpp"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCHITTEST) {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        if (point.y >= 0 &&
            point.y <= kDragAreaHeight &&
            !IsCloseButtonPoint(point)) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    if (ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
        return TRUE;
    }

    switch (message) {
    case WM_SIZE:
        if (g_device != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;

    case WM_CLOSE:
        g_running = false;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

int tane::launcher::RunLauncher(HINSTANCE instance, int showCommand) {
    g_instance = instance;
    ImGui_ImplWin32_EnableDpiAwareness();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(kResourceIcon));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hIcon = icon;
    wc.hIconSm = icon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&wc)) {
        CoUninitialize();
        return 1;
    }

    int x = (GetSystemMetrics(SM_CXSCREEN) - kWindowWidth) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - kWindowHeight) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClass,
        L"TaneClient",
        WS_POPUP,
        x,
        y,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        CoUninitialize();
        return 1;
    }

    g_mainWindow = hwnd;
    ApplyRoundedCorners(hwnd);

    if (!CreateDeviceD3D(hwnd)) {
        DestroyWindow(hwnd);
        CleanupDeviceD3D();
        CoUninitialize();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ApplyImGuiStyle();
    LoadLauncherFont();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device.Get(), g_deviceContext.Get());
    CreateLogoTextureFromResource();

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG message{};
    while (g_running) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (!g_running) {
            break;
        }

        RenderFrame();
    }

    g_logoTextureView.Reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
