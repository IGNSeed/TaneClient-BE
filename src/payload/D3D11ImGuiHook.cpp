#include <Windows.h>
#include <MinHook.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <GameInput.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <wincodec.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace tane::gui {
bool InstallItemHudHooks();
bool InstallEffectHudHooks();
void InitializeEffectHudImages();
void SetEffectHudPayloadModule(HMODULE module);
void RecordFpsFrame();
void RenderOverlay();
bool IsMenuOpen();
bool HandleMenuKeyMessage(UINT message, WPARAM wParam, LPARAM lParam);
bool IsControllerTogglePolling();
bool HandleTabKeyMessage(UINT message, WPARAM wParam, LPARAM lParam);
bool IsTabControllerComboPolling();
}

namespace tane::movement {
bool InstallAutoSprintHooks();
}

namespace tane::camera {
bool InstallZoomHooks();
bool HandleZoomKeyMessage(UINT message, WPARAM wParam, LPARAM lParam);
bool IsZoomControllerComboPolling();
bool InstallFreeLookHooks();
bool HandleFreeLookKeyMessage(UINT message, WPARAM wParam, LPARAM lParam);
bool IsFreeLookControllerComboPolling();
}

namespace tane::imgui_menu {
bool InstallInputBlockHooks();
void TickInputBlock(void* clientInstance);
}

namespace tane::payload {
namespace {

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(
    IDXGISwapChain* swapChain,
    UINT bufferCount,
    UINT width,
    UINT height,
    DXGI_FORMAT newFormat,
    UINT swapChainFlags);
using ExecuteCommandListsFn = void(STDMETHODCALLTYPE*)(
    ID3D12CommandQueue* commandQueue,
    UINT numCommandLists,
    ID3D12CommandList* const* commandLists);
using PeekMessageWFn = BOOL(WINAPI*)(LPMSG message, HWND hwnd, UINT messageFilterMin, UINT messageFilterMax, UINT removeMsg);
using GetMessageWFn = BOOL(WINAPI*)(LPMSG message, HWND hwnd, UINT messageFilterMin, UINT messageFilterMax);
using GetRawInputDataFn = UINT(WINAPI*)(HRAWINPUT rawInput, UINT command, LPVOID data, PUINT size, UINT headerSize);
using GetRawInputBufferFn = UINT(WINAPI*)(PRAWINPUT data, PUINT size, UINT headerSize);
using GetAsyncKeyStateFn = SHORT(WINAPI*)(int virtualKey);
using GetKeyStateFn = SHORT(WINAPI*)(int virtualKey);
using SetCursorPosFn = BOOL(WINAPI*)(int x, int y);
using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE module, LPCSTR procName);
using XInputGetStateRawFn = DWORD(WINAPI*)(DWORD userIndex, void* state);
using XInputGetCapabilitiesRawFn = DWORD(WINAPI*)(DWORD userIndex, DWORD flags, void* capabilities);
using GameInputCreateFn = HRESULT(WINAPI*)(IGameInput** gameInput);
using GameInputGetCurrentReadingFn = HRESULT(STDMETHODCALLTYPE*)(IGameInput*, GameInputKind, IGameInputDevice*, IGameInputReading**);
using GameInputGetNextReadingFn = HRESULT(STDMETHODCALLTYPE*)(IGameInput*, IGameInputReading*, GameInputKind, IGameInputDevice*, IGameInputReading**);
using GameInputGetPreviousReadingFn = HRESULT(STDMETHODCALLTYPE*)(IGameInput*, IGameInputReading*, GameInputKind, IGameInputDevice*, IGameInputReading**);
using GameInputGetTemporalReadingFn = HRESULT(STDMETHODCALLTYPE*)(IGameInput*, std::uint64_t, IGameInputDevice*, IGameInputReading**);
using GameInputReadingGetControllerAxisCountFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*);
using GameInputReadingGetControllerAxisStateFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*, std::uint32_t, float*);
using GameInputReadingGetControllerButtonCountFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*);
using GameInputReadingGetControllerButtonStateFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*, std::uint32_t, bool*);
using GameInputReadingGetControllerSwitchCountFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*);
using GameInputReadingGetControllerSwitchStateFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*, std::uint32_t, GameInputSwitchPosition*);
using GameInputReadingGetKeyCountFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*);
using GameInputReadingGetKeyStateFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*, std::uint32_t, GameInputKeyState*);
using GameInputReadingGetMouseStateFn = bool(STDMETHODCALLTYPE*)(IGameInputReading*, GameInputMouseState*);
using GameInputReadingGetTouchCountFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*);
using GameInputReadingGetTouchStateFn = std::uint32_t(STDMETHODCALLTYPE*)(IGameInputReading*, std::uint32_t, GameInputTouchState*);
using GameInputReadingGetMotionStateFn = bool(STDMETHODCALLTYPE*)(IGameInputReading*, GameInputMotionState*);
using GameInputReadingGetArcadeStickStateFn = bool(STDMETHODCALLTYPE*)(IGameInputReading*, GameInputArcadeStickState*);
using GameInputReadingGetFlightStickStateFn = bool(STDMETHODCALLTYPE*)(IGameInputReading*, GameInputFlightStickState*);
using GameInputReadingGetGamepadStateFn = bool(STDMETHODCALLTYPE*)(IGameInputReading*, GameInputGamepadState*);
using GameInputReadingGetRacingWheelStateFn = bool(STDMETHODCALLTYPE*)(IGameInputReading*, GameInputRacingWheelState*);
using GameInputReadingGetUiNavigationStateFn = bool(STDMETHODCALLTYPE*)(IGameInputReading*, GameInputUiNavigationState*);

