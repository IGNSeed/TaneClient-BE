#include <Windows.h>
#include <MinHook.h>

#include "Offsets.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace tane::gui {
bool IsMenuOpen();
bool CanRunGameplayModules();
}

namespace tane::payload {
std::uint32_t GetExtendedControllerFlags();
}

namespace tane::camera {
namespace {

struct XInputGamepadState {
    WORD buttons = 0;
    BYTE leftTrigger = 0;
    BYTE rightTrigger = 0;
    SHORT leftThumbX = 0;
    SHORT leftThumbY = 0;
    SHORT rightThumbX = 0;
    SHORT rightThumbY = 0;
};

struct XInputState {
    DWORD packetNumber = 0;
    XInputGamepadState gamepad{};
};

using XInputGetStateFn = DWORD(WINAPI*)(DWORD userIndex, XInputState* state);
using GetLocalPlayerFn = void*(__fastcall*)(void* clientInstance);
using UpdatePlayerFromCameraFn = void(__fastcall*)(void* cameraComponent, void* updateContext, void* player);
using OptionsGetPerspectiveFn = int(__fastcall*)(void* options);

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct ActorAngles {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float prevPitch = 0.0f;
    float prevYaw = 0.0f;
};

constexpr WORD kXInputGamepadLeftShoulder = 0x0100;
constexpr WORD kXInputGamepadRightShoulder = 0x0200;
constexpr WORD kXInputGamepadLeftThumb = 0x0040;
constexpr WORD kXInputGamepadRightThumb = 0x0080;
constexpr BYTE kXInputTriggerThreshold = 30;
constexpr std::uint32_t kControllerDpadUp = 1u << 0;
constexpr std::uint32_t kControllerDpadDown = 1u << 1;
constexpr std::uint32_t kControllerDpadLeft = 1u << 2;
constexpr std::uint32_t kControllerDpadRight = 1u << 3;
constexpr std::uint32_t kControllerStart = 1u << 4;
constexpr std::uint32_t kControllerBack = 1u << 5;
constexpr std::uint32_t kControllerLeftThumb = 1u << 6;
constexpr std::uint32_t kControllerRightThumb = 1u << 7;
constexpr std::uint32_t kControllerLeftShoulder = 1u << 8;
constexpr std::uint32_t kControllerRightShoulder = 1u << 9;
constexpr std::uint32_t kControllerA = 1u << 10;
constexpr std::uint32_t kControllerB = 1u << 11;
constexpr std::uint32_t kControllerX = 1u << 12;
constexpr std::uint32_t kControllerY = 1u << 13;
constexpr std::uint32_t kControllerLeftTrigger = 1u << 14;
constexpr std::uint32_t kControllerRightTrigger = 1u << 15;
constexpr std::uint32_t kExtendedControllerButtonShift = 16;
constexpr std::uint32_t kExtendedControllerButtonCount = 8;
constexpr int kFreeLookConfigVersion = 2;
constexpr int kVirtualKeyCount = 256;
constexpr int kDefaultKeyboardKeyA = 'F';
constexpr int kDefaultKeyboardKeyB = 0;
constexpr std::uint32_t kDefaultControllerMask = kControllerY;
constexpr std::uint32_t kLegacyDefaultControllerMask = kControllerRightThumb;
constexpr ULONGLONG kControllerPollIntervalMs = 16;
constexpr ULONGLONG kDisconnectedXInputPollIntervalMs = 300;
constexpr int kPerspectiveFirstPerson = 0;
constexpr int kPerspectiveThirdPersonBack = 1;
constexpr float kDegreesToRadians = 0.017453292519943295769f;

std::atomic_bool g_freeLookEnabled = false;
std::atomic_bool g_configLoaded = false;
std::atomic_bool g_freeLookHeld = false;
std::atomic_bool g_freeLookActive = false;
std::atomic<int> g_keyboardKeyA = kDefaultKeyboardKeyA;
std::atomic<int> g_keyboardKeyB = kDefaultKeyboardKeyB;
std::atomic<std::uint32_t> g_controllerMask = kDefaultControllerMask;
std::atomic<ULONGLONG> g_lastControllerReadTick = 0;
std::atomic<ULONGLONG> g_lastXInputPollTick = 0;
std::atomic<std::uint32_t> g_cachedControllerFlags = 0;
std::atomic<std::uint32_t> g_cachedXInputControllerFlags = 0;
std::atomic_bool g_xInputControllerPresent = false;

bool g_keyboardCaptureActive = false;
bool g_keyboardCaptureWaitingForRelease = false;
bool g_keyboardCaptureCandidateKeys[kVirtualKeyCount] = {};
bool g_keyboardCaptureDownKeys[kVirtualKeyCount] = {};
bool g_controllerCaptureActive = false;
bool g_controllerCaptureWaitingForRelease = false;
std::uint32_t g_controllerCaptureCandidateMask = 0;
char g_keyboardComboLabel[96] = {};
char g_controllerComboLabel[128] = {};
bool g_triedResolveXInput = false;
XInputGetStateFn g_xInputGetState = nullptr;
std::atomic_bool g_hooksInstalled = false;
std::atomic_bool g_hasSavedAngles = false;
std::atomic<float> g_savedPitch = 0.0f;
std::atomic<float> g_savedYaw = 0.0f;
std::atomic<float> g_savedPrevPitch = 0.0f;
std::atomic<float> g_savedPrevYaw = 0.0f;
std::atomic<float> g_frozenLookX = 0.0f;
std::atomic<float> g_frozenLookY = 0.0f;
std::atomic<float> g_frozenLookZ = 0.0f;
std::atomic<float> g_frozenLookW = 1.0f;
std::atomic_bool g_hasLiveCameraLookAngles = false;
std::atomic<float> g_liveCameraLookX = 0.0f;
std::atomic<float> g_liveCameraLookY = 0.0f;
std::atomic<float> g_liveCameraLookZ = 0.0f;
std::atomic<float> g_liveCameraLookW = 1.0f;
std::atomic<void*> g_liveCameraComponent = nullptr;
std::atomic<void*> g_savedRotationComponent = nullptr;
void* g_cachedClientInstanceVtable = nullptr;
void* g_cachedLocalPlayerFunction = nullptr;
UpdatePlayerFromCameraFn g_originalUpdatePlayerFromCamera = nullptr;
OptionsGetPerspectiveFn g_originalOptionsGetPerspective = nullptr;

bool IsReadableAddress(const void* address, std::size_t size) {
    if (address == nullptr || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0 ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    const auto regionStart = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto regionEnd = regionStart + info.RegionSize;
    return end >= start && end <= regionEnd;
}

bool IsExecutableAddress(void* address) {
    if (!IsReadableAddress(address, 1)) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    VirtualQuery(address, &info, sizeof(info));
    const DWORD protect = info.Protect & 0xFF;
    return protect == PAGE_EXECUTE ||
        protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE ||
        protect == PAGE_EXECUTE_WRITECOPY;
}

HMODULE GetMinecraftModule() {
    HMODULE module = GetModuleHandleW(L"Minecraft.Windows.exe");
    return module != nullptr ? module : GetModuleHandleW(nullptr);
}

void* GetImageAddress(std::uintptr_t rva) {
    HMODULE module = GetMinecraftModule();
    if (module == nullptr || rva == 0) {
        return nullptr;
    }
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module) + rva);
}

template <typename T>
bool ReadValue(const void* address, T& value) {
    if (address == nullptr) {
        return false;
    }

    __try {
        value = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool ReadOffset(const void* base, std::size_t offset, T& value) {
    if (base == nullptr) {
        return false;
    }
    return ReadValue(reinterpret_cast<const std::uint8_t*>(base) + offset, value);
}

template <typename T>
bool WriteValue(void* address, const T& value) {
    if (address == nullptr) {
        return false;
    }

    __try {
        *reinterpret_cast<T*>(address) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool WriteOffset(void* base, std::size_t offset, const T& value) {
    if (base == nullptr) {
        return false;
    }
    return WriteValue(reinterpret_cast<std::uint8_t*>(base) + offset, value);
}

void* FindPatternInTextSection(const std::int16_t* pattern, std::size_t patternSize) {
    HMODULE module = GetMinecraftModule();
    if (module == nullptr || pattern == nullptr || patternSize == 0) {
        return nullptr;
    }

    const auto base = reinterpret_cast<std::uint8_t*>(module);
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }

    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }

    auto* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
        if (std::memcmp(section->Name, ".text", 5) != 0) {
            continue;
        }

        auto* begin = base + section->VirtualAddress;
        const std::size_t size = section->Misc.VirtualSize;
        if (size < patternSize) {
            return nullptr;
        }

        for (std::size_t offset = 0; offset <= size - patternSize; ++offset) {
            bool matched = true;
            for (std::size_t j = 0; j < patternSize; ++j) {
                if (pattern[j] >= 0 && begin[offset + j] != static_cast<std::uint8_t>(pattern[j])) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                return begin + offset;
            }
        }
    }
    return nullptr;
}

void StoreSavedAngles(const ActorAngles& angles) {
    g_savedPitch.store(angles.pitch, std::memory_order_relaxed);
    g_savedYaw.store(angles.yaw, std::memory_order_relaxed);
    g_savedPrevPitch.store(angles.prevPitch, std::memory_order_relaxed);
    g_savedPrevYaw.store(angles.prevYaw, std::memory_order_relaxed);
    g_hasSavedAngles.store(true, std::memory_order_release);
}

void StoreFrozenLookAngles(const Vec4& value) {
    g_frozenLookX.store(value.x, std::memory_order_relaxed);
    g_frozenLookY.store(value.y, std::memory_order_relaxed);
    g_frozenLookZ.store(value.z, std::memory_order_relaxed);
    g_frozenLookW.store(value.w, std::memory_order_relaxed);
}

void StoreLiveCameraLookAngles(const Vec4& value) {
    g_liveCameraLookX.store(value.x, std::memory_order_relaxed);
    g_liveCameraLookY.store(value.y, std::memory_order_relaxed);
    g_liveCameraLookZ.store(value.z, std::memory_order_relaxed);
    g_liveCameraLookW.store(value.w, std::memory_order_relaxed);
    g_hasLiveCameraLookAngles.store(true, std::memory_order_release);
}

ActorAngles LoadSavedAngles() {
    return ActorAngles{
        g_savedPitch.load(std::memory_order_relaxed),
        g_savedYaw.load(std::memory_order_relaxed),
        g_savedPrevPitch.load(std::memory_order_relaxed),
        g_savedPrevYaw.load(std::memory_order_relaxed),
    };
}

Vec4 LoadFrozenLookAngles() {
    return Vec4{
        g_frozenLookX.load(std::memory_order_relaxed),
        g_frozenLookY.load(std::memory_order_relaxed),
        g_frozenLookZ.load(std::memory_order_relaxed),
        g_frozenLookW.load(std::memory_order_relaxed),
    };
}

Vec4 LoadLiveCameraLookAngles() {
    return Vec4{
        g_liveCameraLookX.load(std::memory_order_relaxed),
        g_liveCameraLookY.load(std::memory_order_relaxed),
        g_liveCameraLookZ.load(std::memory_order_relaxed),
        g_liveCameraLookW.load(std::memory_order_relaxed),
    };
}

Vec4 RotationToQuaternion(float pitchDegrees, float yawDegrees) {
    const float pitch = pitchDegrees * kDegreesToRadians;
    const float yaw = yawDegrees * kDegreesToRadians;
    constexpr float roll = 0.0f;

    const float cy = std::cos(yaw * 0.5f);
    const float sy = std::sin(yaw * 0.5f);
    const float cp = std::cos(pitch * 0.5f);
    const float sp = std::sin(pitch * 0.5f);
    const float cr = std::cos(roll * 0.5f);
    const float sr = std::sin(roll * 0.5f);

    return Vec4{
        cp * cy * sr - sp * sy * cr,
        cp * cy * cr + sp * sy * sr,
        sp * cy * cr - cp * sy * sr,
        cp * sy * cr + sp * cy * sr,
    };
}

bool QuaternionToForward(const Vec4& look, float& x, float& y, float& z) {
    const float sinPitch = std::clamp(2.0f * (look.z * look.y - look.x * look.w), -1.0f, 1.0f);
    const float cosPitchSq = std::max(0.0f, 1.0f - sinPitch * sinPitch);
    const float cosPitch = std::sqrt(cosPitchSq);
    if (cosPitch <= 0.0001f) {
        x = 0.0f;
        y = -sinPitch;
        z = 0.0f;
        return std::isfinite(y);
    }

    const float sinYaw = 2.0f * (look.y * look.w - look.x * look.z);
    const float cosYaw = look.y * look.y + look.z * look.z - look.x * look.x - look.w * look.w;
    const float yawLength = std::sqrt(sinYaw * sinYaw + cosYaw * cosYaw);
    if (yawLength <= 0.0001f) {
        return false;
    }

    x = -(sinYaw / yawLength) * cosPitch;
    y = -sinPitch;
    z = (cosYaw / yawLength) * cosPitch;
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

bool BuildFreeLookCameraDirectoryPath(wchar_t* path, DWORD pathCount) {
    if (path == nullptr || pathCount == 0) {
        return false;
    }

    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    if (tempLength == 0 || tempLength >= MAX_PATH) {
        return false;
    }

    wchar_t basePath[MAX_PATH]{};
    if (swprintf_s(basePath, L"%sTaneClient", tempPath) < 0) {
        return false;
    }
    CreateDirectoryW(basePath, nullptr);

    wchar_t configPath[MAX_PATH]{};
    if (swprintf_s(configPath, L"%s\\Config", basePath) < 0) {
        return false;
    }
    CreateDirectoryW(configPath, nullptr);

    if (swprintf_s(path, pathCount, L"%s\\Camera", configPath) < 0) {
        return false;
    }
    CreateDirectoryW(path, nullptr);
    return true;
}

bool BuildFreeLookConfigPath(wchar_t* path, DWORD pathCount) {
    wchar_t cameraPath[MAX_PATH]{};
    if (!BuildFreeLookCameraDirectoryPath(cameraPath, MAX_PATH)) {
        return false;
    }
    return swprintf_s(path, pathCount, L"%s\\FreeLook.json", cameraPath) >= 0;
}

void AppendLabel(char* buffer, std::size_t bufferSize, const char* text) {
    if (buffer == nullptr || bufferSize == 0 || text == nullptr || text[0] == '\0') {
        return;
    }

    const std::size_t currentLength = std::strlen(buffer);
    if (currentLength + 1 >= bufferSize) {
        return;
    }

    if (currentLength > 0) {
        std::strncat(buffer, " + ", bufferSize - currentLength - 1);
    }

    const std::size_t updatedLength = std::strlen(buffer);
    std::strncat(buffer, text, bufferSize - updatedLength - 1);
}

const char* GetVirtualKeyName(int virtualKey) {
    switch (virtualKey) {
    case VK_CONTROL: return "Ctrl";
    case VK_LCONTROL: return "LCtrl";
    case VK_RCONTROL: return "RCtrl";
    case VK_SHIFT: return "Shift";
    case VK_LSHIFT: return "LShift";
    case VK_RSHIFT: return "RShift";
    case VK_MENU: return "Alt";
    case VK_LMENU: return "LAlt";
    case VK_RMENU: return "RAlt";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Escape";
    case VK_BACK: return "Backspace";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_LBUTTON: return "Mouse Left";
    case VK_RBUTTON: return "Mouse Right";
    case VK_MBUTTON: return "Mouse Middle";
    case VK_XBUTTON1: return "Mouse Side 1";
    case VK_XBUTTON2: return "Mouse Side 2";
    default:
        break;
    }

    static char name[32]{};
    if ((virtualKey >= 'A' && virtualKey <= 'Z') || (virtualKey >= '0' && virtualKey <= '9')) {
        name[0] = static_cast<char>(virtualKey);
        name[1] = '\0';
        return name;
    }
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        std::snprintf(name, sizeof(name), "F%d", virtualKey - VK_F1 + 1);
        return name;
    }
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9) {
        std::snprintf(name, sizeof(name), "Num%d", virtualKey - VK_NUMPAD0);
        return name;
    }

    std::snprintf(name, sizeof(name), "VK 0x%02X", virtualKey & 0xFF);
    return name;
}

int NormalizeVirtualKey(WPARAM virtualKey) {
    const int normalized = static_cast<int>(virtualKey);
    return normalized > 0 && normalized < kVirtualKeyCount ? normalized : 0;
}

int ResolveMessageVirtualKey(WPARAM virtualKey, LPARAM lParam) {
    const UINT scanCode = static_cast<UINT>((lParam >> 16) & 0xFF);
    if (scanCode != 0) {
        UINT extendedScanCode = scanCode;
        if ((lParam & (1u << 24)) != 0) {
            extendedScanCode |= 0xE000;
        }
        const UINT mapped = MapVirtualKeyW(extendedScanCode, MAPVK_VSC_TO_VK_EX);
        switch (mapped) {
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LMENU:
        case VK_RMENU:
            return NormalizeVirtualKey(mapped);
        default:
            break;
        }
    }
    return NormalizeVirtualKey(virtualKey);
}

int ResolveMouseButtonVirtualKey(UINT message, WPARAM wParam) {
    switch (message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
        return GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 :
            (GET_XBUTTON_WPARAM(wParam) == XBUTTON2 ? VK_XBUTTON2 : 0);
    default:
        return 0;
    }
}

bool IsVirtualKeyDown(int virtualKey) {
    return virtualKey > 0 && virtualKey < kVirtualKeyCount && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

void ClearKeyboardKeySet(bool* keys) {
    if (keys != nullptr) {
        std::memset(keys, 0, sizeof(bool) * kVirtualKeyCount);
    }
}

bool HasKeyboardKeySet(const bool* keys) {
    if (keys == nullptr) {
        return false;
    }
    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (keys[virtualKey]) {
            return true;
        }
    }
    return false;
}

bool HasKeyboardCaptureDownKeys() {
    return HasKeyboardKeySet(g_keyboardCaptureDownKeys);
}

bool IsPriorityKeyboardLabelKey(int virtualKey) {
    switch (virtualKey) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return true;
    default:
        return false;
    }
}

void SaveFreeLookConfig();

void UpdateKeyboardComboLabel() {
    g_keyboardComboLabel[0] = '\0';
    const int keyA = g_keyboardKeyA.load(std::memory_order_relaxed);
    const int keyB = g_keyboardKeyB.load(std::memory_order_relaxed);
    AppendLabel(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), GetVirtualKeyName(keyA));
    if (keyB != 0 && keyB != keyA) {
        AppendLabel(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), GetVirtualKeyName(keyB));
    }
    if (g_keyboardComboLabel[0] == '\0') {
        std::snprintf(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), "Not set");
    }
}

void UpdateControllerComboLabel() {
    const std::uint32_t mask = g_controllerMask.load(std::memory_order_relaxed);
    g_controllerComboLabel[0] = '\0';
    if ((mask & kControllerDpadUp) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Up");
    if ((mask & kControllerDpadDown) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Down");
    if ((mask & kControllerDpadLeft) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Left");
    if ((mask & kControllerDpadRight) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Right");
    if ((mask & kControllerStart) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Start");
    if ((mask & kControllerBack) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Back");
    if ((mask & kControllerLeftThumb) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LStick");
    if ((mask & kControllerRightThumb) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RStick");
    if ((mask & kControllerLeftShoulder) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LB");
    if ((mask & kControllerRightShoulder) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RB");
    if ((mask & kControllerA) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "A");
    if ((mask & kControllerB) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "B");
    if ((mask & kControllerX) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "X");
    if ((mask & kControllerY) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Y");
    if ((mask & kControllerLeftTrigger) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LT");
    if ((mask & kControllerRightTrigger) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RT");
    for (std::uint32_t index = 0; index < kExtendedControllerButtonCount; ++index) {
        if ((mask & (1u << (kExtendedControllerButtonShift + index))) != 0) {
            char label[16]{};
            std::snprintf(label, sizeof(label), "Rear %u", index + 1);
            AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), label);
        }
    }
    if (g_controllerComboLabel[0] == '\0') {
        std::snprintf(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Not set");
    }
}

void StoreCapturedKeyboardCombo() {
    if (!HasKeyboardKeySet(g_keyboardCaptureCandidateKeys)) {
        return;
    }

    int firstKey = 0;
    int secondKey = 0;
    constexpr int kPriorityKeys[] = {
        VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
        VK_SHIFT, VK_LSHIFT, VK_RSHIFT,
        VK_MENU, VK_LMENU, VK_RMENU,
    };
    auto appendKey = [&](int virtualKey) {
        if (virtualKey <= 0 || virtualKey >= kVirtualKeyCount || !g_keyboardCaptureCandidateKeys[virtualKey]) {
            return;
        }
        if (firstKey == 0) {
            firstKey = virtualKey;
        } else if (secondKey == 0 && firstKey != virtualKey) {
            secondKey = virtualKey;
        }
    };

    for (int virtualKey : kPriorityKeys) {
        appendKey(virtualKey);
    }
    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (!IsPriorityKeyboardLabelKey(virtualKey)) {
            appendKey(virtualKey);
        }
    }

    if (firstKey != 0) {
        g_keyboardKeyA.store(firstKey, std::memory_order_relaxed);
        g_keyboardKeyB.store(secondKey, std::memory_order_relaxed);
        UpdateKeyboardComboLabel();
        SaveFreeLookConfig();
    }
}

void LoadFreeLookConfig() {
    if (g_configLoaded.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildFreeLookConfigPath(path, MAX_PATH)) {
        UpdateKeyboardComboLabel();
        UpdateControllerComboLabel();
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize{};
        if (GetFileSizeEx(file, &fileSize) && fileSize.QuadPart > 0 && fileSize.QuadPart < 4096) {
            std::string json(static_cast<std::size_t>(fileSize.QuadPart), '\0');
            DWORD bytesRead = 0;
            if (ReadFile(file, json.data(), static_cast<DWORD>(json.size()), &bytesRead, nullptr)) {
                json.resize(bytesRead);
                auto readBool = [&](const char* key, bool fallback) {
                    const char* keyPosition = std::strstr(json.c_str(), key);
                    const char* colon = keyPosition != nullptr ? std::strchr(keyPosition, ':') : nullptr;
                    if (colon == nullptr) return fallback;
                    while (*++colon == ' ' || *colon == '\t') {}
                    if (std::strncmp(colon, "true", 4) == 0) return true;
                    if (std::strncmp(colon, "false", 5) == 0) return false;
                    return fallback;
                };
                auto readInt = [&](const char* key, int fallback) {
                    const char* keyPosition = std::strstr(json.c_str(), key);
                    const char* colon = keyPosition != nullptr ? std::strchr(keyPosition, ':') : nullptr;
                    if (colon == nullptr) return fallback;
                    char* end = nullptr;
                    const long value = std::strtol(colon + 1, &end, 10);
                    return end != colon + 1 ? static_cast<int>(value) : fallback;
                };

                const int configVersion = readInt("\"configVersion\"", 0);
                const int loadedControllerMask = readInt("\"controllerMask\"", static_cast<int>(kDefaultControllerMask));
                std::uint32_t controllerMask = static_cast<std::uint32_t>(std::max(0, loadedControllerMask));
                if (configVersion < 2 && controllerMask == kLegacyDefaultControllerMask) {
                    controllerMask = kDefaultControllerMask;
                }

                g_freeLookEnabled.store(readBool("\"enabled\"", false), std::memory_order_relaxed);
                g_keyboardKeyA.store(readInt("\"keyboardKeyA\"", kDefaultKeyboardKeyA), std::memory_order_relaxed);
                g_keyboardKeyB.store(readInt("\"keyboardKeyB\"", kDefaultKeyboardKeyB), std::memory_order_relaxed);
                g_controllerMask.store(controllerMask, std::memory_order_relaxed);
            }
        }
        CloseHandle(file);
    }

    UpdateKeyboardComboLabel();
    UpdateControllerComboLabel();
}

void EnsureFreeLookConfigLoaded() {
    if (!g_configLoaded.load(std::memory_order_acquire)) {
        LoadFreeLookConfig();
    }
}

void SaveFreeLookConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildFreeLookConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[320]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"configVersion\": %d,\n"
        "  \"enabled\": %s,\n"
        "  \"keyboardKeyA\": %d,\n"
        "  \"keyboardKeyB\": %d,\n"
        "  \"controllerMask\": %u\n"
        "}\n",
        kFreeLookConfigVersion,
        g_freeLookEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_keyboardKeyA.load(std::memory_order_relaxed),
        g_keyboardKeyB.load(std::memory_order_relaxed),
        g_controllerMask.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD bytesWritten = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &bytesWritten, nullptr);
    CloseHandle(file);
}

XInputGetStateFn ResolveXInputGetState() {
    if (g_triedResolveXInput) {
        return g_xInputGetState;
    }
    g_triedResolveXInput = true;
    constexpr const wchar_t* kDllNames[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    for (const wchar_t* dllName : kDllNames) {
        HMODULE module = LoadLibraryW(dllName);
        if (module == nullptr) {
            continue;
        }
        g_xInputGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(module, "XInputGetState"));
        if (g_xInputGetState != nullptr) {
            return g_xInputGetState;
        }
    }
    return nullptr;
}

std::uint32_t GetControllerFlagsFromState(const XInputGamepadState& gamepad) {
    std::uint32_t flags = 0;
    if ((gamepad.buttons & 0x0001) != 0) flags |= kControllerDpadUp;
    if ((gamepad.buttons & 0x0002) != 0) flags |= kControllerDpadDown;
    if ((gamepad.buttons & 0x0004) != 0) flags |= kControllerDpadLeft;
    if ((gamepad.buttons & 0x0008) != 0) flags |= kControllerDpadRight;
    if ((gamepad.buttons & 0x0010) != 0) flags |= kControllerStart;
    if ((gamepad.buttons & 0x0020) != 0) flags |= kControllerBack;
    if ((gamepad.buttons & kXInputGamepadLeftThumb) != 0) flags |= kControllerLeftThumb;
    if ((gamepad.buttons & kXInputGamepadRightThumb) != 0) flags |= kControllerRightThumb;
    if ((gamepad.buttons & kXInputGamepadLeftShoulder) != 0) flags |= kControllerLeftShoulder;
    if ((gamepad.buttons & kXInputGamepadRightShoulder) != 0) flags |= kControllerRightShoulder;
    if ((gamepad.buttons & 0x1000) != 0) flags |= kControllerA;
    if ((gamepad.buttons & 0x2000) != 0) flags |= kControllerB;
    if ((gamepad.buttons & 0x4000) != 0) flags |= kControllerX;
    if ((gamepad.buttons & 0x8000) != 0) flags |= kControllerY;
    if (gamepad.leftTrigger > kXInputTriggerThreshold) flags |= kControllerLeftTrigger;
    if (gamepad.rightTrigger > kXInputTriggerThreshold) flags |= kControllerRightTrigger;
    return flags;
}

bool ReadXInputControllerFlags(std::uint32_t& flags, ULONGLONG now) {
    flags = 0;
    const bool controllerPresent = g_xInputControllerPresent.load(std::memory_order_relaxed);
    const ULONGLONG lastPoll = g_lastXInputPollTick.load(std::memory_order_relaxed);
    if (!controllerPresent && lastPoll != 0 && now - lastPoll < kDisconnectedXInputPollIntervalMs) {
        return false;
    }
    XInputGetStateFn getState = ResolveXInputGetState();
    if (getState == nullptr) {
        g_xInputControllerPresent.store(false, std::memory_order_relaxed);
        return false;
    }

    bool foundController = false;
    for (DWORD userIndex = 0; userIndex < 4; ++userIndex) {
        XInputState state{};
        if (getState(userIndex, &state) == ERROR_SUCCESS) {
            foundController = true;
            flags |= GetControllerFlagsFromState(state.gamepad);
        }
    }
    g_lastXInputPollTick.store(now, std::memory_order_relaxed);
    g_xInputControllerPresent.store(foundController, std::memory_order_relaxed);
    g_cachedXInputControllerFlags.store(flags, std::memory_order_relaxed);
    return foundController;
}

bool ReadVirtualGamepadFlags(std::uint32_t& flags) {
    flags = 0;
#ifdef VK_GAMEPAD_DPAD_UP
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_UP) & 0x8000) != 0) flags |= kControllerDpadUp;
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_DOWN) & 0x8000) != 0) flags |= kControllerDpadDown;
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_LEFT) & 0x8000) != 0) flags |= kControllerDpadLeft;
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_RIGHT) & 0x8000) != 0) flags |= kControllerDpadRight;
    if ((GetAsyncKeyState(VK_GAMEPAD_MENU) & 0x8000) != 0) flags |= kControllerStart;
    if ((GetAsyncKeyState(VK_GAMEPAD_VIEW) & 0x8000) != 0) flags |= kControllerBack;
    if ((GetAsyncKeyState(VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON) & 0x8000) != 0) flags |= kControllerLeftThumb;
    if ((GetAsyncKeyState(VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON) & 0x8000) != 0) flags |= kControllerRightThumb;
    if ((GetAsyncKeyState(VK_GAMEPAD_LEFT_SHOULDER) & 0x8000) != 0) flags |= kControllerLeftShoulder;
    if ((GetAsyncKeyState(VK_GAMEPAD_RIGHT_SHOULDER) & 0x8000) != 0) flags |= kControllerRightShoulder;
    if ((GetAsyncKeyState(VK_GAMEPAD_A) & 0x8000) != 0) flags |= kControllerA;
    if ((GetAsyncKeyState(VK_GAMEPAD_B) & 0x8000) != 0) flags |= kControllerB;
    if ((GetAsyncKeyState(VK_GAMEPAD_X) & 0x8000) != 0) flags |= kControllerX;
    if ((GetAsyncKeyState(VK_GAMEPAD_Y) & 0x8000) != 0) flags |= kControllerY;
    if ((GetAsyncKeyState(VK_GAMEPAD_LEFT_TRIGGER) & 0x8000) != 0) flags |= kControllerLeftTrigger;
    if ((GetAsyncKeyState(VK_GAMEPAD_RIGHT_TRIGGER) & 0x8000) != 0) flags |= kControllerRightTrigger;