PresentFn g_originalPresent = nullptr;
ResizeBuffersFn g_originalResizeBuffers = nullptr;
ExecuteCommandListsFn g_originalExecuteCommandLists = nullptr;
PeekMessageWFn g_originalPeekMessageW = nullptr;
GetMessageWFn g_originalGetMessageW = nullptr;
GetRawInputDataFn g_originalGetRawInputData = nullptr;
GetRawInputBufferFn g_originalGetRawInputBuffer = nullptr;
GetAsyncKeyStateFn g_originalGetAsyncKeyState = nullptr;
GetKeyStateFn g_originalGetKeyState = nullptr;
SetCursorPosFn g_originalSetCursorPos = nullptr;
GetProcAddressFn g_originalGetProcAddress = nullptr;
XInputGetStateRawFn g_originalXInputGetState = nullptr;
XInputGetCapabilitiesRawFn g_originalXInputGetCapabilities = nullptr;
GameInputCreateFn g_originalGameInputCreate = nullptr;
GameInputGetCurrentReadingFn g_originalGameInputGetCurrentReading = nullptr;
GameInputGetNextReadingFn g_originalGameInputGetNextReading = nullptr;
GameInputGetPreviousReadingFn g_originalGameInputGetPreviousReading = nullptr;
GameInputGetTemporalReadingFn g_originalGameInputGetTemporalReading = nullptr;
GameInputReadingGetControllerAxisCountFn g_originalReadingGetControllerAxisCount = nullptr;
GameInputReadingGetControllerAxisStateFn g_originalReadingGetControllerAxisState = nullptr;
GameInputReadingGetControllerButtonCountFn g_originalReadingGetControllerButtonCount = nullptr;
GameInputReadingGetControllerButtonStateFn g_originalReadingGetControllerButtonState = nullptr;
GameInputReadingGetControllerSwitchCountFn g_originalReadingGetControllerSwitchCount = nullptr;
GameInputReadingGetControllerSwitchStateFn g_originalReadingGetControllerSwitchState = nullptr;
GameInputReadingGetKeyCountFn g_originalReadingGetKeyCount = nullptr;
GameInputReadingGetKeyStateFn g_originalReadingGetKeyState = nullptr;
GameInputReadingGetMouseStateFn g_originalReadingGetMouseState = nullptr;
GameInputReadingGetTouchCountFn g_originalReadingGetTouchCount = nullptr;
GameInputReadingGetTouchStateFn g_originalReadingGetTouchState = nullptr;
GameInputReadingGetMotionStateFn g_originalReadingGetMotionState = nullptr;
GameInputReadingGetArcadeStickStateFn g_originalReadingGetArcadeStickState = nullptr;
GameInputReadingGetFlightStickStateFn g_originalReadingGetFlightStickState = nullptr;
GameInputReadingGetGamepadStateFn g_originalReadingGetGamepadState = nullptr;
GameInputReadingGetRacingWheelStateFn g_originalReadingGetRacingWheelState = nullptr;
GameInputReadingGetUiNavigationStateFn g_originalReadingGetUiNavigationState = nullptr;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11RenderTargetView* g_renderTarget = nullptr;
IDXGISwapChain* g_overlaySwapChain = nullptr;
ID3D11ShaderResourceView* g_menuLogoTextureView = nullptr;
ID3D12Resource* g_menuLogoDx12Texture = nullptr;
D3D12_CPU_DESCRIPTOR_HANDLE g_menuLogoDx12CpuHandle{};
D3D12_GPU_DESCRIPTOR_HANDLE g_menuLogoDx12GpuHandle{};
ImVec2 g_menuLogoTextureSize(0.0f, 0.0f);
bool g_menuLogoTextureAttempted = false;
constexpr int kItemHudPreviewTextureCount = 7;
ID3D11ShaderResourceView* g_itemHudPreviewTextureViews[kItemHudPreviewTextureCount] = {};
ID3D12Resource* g_itemHudPreviewDx12Textures[kItemHudPreviewTextureCount] = {};
D3D12_CPU_DESCRIPTOR_HANDLE g_itemHudPreviewDx12CpuHandles[kItemHudPreviewTextureCount] = {};
D3D12_GPU_DESCRIPTOR_HANDLE g_itemHudPreviewDx12GpuHandles[kItemHudPreviewTextureCount] = {};
ImVec2 g_itemHudPreviewTextureSizes[kItemHudPreviewTextureCount] = {};
bool g_itemHudPreviewTexturesAttempted = false;
constexpr int kExternalPngTextureCount = 256;
struct ExternalPngTexture {
    wchar_t path[MAX_PATH]{};
    ID3D11ShaderResourceView* d3d11View = nullptr;
    ID3D12Resource* d3d12Texture = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12CpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuHandle{};
    ImVec2 size{};
    bool attempted = false;
};
ExternalPngTexture g_externalPngTextures[kExternalPngTextureCount]{};
std::vector<std::uint8_t> g_menuRegularFontBytes;
std::vector<std::uint8_t> g_menuBoldFontBytes;
std::vector<std::uint8_t> g_itemHudFontBytes;
ImFont* g_menuRegularFont = nullptr;
ImFont* g_menuBoldFont = nullptr;
ImFont* g_itemHudFont = nullptr;