#endif
    return true;
}

std::uint32_t ReadControllerFlags() {
    const ULONGLONG now = GetTickCount64();
    ULONGLONG lastReadTick = g_lastControllerReadTick.load(std::memory_order_relaxed);
    if (lastReadTick != 0 && now - lastReadTick < kControllerPollIntervalMs) {
        return g_cachedControllerFlags.load(std::memory_order_relaxed);
    }
    if (!g_lastControllerReadTick.compare_exchange_strong(lastReadTick, now, std::memory_order_relaxed)) {
        return g_cachedControllerFlags.load(std::memory_order_relaxed);
    }

    std::uint32_t xInputFlags = g_cachedXInputControllerFlags.load(std::memory_order_relaxed);
    std::uint32_t virtualFlags = 0;
    ReadXInputControllerFlags(xInputFlags, now);
    ReadVirtualGamepadFlags(virtualFlags);
    const std::uint32_t flags = xInputFlags | virtualFlags | tane::payload::GetExtendedControllerFlags();
    g_cachedControllerFlags.store(flags, std::memory_order_relaxed);
    return flags;
}

bool IsKeyboardComboHeld() {
    const int keyA = g_keyboardKeyA.load(std::memory_order_relaxed);
    const int keyB = g_keyboardKeyB.load(std::memory_order_relaxed);
    return keyA > 0 && IsVirtualKeyDown(keyA) && (keyB <= 0 || IsVirtualKeyDown(keyB));
}