enum class RenderBackend {
    Unknown,
    D3D11,
    D3D12,
};

struct Dx12FrameContext {
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12Resource* renderTarget = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
    UINT64 fenceValue = 0;
};

RenderBackend g_backend = RenderBackend::Unknown;

ID3D12Device* g_d3d12Device = nullptr;
std::atomic<ID3D12CommandQueue*> g_d3d12CommandQueue = nullptr;
ID3D12DescriptorHeap* g_d3d12RtvHeap = nullptr;
ID3D12DescriptorHeap* g_d3d12SrvHeap = nullptr;
ID3D12GraphicsCommandList* g_d3d12CommandList = nullptr;
ID3D12Fence* g_d3d12Fence = nullptr;
HANDLE g_d3d12FenceEvent = nullptr;
UINT64 g_d3d12FenceValue = 0;
UINT g_d3d12BufferCount = 0;
UINT g_d3d12SrvDescriptorSize = 0;
Dx12FrameContext* g_d3d12Frames = nullptr;

std::atomic_bool g_imguiReady = false;
std::atomic_bool g_imguiInitializing = false;
std::atomic_bool g_hooked = false;
std::atomic_bool g_gameInputCreateHookInstalled = false;
std::atomic_bool g_gameInputMethodsHookInstalled = false;
std::atomic_bool g_gameInputReadingHooksInstalled = false;
std::atomic<std::uint32_t> g_cachedExtendedControllerFlags = 0;
std::atomic<ULONGLONG> g_lastExtendedControllerTick = 0;
HMODULE g_module = nullptr;
SRWLOCK g_imguiLock = SRWLOCK_INIT;