bool IsControllerComboHeld(std::uint32_t controllerFlags) {
    const std::uint32_t mask = g_controllerMask.load(std::memory_order_relaxed);
    return mask != 0 && (controllerFlags & mask) == mask;
}

void TickControllerComboCapture() {
    if (!g_controllerCaptureActive) {
        return;
    }
    const std::uint32_t flags = ReadControllerFlags();
    if (flags != 0) {
        if (g_controllerCaptureWaitingForRelease) {
            g_controllerCaptureWaitingForRelease = false;
        }
        g_controllerCaptureCandidateMask |= flags;
    } else if (!g_controllerCaptureWaitingForRelease && g_controllerCaptureCandidateMask != 0) {
        g_controllerMask.store(g_controllerCaptureCandidateMask, std::memory_order_relaxed);
        g_controllerCaptureActive = false;
        g_controllerCaptureWaitingForRelease = false;
        g_controllerCaptureCandidateMask = 0;
        UpdateControllerComboLabel();
        SaveFreeLookConfig();
    }
}

void* GetClientInstanceLocalPlayerFunction(void* clientInstance) {
    void** vtable = nullptr;
    if (!ReadValue(clientInstance, vtable) || vtable == nullptr) {
        return nullptr;
    }

    if (g_cachedClientInstanceVtable == vtable && g_cachedLocalPlayerFunction != nullptr) {
        return g_cachedLocalPlayerFunction;
    }

    void* function = nullptr;
    if (!ReadOffset(vtable, tane::offsets::camera::kClientInstanceLocalPlayerVtableOffset, function) ||
        !IsExecutableAddress(function)) {
        g_cachedClientInstanceVtable = nullptr;
        g_cachedLocalPlayerFunction = nullptr;
        return nullptr;
    }

    g_cachedClientInstanceVtable = vtable;
    g_cachedLocalPlayerFunction = function;
    return function;
}