HWND g_gameWindow = nullptr;
WNDPROC g_originalWndProc = nullptr;
std::atomic_bool g_platformReady = false;
bool g_menuInputCaptured = false;
SRWLOCK g_pendingInputMessagesLock = SRWLOCK_INIT;
constexpr UINT kMaxPendingInputMessages = 128;
MSG g_pendingInputMessages[kMaxPendingInputMessages]{};
UINT g_pendingInputMessageCount = 0;
constexpr int kResourceMenuLogoPng = 201;
constexpr int kResourceMenuRegularFont = 202;
constexpr int kResourceMenuBoldFont = 203;
constexpr int kResourceItemHudFont = 204;
constexpr int kResourceItemHudPreviewBase = 4000;
constexpr std::uint32_t kExtendedControllerButtonShift = 16;
constexpr std::uint32_t kMaxExtendedControllerButtons = 8;
constexpr ULONGLONG kExtendedControllerFreshMs = 150;
constexpr UINT kDx12SrvDescriptorCount = 96;
constexpr UINT kMenuLogoSrvDescriptorIndex = 1;
constexpr UINT kItemHudPreviewSrvDescriptorStart = 2;
bool g_d3d12SrvDescriptorUsed[kDx12SrvDescriptorCount] = {};
constexpr int kFpsPresentCandidateCount = 8;
constexpr double kFpsPresentSelectionWindowSeconds = 0.35;
constexpr double kFpsPresentSwitchMargin = 1.08;
constexpr ULONGLONG kFpsPresentCandidateStaleMs = 1500;

struct FpsPresentCandidate {
    IDXGISwapChain* swapChain = nullptr;
    HWND window = nullptr;
    std::int64_t lastCounter = 0;
    double accumulatedSeconds = 0.0;
    int accumulatedFrames = 0;
    double measuredFps = 0.0;
    ULONGLONG lastSeenTick = 0;
};

SRWLOCK g_fpsPresentLock = SRWLOCK_INIT;
FpsPresentCandidate g_fpsPresentCandidates[kFpsPresentCandidateCount]{};
IDXGISwapChain* g_fpsPresentSourceSwapChain = nullptr;
std::int64_t g_fpsPresentCounterFrequency = 0;

HWND FindCurrentProcessWindow();
LRESULT CALLBACK HookWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void InstallGameInputHooks();
void InstallXInputHooks();
void InstallGameInputReadingHooks(IGameInputReading* reading);
void ClearPendingInputMessages();
DWORD WINAPI HookXInputGetState(DWORD userIndex, void* state);
DWORD WINAPI HookXInputGetCapabilities(DWORD userIndex, DWORD flags, void* capabilities);

class ImGuiExclusiveLock {
public:
    explicit ImGuiExclusiveLock(bool wait) {
        if (wait) {
            AcquireSRWLockExclusive(&g_imguiLock);
            locked_ = true;
        } else {
            locked_ = TryAcquireSRWLockExclusive(&g_imguiLock) != FALSE;
        }
    }

    ~ImGuiExclusiveLock() {
        if (locked_) {
            ReleaseSRWLockExclusive(&g_imguiLock);
        }
    }

    ImGuiExclusiveLock(const ImGuiExclusiveLock&) = delete;
    ImGuiExclusiveLock& operator=(const ImGuiExclusiveLock&) = delete;

    explicit operator bool() const {
        return locked_;
    }

private:
    bool locked_ = false;
};

#include "D3D11ImGuiResources.cpp"

#include "D3D11ImGuiInput.cpp"

#include "D3D11ImGuiBackend.cpp"

std::uint32_t GetExtendedControllerFlags() {
    const ULONGLONG lastTick = g_lastExtendedControllerTick.load(std::memory_order_relaxed);
    if (lastTick == 0 || GetTickCount64() - lastTick > kExtendedControllerFreshMs) {
        return 0;
    }
    return g_cachedExtendedControllerFlags.load(std::memory_order_relaxed);
}

ImTextureRef GetMenuLogoTexture() {
    if (EnsureMenuLogoTexture()) {
        if (g_backend == RenderBackend::D3D11 && g_menuLogoTextureView != nullptr) {
            return ImTextureRef(static_cast<void*>(g_menuLogoTextureView));
        }
        if (g_backend == RenderBackend::D3D12 && g_menuLogoDx12GpuHandle.ptr != 0) {
            return ImTextureRef(static_cast<ImTextureID>(g_menuLogoDx12GpuHandle.ptr));
        }
    }

    return ImTextureRef();
}

ImVec2 GetMenuLogoTextureSize() {
    return g_menuLogoTextureSize;
}

ImTextureRef GetItemHudPreviewTexture(int index) {
    if (index < 0 || index >= kItemHudPreviewTextureCount) {
        return ImTextureRef();
    }
    if (EnsureItemHudPreviewTextures()) {
        if (g_backend == RenderBackend::D3D11 && g_itemHudPreviewTextureViews[index] != nullptr) {
            return ImTextureRef(static_cast<void*>(g_itemHudPreviewTextureViews[index]));
        }
        if (g_backend == RenderBackend::D3D12 && g_itemHudPreviewDx12GpuHandles[index].ptr != 0) {
            return ImTextureRef(static_cast<ImTextureID>(g_itemHudPreviewDx12GpuHandles[index].ptr));
        }
    }

    return ImTextureRef();
}

ImVec2 GetItemHudPreviewTextureSize(int index) {
    if (index < 0 || index >= kItemHudPreviewTextureCount) {
        return ImVec2(0.0f, 0.0f);
    }

    return g_itemHudPreviewTextureSizes[index];
}

bool GetPngTextureFromFile(const wchar_t* path, ImTextureRef& texture, ImVec2& size) {
    texture = ImTextureRef();
    size = ImVec2(0.0f, 0.0f);

    int index = -1;
    if (!EnsureExternalPngTexture(path, index) || index < 0 || index >= kExternalPngTextureCount) {
        return false;
    }

    ExternalPngTexture& cached = g_externalPngTextures[index];
    if (g_backend == RenderBackend::D3D11 && cached.d3d11View != nullptr) {
        texture = ImTextureRef(static_cast<void*>(cached.d3d11View));
        size = cached.size;
        return true;
    }
    if (g_backend == RenderBackend::D3D12 && cached.d3d12GpuHandle.ptr != 0) {
        texture = ImTextureRef(static_cast<ImTextureID>(cached.d3d12GpuHandle.ptr));
        size = cached.size;
        return true;
    }

    return false;
}