bool LooksLikeActor(void* actor) {
    void** vtable = nullptr;
    void* entityContext = nullptr;
    std::uint32_t entityId = 0;
    return ReadValue(actor, vtable) &&
        vtable != nullptr &&
        ReadOffset(actor, tane::offsets::camera::kActorEntityContextOffset, entityContext) &&
        ReadOffset(actor, tane::offsets::camera::kActorEntityIdOffset, entityId) &&
        entityContext != nullptr &&
        entityId != 0;
}

void* GetLocalPlayer(void* clientInstance) {
    void* function = GetClientInstanceLocalPlayerFunction(clientInstance);
    if (function == nullptr) {
        return nullptr;
    }

    __try {
        void* localPlayer = reinterpret_cast<GetLocalPlayerFn>(function)(clientInstance);
        return LooksLikeActor(localPlayer) ? localPlayer : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool ResolvePackedComponentId(void* storage, std::uint32_t entityId, std::uint32_t& packedComponentId) {
    void* pageListBegin = nullptr;
    void* pageListEnd = nullptr;
    if (!ReadOffset(storage, 0x08, pageListBegin) || !ReadOffset(storage, 0x10, pageListEnd)) {
        return false;
    }

    const auto begin = reinterpret_cast<std::uintptr_t>(pageListBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(pageListEnd);
    if (end < begin) {
        return false;
    }

    const std::uint32_t sparseId = entityId & 0x3FFFF;
    const std::uintptr_t pageIndex = sparseId >> 11;
    const std::uintptr_t pageCount = (end - begin) / sizeof(void*);
    if (pageIndex >= pageCount) {
        return false;
    }

    void* page = nullptr;
    if (!ReadOffset(pageListBegin, pageIndex * sizeof(void*), page) || page == nullptr) {
        return false;
    }

    if (!ReadOffset(page, (sparseId & 0x7FF) * sizeof(std::uint32_t), packedComponentId)) {
        return false;
    }

    return ((entityId & 0xFFFC0000) ^ packedComponentId) <= 0x3FFFE;
}

void* ResolveComponentFromStorage(void* storage, std::uint32_t entityId, std::size_t entrySize) {
    std::uint32_t packedComponentId = 0;
    void* componentPages = nullptr;
    if (entrySize == 0 ||
        !ResolvePackedComponentId(storage, entityId, packedComponentId) ||
        !ReadOffset(storage, 0x50, componentPages) ||
        componentPages == nullptr) {
        return nullptr;
    }

    const std::size_t pageOffset = ((packedComponentId >> 4) & 0x3FF8);
    void* componentPage = nullptr;
    if (!ReadOffset(componentPages, pageOffset, componentPage) || componentPage == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<std::uint8_t*>(componentPage) + (packedComponentId & 0x7F) * entrySize;
}

void* FindComponentStorage(void* entityContext, std::uint32_t hash) {
    if (entityContext == nullptr) {
        return nullptr;
    }

    void* hashBegin = nullptr;
    void* hashEnd = nullptr;
    void* hashEntries = nullptr;
    void* hashSentinel = nullptr;
    if (!ReadOffset(entityContext, 0x48, hashBegin) ||
        !ReadOffset(entityContext, 0x50, hashEnd) ||
        !ReadOffset(entityContext, 0x68, hashEntries) ||
        !ReadOffset(entityContext, 0x70, hashSentinel) ||
        hashBegin == nullptr ||
        hashEntries == nullptr) {
        return nullptr;
    }

    const auto begin = reinterpret_cast<std::uintptr_t>(hashBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(hashEnd);
    if (end <= begin) {
        return nullptr;
    }

    const std::uintptr_t slotCount = (end - begin) / sizeof(void*);
    if (slotCount == 0) {
        return nullptr;
    }

    void* slot = reinterpret_cast<void*>(begin + ((slotCount - 1) & hash) * sizeof(void*));
    for (int attempt = 0; attempt < 64; ++attempt) {
        std::uint64_t entryIndex = 0;
        if (!ReadValue(slot, entryIndex) || entryIndex == UINT64_MAX) {
            return nullptr;
        }

        void* entry = reinterpret_cast<std::uint8_t*>(hashEntries) + entryIndex * 0x20;
        std::uint32_t entryHash = 0;
        if (!ReadOffset(entry, 0x08, entryHash)) {
            return nullptr;
        }

        if (entryHash == hash) {
            if (entry == hashSentinel) {
                return nullptr;
            }

            void* storage = nullptr;
            return ReadOffset(entry, 0x10, storage) ? storage : nullptr;
        }

        slot = entry;
    }

    return nullptr;
}

void* ResolveComponent(void* actor, std::uint32_t hash, std::size_t entrySize) {
    void* entityContext = nullptr;
    std::uint32_t entityId = 0;
    if (!ReadOffset(actor, tane::offsets::camera::kActorEntityContextOffset, entityContext) ||
        !ReadOffset(actor, tane::offsets::camera::kActorEntityIdOffset, entityId) ||
        entityContext == nullptr) {
        return nullptr;
    }

    void* storage = FindComponentStorage(entityContext, hash);
    return storage != nullptr ? ResolveComponentFromStorage(storage, entityId, entrySize) : nullptr;
}

bool ReadActorAnglesFromComponent(void* rotation, ActorAngles& angles) {
    return ReadOffset(rotation, tane::offsets::camera::kActorRotationPitchOffset, angles.pitch) &&
        ReadOffset(rotation, tane::offsets::camera::kActorRotationYawOffset, angles.yaw) &&
        ReadOffset(rotation, tane::offsets::camera::kActorRotationPrevPitchOffset, angles.prevPitch) &&
        ReadOffset(rotation, tane::offsets::camera::kActorRotationPrevYawOffset, angles.prevYaw) &&
        std::isfinite(angles.pitch) &&
        std::isfinite(angles.yaw);
}

void* ResolveActorRotationComponent(void* localPlayer) {
    return ResolveComponent(
        localPlayer,
        tane::offsets::camera::kActorRotationComponentHash,
        tane::offsets::camera::kActorRotationComponentEntrySize);
}

bool WriteActorAnglesToComponent(void* rotation, const ActorAngles& angles) {
    if (rotation == nullptr) {
        return false;
    }

    const bool wrotePitch = WriteOffset(rotation, tane::offsets::camera::kActorRotationPitchOffset, angles.pitch);
    const bool wroteYaw = WriteOffset(rotation, tane::offsets::camera::kActorRotationYawOffset, angles.yaw);
    const bool wrotePrevPitch = WriteOffset(rotation, tane::offsets::camera::kActorRotationPrevPitchOffset, angles.prevPitch);
    const bool wrotePrevYaw = WriteOffset(rotation, tane::offsets::camera::kActorRotationPrevYawOffset, angles.prevYaw);
    return wrotePitch && wroteYaw && wrotePrevPitch && wrotePrevYaw;
}

void CaptureFreeLookRotation(void* clientInstance) {
    if (g_hasSavedAngles.load(std::memory_order_acquire)) {
        return;
    }

    void* localPlayer = GetLocalPlayer(clientInstance);
    ActorAngles angles{};
    void* rotation = localPlayer != nullptr ? ResolveActorRotationComponent(localPlayer) : nullptr;
    if (rotation != nullptr && ReadActorAnglesFromComponent(rotation, angles)) {
        g_savedRotationComponent.store(rotation, std::memory_order_release);
        StoreSavedAngles(angles);
        StoreFrozenLookAngles(RotationToQuaternion(angles.pitch, angles.yaw));
    }
}

void RestoreFreeLookRotation() {
    if (!g_hasSavedAngles.load(std::memory_order_acquire)) {
        return;
    }

    WriteActorAnglesToComponent(
        g_savedRotationComponent.load(std::memory_order_acquire),
        LoadSavedAngles());
}

int __fastcall HookOptionsGetPerspective(void* options) {
    if (g_freeLookActive.load(std::memory_order_relaxed) &&
        tane::gui::CanRunGameplayModules()) {
        return kPerspectiveThirdPersonBack;
    }

    return g_originalOptionsGetPerspective != nullptr
        ? g_originalOptionsGetPerspective(options)
        : kPerspectiveFirstPerson;
}

void __fastcall HookUpdatePlayerFromCamera(void* cameraComponent, void* updateContext, void* player) {
    if (g_originalUpdatePlayerFromCamera == nullptr) {
        return;
    }

    if (!g_freeLookActive.load(std::memory_order_relaxed) ||
        !tane::gui::CanRunGameplayModules() ||
        !g_hasSavedAngles.load(std::memory_order_acquire)) {
        g_originalUpdatePlayerFromCamera(cameraComponent, updateContext, player);
        return;
    }

    Vec4 originalLookAngles{};
    void* lookAnglesAddress = reinterpret_cast<std::uint8_t*>(cameraComponent) +
        tane::offsets::camera::kCameraComponentLookAnglesOffset;
    if (!ReadValue(lookAnglesAddress, originalLookAngles)) {
        g_originalUpdatePlayerFromCamera(cameraComponent, updateContext, player);
        return;
    }

    g_liveCameraComponent.store(cameraComponent, std::memory_order_release);
    StoreLiveCameraLookAngles(originalLookAngles);
    WriteValue(lookAnglesAddress, LoadFrozenLookAngles());
    g_originalUpdatePlayerFromCamera(cameraComponent, updateContext, player);
    WriteValue(lookAnglesAddress, originalLookAngles);
}

bool CreateAndEnableHook(void* target, void* detour, void** original) {
    if (target == nullptr || detour == nullptr || original == nullptr || !IsExecutableAddress(target)) {
        return false;
    }

    const MH_STATUS createStatus = MH_CreateHook(target, detour, original);
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    return enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED;
}

void* ResolveOptionsGetPerspectiveTarget() {
    constexpr std::int16_t kOptionsGetPerspectiveSignature[] = {
        0x48, 0x83, 0xEC, 0x38, 0x48, 0x8B, 0x05, -1, -1, -1, -1,
        0x48, 0x31, 0xE0, 0x48, 0x89, 0x44, 0x24, -1,
        0x48, 0x8B, 0x01, 0x48, 0x8B, 0x40, 0x08,
        0x48, 0x8D, 0x54, 0x24, -1, 0x41, 0xB8, 0x03, 0x00, 0x00, 0x00,
    };
    return FindPatternInTextSection(
        kOptionsGetPerspectiveSignature,
        sizeof(kOptionsGetPerspectiveSignature) / sizeof(kOptionsGetPerspectiveSignature[0]));
}

void StartFreeLook(void* clientInstance) {
    CaptureFreeLookRotation(clientInstance);
    g_freeLookActive.store(true, std::memory_order_relaxed);
    RestoreFreeLookRotation();
}

void StopFreeLook() {
    RestoreFreeLookRotation();
    g_freeLookActive.store(false, std::memory_order_relaxed);
    g_savedRotationComponent.store(nullptr, std::memory_order_release);
    g_hasSavedAngles.store(false, std::memory_order_release);
    g_hasLiveCameraLookAngles.store(false, std::memory_order_release);
    g_liveCameraComponent.store(nullptr, std::memory_order_release);
}

}  // namespace

bool IsFreeLookEnabled() {
    EnsureFreeLookConfigLoaded();
    return g_freeLookEnabled.load(std::memory_order_relaxed);
}

bool GetFreeLookCameraForward(float& x, float& y, float& z) {
    if (!g_freeLookActive.load(std::memory_order_relaxed) ||
        !g_hasLiveCameraLookAngles.load(std::memory_order_acquire) ||
        !tane::gui::CanRunGameplayModules()) {
        return false;
    }

    Vec4 look = LoadLiveCameraLookAngles();
    void* cameraComponent = g_liveCameraComponent.load(std::memory_order_acquire);
    if (cameraComponent != nullptr) {
        const void* lookAnglesAddress = reinterpret_cast<const std::uint8_t*>(cameraComponent) +
            tane::offsets::camera::kCameraComponentLookAnglesOffset;
        Vec4 liveLook{};
        if (ReadValue(lookAnglesAddress, liveLook)) {
            look = liveLook;
            StoreLiveCameraLookAngles(liveLook);
        }
    }

    return QuaternionToForward(look, x, y, z);
}

void SetFreeLookEnabled(bool enabled) {
    EnsureFreeLookConfigLoaded();
    g_freeLookEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        g_freeLookHeld.store(false, std::memory_order_relaxed);
        StopFreeLook();
    }
    SaveFreeLookConfig();
}

const char* GetFreeLookKeyboardComboLabel() {
    EnsureFreeLookConfigLoaded();
    UpdateKeyboardComboLabel();
    return g_keyboardComboLabel;
}

const char* GetFreeLookControllerComboLabel() {
    EnsureFreeLookConfigLoaded();
    UpdateControllerComboLabel();
    return g_controllerComboLabel;
}

bool IsFreeLookKeyboardComboCaptureActive() {
    return g_keyboardCaptureActive;
}

bool IsFreeLookControllerComboCaptureActive() {
    return g_controllerCaptureActive;
}

bool IsFreeLookControllerComboPolling() {
    return g_controllerCaptureActive;
}

void BeginFreeLookKeyboardComboCapture() {
    EnsureFreeLookConfigLoaded();
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_keyboardCaptureWaitingForRelease = true;
    g_keyboardCaptureActive = true;
    g_controllerCaptureActive = false;
    g_controllerCaptureCandidateMask = 0;
}

void BeginFreeLookControllerComboCapture() {
    EnsureFreeLookConfigLoaded();
    g_controllerCaptureActive = true;
    g_controllerCaptureWaitingForRelease = true;
    g_controllerCaptureCandidateMask = 0;
    g_keyboardCaptureActive = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
}

void CancelFreeLookComboCapture() {
    g_keyboardCaptureActive = false;
    g_keyboardCaptureWaitingForRelease = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_controllerCaptureActive = false;
    g_controllerCaptureWaitingForRelease = false;
    g_controllerCaptureCandidateMask = 0;
}

bool HandleFreeLookKeyMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    EnsureFreeLookConfigLoaded();
    const bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool keyUp = message == WM_KEYUP || message == WM_SYSKEYUP;
    const bool mouseDown = message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK;
    const bool mouseUp = message == WM_XBUTTONUP;

    if (!g_keyboardCaptureActive) {
        return false;
    }
    if (!keyDown && !keyUp && !mouseDown && !mouseUp) {
        return false;
    }

    const bool firstPress = mouseDown || (lParam & (1u << 30)) == 0;
    const int resolvedVirtualKey = (keyDown || keyUp)
        ? ResolveMessageVirtualKey(wParam, lParam)
        : ResolveMouseButtonVirtualKey(message, wParam);
    if (keyDown && firstPress && resolvedVirtualKey == VK_ESCAPE) {
        CancelFreeLookComboCapture();
        return true;
    }

    if ((keyDown || mouseDown) && firstPress) {
        if (g_keyboardCaptureWaitingForRelease) {
            g_keyboardCaptureWaitingForRelease = false;
        }
        if (resolvedVirtualKey > 0) {
            g_keyboardCaptureCandidateKeys[resolvedVirtualKey] = true;
            g_keyboardCaptureDownKeys[resolvedVirtualKey] = true;
        }
    } else if (keyUp || mouseUp) {
        if (resolvedVirtualKey > 0) {
            g_keyboardCaptureDownKeys[resolvedVirtualKey] = false;
        }
        if (HasKeyboardKeySet(g_keyboardCaptureCandidateKeys) && !HasKeyboardCaptureDownKeys()) {
            StoreCapturedKeyboardCombo();
            g_keyboardCaptureActive = false;
            g_keyboardCaptureWaitingForRelease = false;
            ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
            ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
        }
    }

    return true;
}

void TickFreeLookComboCapture() {
    EnsureFreeLookConfigLoaded();
    TickControllerComboCapture();
}

void TickFreeLook(void* clientInstance) {
    EnsureFreeLookConfigLoaded();
    TickControllerComboCapture();

    if (!g_freeLookEnabled.load(std::memory_order_relaxed) ||
        clientInstance == nullptr ||
        !tane::gui::CanRunGameplayModules()) {
        g_freeLookHeld.store(false, std::memory_order_relaxed);
        if (g_freeLookActive.load(std::memory_order_relaxed)) {
            StopFreeLook();
        }
        return;
    }

    const std::uint32_t controllerFlags = ReadControllerFlags();
    const bool freeLookHeld = IsKeyboardComboHeld() || IsControllerComboHeld(controllerFlags);
    g_freeLookHeld.store(freeLookHeld, std::memory_order_relaxed);

    if (freeLookHeld) {
        if (!g_freeLookActive.load(std::memory_order_relaxed)) {
            StartFreeLook(clientInstance);
        } else {
            RestoreFreeLookRotation();
        }
    } else if (g_freeLookActive.load(std::memory_order_relaxed)) {
        StopFreeLook();
    }
}

bool InstallFreeLookHooks() {
    EnsureFreeLookConfigLoaded();
    if (g_hooksInstalled.exchange(true, std::memory_order_acq_rel)) {
        return true;
    }

    bool installedAnyHook = false;
    void* updatePlayerTarget = GetImageAddress(tane::offsets::camera::kFreeLookUpdatePlayerFromCameraRva);
    installedAnyHook |= CreateAndEnableHook(
        updatePlayerTarget,
        reinterpret_cast<void*>(&HookUpdatePlayerFromCamera),
        reinterpret_cast<void**>(&g_originalUpdatePlayerFromCamera));

    void* optionsPerspectiveTarget = ResolveOptionsGetPerspectiveTarget();
    installedAnyHook |= CreateAndEnableHook(
        optionsPerspectiveTarget,
        reinterpret_cast<void*>(&HookOptionsGetPerspective),
        reinterpret_cast<void**>(&g_originalOptionsGetPerspective));

    if (!installedAnyHook) {
        g_hooksInstalled.store(false, std::memory_order_release);
    }
    return installedAnyHook;
}

}  // namespace tane::camera