bool GetRgbaTextureFromMemory(const char* key, const unsigned char* pixels, int width, int height, ImTextureRef& texture, ImVec2& size) {
    texture = ImTextureRef();
    size = ImVec2(0.0f, 0.0f);
    if (key == nullptr || key[0] == '\0' || pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    wchar_t wideKey[MAX_PATH]{};
    if (swprintf_s(wideKey, L"memory:%S", key) < 0) {
        return false;
    }

    const int index = FindExternalPngTextureSlot(wideKey);
    if (index < 0 || index >= kExternalPngTextureCount) {
        return false;
    }

    ExternalPngTexture& cached = g_externalPngTextures[index];
    if (cached.path[0] == L'\0') {
        wcsncpy_s(cached.path, wideKey, _TRUNCATE);
    }

    if (cached.d3d11View == nullptr && cached.d3d12Texture == nullptr && !cached.attempted) {
        cached.attempted = true;
        const std::size_t byteCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        std::vector<std::uint8_t> rgba(pixels, pixels + byteCount);
        if (g_backend == RenderBackend::D3D11) {
            CreateExternalPngD3D11Texture(index, rgba, static_cast<UINT>(width), static_cast<UINT>(height));
        } else if (g_backend == RenderBackend::D3D12) {
            CreateExternalPngD3D12Texture(index, rgba, static_cast<UINT>(width), static_cast<UINT>(height));
        }
    }

    if (g_backend == RenderBackend::D3D11 && cached.d3d11View != nullptr) {
        texture = ImTextureRef(static_cast<void*>(cached.d3d11View));
        size = cached.size;
        return true;
    }
    if (g_backend == RenderBackend::D3D12 && cached.d3d12GpuHandle.ptr != 0) {
        texture = ImTextureRef(static_cast<ImTextureID>(cached.d3d12GpuHandle.ptr));
        size = cached.size;
        return true;
    }
    return false;
}

ImFont* GetMenuRegularFont() {
    return g_menuRegularFont;
}

ImFont* GetMenuBoldFont() {
    return g_menuBoldFont;
}

ImFont* GetItemHudFont() {
    return g_itemHudFont;
}

bool InstallD3D11Hook(HMODULE module) {
    g_module = module;
    tane::gui::SetEffectHudPayloadModule(module);

    if (g_hooked.load()) {
        return true;
    }

    void* present = nullptr;
    void* resizeBuffers = nullptr;
    void* executeCommandLists = nullptr;
    if (!GetSwapChainMethods(&present, &resizeBuffers)) {
        return false;
    }
    if (GetCommandQueueMethods(&executeCommandLists)) {
    } else {
    }

    MH_STATUS initializeStatus = MH_Initialize();
    if (initializeStatus != MH_OK && initializeStatus != MH_ERROR_ALREADY_INITIALIZED) {
        return false;
    }

    InstallInputApiHooks();
    tane::gui::InitializeEffectHudImages();
    if (!tane::imgui_menu::InstallInputBlockHooks()) {
    }

    if (!tane::gui::InstallItemHudHooks()) {
    }
    if (!tane::gui::InstallEffectHudHooks()) {
    }
    if (!tane::movement::InstallAutoSprintHooks()) {
    }
    if (!tane::camera::InstallZoomHooks()) {
    }
    if (!tane::camera::InstallFreeLookHooks()) {
    }
    MH_STATUS createPresent = MH_CreateHook(present, &HookPresent, reinterpret_cast<void**>(&g_originalPresent));
    if (createPresent != MH_OK) {
        return false;
    }

    MH_STATUS createResize = MH_CreateHook(resizeBuffers, &HookResizeBuffers, reinterpret_cast<void**>(&g_originalResizeBuffers));
    if (createResize != MH_OK) {
        MH_RemoveHook(present);
        return false;
    }

    if (executeCommandLists != nullptr) {
        MH_STATUS createExecute = MH_CreateHook(
            executeCommandLists,
            &HookExecuteCommandLists,
            reinterpret_cast<void**>(&g_originalExecuteCommandLists));
        if (createExecute != MH_OK) {
        }
    }

    MH_STATUS enableExecute = executeCommandLists != nullptr ? MH_EnableHook(executeCommandLists) : MH_OK;
    MH_STATUS enablePresent = MH_EnableHook(present);
    MH_STATUS enableResize = MH_EnableHook(resizeBuffers);
    if (enableExecute != MH_OK || enablePresent != MH_OK || enableResize != MH_OK) {
        MH_RemoveHook(present);
        MH_RemoveHook(resizeBuffers);
        if (executeCommandLists != nullptr) {
            MH_RemoveHook(executeCommandLists);
        }
        return false;
    }

    g_hooked.store(true);
    return true;
}

void RemoveD3D11Hook() {
    g_imguiInitializing.store(false);

    if (g_hooked.exchange(false)) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(MH_ALL_HOOKS);
    }

    if (g_imguiReady.exchange(false)) {
        ImGuiExclusiveLock imguiLock(true);

        if (g_backend == RenderBackend::D3D11) {
            ImGui_ImplDX11_Shutdown();
        } else if (g_backend == RenderBackend::D3D12) {
            ImGui_ImplDX12_Shutdown();
        }
        if (g_platformReady.exchange(false)) {
            ImGui_ImplWin32_Shutdown();
        }
        RemoveWindowSubclass();
        ImGui::DestroyContext();
        ReleaseMenuFonts();
    }

    ReleaseRenderTarget();
    ReleaseMenuLogoTexture();
    ReleaseItemHudPreviewTextures();
    ReleaseExternalPngTextures();

    if (g_context != nullptr) {
        g_context->Release();
        g_context = nullptr;
    }
    if (g_device != nullptr) {
        g_device->Release();
        g_device = nullptr;
    }

    if (g_d3d12CommandList != nullptr) {
        g_d3d12CommandList->Release();
        g_d3d12CommandList = nullptr;
    }
    if (g_d3d12Frames != nullptr) {
        for (UINT i = 0; i < g_d3d12BufferCount; ++i) {
            if (g_d3d12Frames[i].commandAllocator != nullptr) {
                g_d3d12Frames[i].commandAllocator->Release();
            }
        }
        delete[] g_d3d12Frames;
        g_d3d12Frames = nullptr;
    }
    if (g_d3d12FenceEvent != nullptr) {
        CloseHandle(g_d3d12FenceEvent);
        g_d3d12FenceEvent = nullptr;
    }
    if (g_d3d12Fence != nullptr) {
        g_d3d12Fence->Release();
        g_d3d12Fence = nullptr;
    }
    if (g_d3d12SrvHeap != nullptr) {
        g_d3d12SrvHeap->Release();
        g_d3d12SrvHeap = nullptr;
    }
    g_d3d12SrvDescriptorSize = 0;
    std::memset(g_d3d12SrvDescriptorUsed, 0, sizeof(g_d3d12SrvDescriptorUsed));
    if (g_d3d12RtvHeap != nullptr) {
        g_d3d12RtvHeap->Release();
        g_d3d12RtvHeap = nullptr;
    }
    ID3D12CommandQueue* commandQueue = g_d3d12CommandQueue.exchange(nullptr);
    if (commandQueue != nullptr) {
        commandQueue->Release();
    }
    if (g_d3d12Device != nullptr) {
        g_d3d12Device->Release();
        g_d3d12Device = nullptr;
    }
    g_backend = RenderBackend::Unknown;
    AcquireSRWLockExclusive(&g_fpsPresentLock);
    std::memset(g_fpsPresentCandidates, 0, sizeof(g_fpsPresentCandidates));
    g_fpsPresentSourceSwapChain = nullptr;
    g_fpsPresentCounterFrequency = 0;
    ReleaseSRWLockExclusive(&g_fpsPresentLock);
    g_overlaySwapChain = nullptr;
    g_d3d12BufferCount = 0;
}

}  // namespace tane::payload
