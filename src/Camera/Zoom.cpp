#include <Windows.h>
#include <MinHook.h>

#include "Offsets.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
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
using CameraProjectionFovOptionFn = float(__fastcall*)(void* options, float frameAlpha, int applyViewportScale);
using CameraDefaultFovApplyFn = void(__fastcall*)(void* cameraOutput, const float* sourceFov);
using CameraCustomFovApplyFn = void(__fastcall*)(
    void* cameraContext,
    void* cameraOutput,
    const float* sourceFov,
    void* customFovComponent,
    void* cameraEntityContext,
    float deltaTime);

constexpr WORD kXInputGamepadLeftShoulder = 0x0100;
constexpr WORD kXInputGamepadRightShoulder = 0x0200;
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
constexpr int kZoomConfigVersion = 2;
constexpr int kVirtualKeyCount = 256;
constexpr int kDefaultKeyboardKeyA = 'C';
constexpr int kDefaultKeyboardKeyB = 0;
constexpr int kLegacyDefaultKeyboardKeyA = VK_RCONTROL;
constexpr int kLegacyDefaultKeyboardKeyB = 'Z';
constexpr int kDevelopmentDefaultKeyboardKeyA = 'M';
constexpr int kDevelopmentDefaultKeyboardKeyB = 0;
constexpr std::uint32_t kDefaultControllerMask = kControllerLeftThumb;
constexpr std::uint32_t kLegacyDefaultControllerMask = kControllerLeftShoulder | kControllerRightShoulder | kControllerRightThumb;
constexpr float kDefaultZoomAmount = 2.0f;
constexpr float kMinZoomAmount = 1.0f;
constexpr float kMaxZoomAmount = 10.0f;
constexpr float kWheelZoomStep = 0.50f;
constexpr float kZoomAnimationLerpFactor = 0.5f;
constexpr ULONGLONG kControllerPollIntervalMs = 16;
constexpr ULONGLONG kDisconnectedXInputPollIntervalMs = 300;

std::atomic_bool g_zoomEnabled = false;
std::atomic_bool g_configLoaded = false;
std::atomic_bool g_zoomHeld = false;
std::atomic_bool g_smoothZoomAnimationEnabled = false;
std::atomic<float> g_zoomAmount = kDefaultZoomAmount;
std::atomic<float> g_keyboardSessionZoomAmount = kDefaultZoomAmount;
std::atomic_bool g_keyboardSessionZoomInitialized = false;
std::atomic<float> g_effectiveZoomAmount = 1.0f;
std::atomic<float> g_zoomFovMultiplier = 1.0f;
std::atomic<std::uint64_t> g_lastZoomAnimationCounter = 0;
std::atomic<int> g_keyboardKeyA = kDefaultKeyboardKeyA;
std::atomic<int> g_keyboardKeyB = kDefaultKeyboardKeyB;
std::atomic<std::uint32_t> g_controllerMask = kDefaultControllerMask;
std::atomic_bool g_hooksInstalled = false;
std::atomic<ULONGLONG> g_lastControllerReadTick = 0;
std::atomic<ULONGLONG> g_lastXInputPollTick = 0;
std::atomic<std::uint32_t> g_cachedControllerFlags = 0;
std::atomic<std::uint32_t> g_cachedXInputControllerFlags = 0;
std::atomic_bool g_xInputControllerPresent = false;
std::atomic<int> g_zoomInputMode = 0;

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
CameraProjectionFovOptionFn g_originalCameraProjectionFovOption = nullptr;
CameraDefaultFovApplyFn g_originalCameraDefaultFovApply = nullptr;
CameraCustomFovApplyFn g_originalCameraCustomFovApply = nullptr;

float ClampZoomAmount(float value) {
    return std::clamp(value, kMinZoomAmount, kMaxZoomAmount);
}

bool BuildZoomCameraDirectoryPath(wchar_t* path, DWORD pathCount) {
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
    case VK_NUMLOCK: return "NumLock";
    case VK_SCROLL: return "ScrollLock";
    case VK_CAPITAL: return "CapsLock";
    case VK_OEM_1: return ";";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    case VK_OEM_3: return "`";
    case VK_OEM_4: return "[";
    case VK_OEM_5: return "\\";
    case VK_OEM_6: return "]";
    case VK_OEM_7: return "'";
    default:
        break;
    }

    static char name[32] = {};
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

    const UINT scanCode = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC_EX);
    if (scanCode != 0) {
        const LONG lParam = static_cast<LONG>(scanCode << 16);
        if (GetKeyNameTextA(lParam, name, static_cast<int>(sizeof(name))) > 0) {
            return name;
        }
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
        switch (GET_XBUTTON_WPARAM(wParam)) {
        case XBUTTON1:
            return VK_XBUTTON1;
        case XBUTTON2:
            return VK_XBUTTON2;
        default:
            return 0;
        }
    default:
        break;
    }

    return 0;
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

bool BuildZoomConfigPath(wchar_t* path, DWORD pathCount) {
    wchar_t cameraPath[MAX_PATH]{};
    if (!BuildZoomCameraDirectoryPath(cameraPath, MAX_PATH)) {
        return false;
    }

    return swprintf_s(path, pathCount, L"%s\\Zoom.json", cameraPath) >= 0;
}

void UpdateKeyboardComboLabel();
void UpdateControllerComboLabel();
void SaveZoomConfig();

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
        SaveZoomConfig();
    }
}

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

void LoadZoomConfig() {
    if (g_configLoaded.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildZoomConfigPath(path, MAX_PATH)) {
        UpdateKeyboardComboLabel();
        UpdateControllerComboLabel();
        return;
    }

    bool migratedConfig = false;
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
                    if (keyPosition == nullptr) {
                        return fallback;
                    }
                    const char* colon = std::strchr(keyPosition, ':');
                    if (colon == nullptr) {
                        return fallback;
                    }
                    while (*++colon == ' ' || *colon == '\t') {
                    }
                    if (std::strncmp(colon, "true", 4) == 0) {
                        return true;
                    }
                    if (std::strncmp(colon, "false", 5) == 0) {
                        return false;
                    }
                    return fallback;
                };
                auto readInt = [&](const char* key, int fallback) {
                    const char* keyPosition = std::strstr(json.c_str(), key);
                    if (keyPosition == nullptr) {
                        return fallback;
                    }
                    const char* colon = std::strchr(keyPosition, ':');
                    if (colon == nullptr) {
                        return fallback;
                    }
                    char* end = nullptr;
                    const long value = std::strtol(colon + 1, &end, 10);
                    return end != colon + 1 ? static_cast<int>(value) : fallback;
                };
                auto readFloat = [&](const char* key, float fallback) {
                    const char* keyPosition = std::strstr(json.c_str(), key);
                    if (keyPosition == nullptr) {
                        return fallback;
                    }
                    const char* colon = std::strchr(keyPosition, ':');
                    if (colon == nullptr) {
                        return fallback;
                    }
                    char* end = nullptr;
                    const float value = std::strtof(colon + 1, &end);
                    return end != colon + 1 ? value : fallback;
                };

                const int configVersion = readInt("\"configVersion\"", 0);
                int keyboardKeyA = readInt("\"keyboardKeyA\"", kDefaultKeyboardKeyA);
                int keyboardKeyB = readInt("\"keyboardKeyB\"", kDefaultKeyboardKeyB);
                if (configVersion < kZoomConfigVersion &&
                    ((keyboardKeyA == kLegacyDefaultKeyboardKeyA && keyboardKeyB == kLegacyDefaultKeyboardKeyB) ||
                     (keyboardKeyA == kDevelopmentDefaultKeyboardKeyA && keyboardKeyB == kDevelopmentDefaultKeyboardKeyB))) {
                    keyboardKeyA = kDefaultKeyboardKeyA;
                    keyboardKeyB = kDefaultKeyboardKeyB;
                    migratedConfig = true;
                }

                std::uint32_t controllerMask = static_cast<std::uint32_t>(readInt("\"controllerMask\"", kDefaultControllerMask));
                if (configVersion < kZoomConfigVersion && controllerMask == kLegacyDefaultControllerMask) {
                    controllerMask = kDefaultControllerMask;
                    migratedConfig = true;
                }

                g_zoomEnabled.store(readBool("\"enabled\"", false), std::memory_order_relaxed);
                g_keyboardKeyA.store(keyboardKeyA, std::memory_order_relaxed);
                g_keyboardKeyB.store(keyboardKeyB, std::memory_order_relaxed);
                g_controllerMask.store(controllerMask, std::memory_order_relaxed);
                g_zoomAmount.store(ClampZoomAmount(readFloat("\"amount\"", kDefaultZoomAmount)), std::memory_order_relaxed);
                g_smoothZoomAnimationEnabled.store(readBool("\"smoothAnimation\"", false), std::memory_order_relaxed);
            }
        }
        CloseHandle(file);
    }

    UpdateKeyboardComboLabel();
    UpdateControllerComboLabel();
    if (migratedConfig) {
        SaveZoomConfig();
    }
}

void EnsureZoomConfigLoaded() {
    if (!g_configLoaded.load(std::memory_order_acquire)) {
        LoadZoomConfig();
    }
}

void SaveZoomConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildZoomConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[448]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"configVersion\": %d,\n"
        "  \"enabled\": %s,\n"
        "  \"keyboardKeyA\": %d,\n"
        "  \"keyboardKeyB\": %d,\n"
        "  \"controllerMask\": %u,\n"
        "  \"amount\": %.3f,\n"
        "  \"smoothAnimation\": %s\n"
        "}\n",
        kZoomConfigVersion,
        g_zoomEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_keyboardKeyA.load(std::memory_order_relaxed),
        g_keyboardKeyB.load(std::memory_order_relaxed),
        g_controllerMask.load(std::memory_order_relaxed),
        static_cast<double>(g_zoomAmount.load(std::memory_order_relaxed)),
        g_smoothZoomAnimationEnabled.load(std::memory_order_relaxed) ? "true" : "false");

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
    constexpr const wchar_t* kDllNames[] = {
        L"xinput1_4.dll",
        L"xinput1_3.dll",
        L"xinput9_1_0.dll",
    };

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
    if ((gamepad.buttons & 0x0040) != 0) flags |= kControllerLeftThumb;
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
    if (!controllerPresent &&
        lastPoll != 0 &&
        now - lastPoll < kDisconnectedXInputPollIntervalMs) {
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
    if (lastReadTick != 0 &&
        now - lastReadTick < kControllerPollIntervalMs) {
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
    if (keyA <= 0) {
        return false;
    }

    return IsVirtualKeyDown(keyA) && (keyB <= 0 || IsVirtualKeyDown(keyB));
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
        SaveZoomConfig();
    }
}

bool IsWritableAddress(void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr || VirtualQuery(address, &info, sizeof(info)) == 0) {
        return false;
    }

    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    const auto regionStart = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto regionEnd = regionStart + info.RegionSize;
    if (end < start || end > regionEnd) {
        return false;
    }

    const DWORD protect = info.Protect & 0xFF;
    return protect == PAGE_READWRITE ||
        protect == PAGE_WRITECOPY ||
        protect == PAGE_EXECUTE_READWRITE ||
        protect == PAGE_EXECUTE_WRITECOPY;
}

bool IsExecutableAddress(void* address) {
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr || VirtualQuery(address, &info, sizeof(info)) == 0) {
        return false;
    }

    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const DWORD protect = info.Protect & 0xFF;
    return protect == PAGE_EXECUTE ||
        protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE ||
        protect == PAGE_EXECUTE_WRITECOPY;
}

bool IsValidFov(float fov) {
    return std::isfinite(fov) &&
        fov >= tane::offsets::camera::kMinZoomFovRadians &&
        fov <= tane::offsets::camera::kMaxZoomFovRadians;
}

bool IsValidFovDegrees(float fov) {
    return std::isfinite(fov) && fov > 0.0f && fov < 180.0f;
}

float GetTargetZoomAmount() {
    if (g_zoomInputMode.load(std::memory_order_relaxed) == 2) {
        return kMaxZoomAmount;
    }
    if (g_zoomInputMode.load(std::memory_order_relaxed) == 1) {
        return ClampZoomAmount(g_keyboardSessionZoomAmount.load(std::memory_order_relaxed));
    }
    return ClampZoomAmount(g_zoomAmount.load(std::memory_order_relaxed));
}

void StartKeyboardZoomSession() {
    g_keyboardSessionZoomAmount.store(ClampZoomAmount(g_zoomAmount.load(std::memory_order_relaxed)), std::memory_order_relaxed);
    g_keyboardSessionZoomInitialized.store(true, std::memory_order_relaxed);
}

void EndKeyboardZoomSession() {
    g_keyboardSessionZoomInitialized.store(false, std::memory_order_relaxed);
}

std::uint64_t GetZoomAnimationCounter() {
    LARGE_INTEGER counter{};
    return QueryPerformanceCounter(&counter) ? static_cast<std::uint64_t>(counter.QuadPart) : 0;
}

float GetZoomAnimationDeltaSeconds(std::uint64_t previousCounter, std::uint64_t currentCounter) {
    if (previousCounter == 0 || currentCounter <= previousCounter) {
        return 1.0f / 60.0f;
    }

    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value;
    }();
    if (frequency.QuadPart <= 0) {
        return 1.0f / 60.0f;
    }

    const double seconds = static_cast<double>(currentCounter - previousCounter) / static_cast<double>(frequency.QuadPart);
    return std::clamp(static_cast<float>(seconds), 0.0f, 0.05f);
}

float GetWiiUZoomLerpFactor(float deltaSeconds) {
    return std::clamp(1.0f - std::pow(1.0f - kZoomAnimationLerpFactor, deltaSeconds * 60.0f), 0.0f, 1.0f);
}

void ResetEffectiveZoomAmount() {
    g_effectiveZoomAmount.store(1.0f, std::memory_order_relaxed);
    g_zoomFovMultiplier.store(1.0f, std::memory_order_relaxed);
    g_lastZoomAnimationCounter.store(0, std::memory_order_relaxed);
}

void UpdateEffectiveZoomAmount() {
    if (!g_zoomEnabled.load(std::memory_order_relaxed) || !tane::gui::CanRunGameplayModules()) {
        ResetEffectiveZoomAmount();
        return;
    }

    const float target = g_zoomHeld.load(std::memory_order_relaxed) ? GetTargetZoomAmount() : 1.0f;
    if (!g_smoothZoomAnimationEnabled.load(std::memory_order_relaxed)) {
        g_effectiveZoomAmount.store(target, std::memory_order_relaxed);
        g_zoomFovMultiplier.store(1.0f / std::max(target, 0.001f), std::memory_order_relaxed);
        g_lastZoomAnimationCounter.store(0, std::memory_order_relaxed);
        return;
    }

    const std::uint64_t now = GetZoomAnimationCounter();
    const std::uint64_t previous = g_lastZoomAnimationCounter.exchange(now, std::memory_order_relaxed);
    const float lerpFactor = GetWiiUZoomLerpFactor(GetZoomAnimationDeltaSeconds(previous, now));
    const float targetMultiplier = 1.0f / std::max(target, 0.001f);
    float currentMultiplier = std::clamp(g_zoomFovMultiplier.load(std::memory_order_relaxed), 1.0f / kMaxZoomAmount, 1.0f);
    currentMultiplier += (targetMultiplier - currentMultiplier) * lerpFactor;
    if (std::fabs(currentMultiplier - targetMultiplier) < 0.001f) {
        currentMultiplier = targetMultiplier;
    }
    currentMultiplier = std::clamp(currentMultiplier, 1.0f / kMaxZoomAmount, 1.0f);
    g_zoomFovMultiplier.store(currentMultiplier, std::memory_order_relaxed);
    g_effectiveZoomAmount.store(std::clamp(1.0f / std::max(currentMultiplier, 0.001f), 1.0f, kMaxZoomAmount), std::memory_order_relaxed);
}

float GetEffectiveZoomAmount() {
    if (!g_zoomEnabled.load(std::memory_order_relaxed) || !tane::gui::CanRunGameplayModules()) {
        return 1.0f;
    }

    if (!g_smoothZoomAnimationEnabled.load(std::memory_order_relaxed)) {
        return g_zoomHeld.load(std::memory_order_relaxed) ? GetTargetZoomAmount() : 1.0f;
    }

    return std::clamp(g_effectiveZoomAmount.load(std::memory_order_relaxed), 1.0f, kMaxZoomAmount);
}

bool ShouldApplyZoomAmount(float amount) {
    return amount > 1.0005f;
}

void ApplyZoomToCameraOutput(void* cameraOutput) {
    const float amount = GetEffectiveZoomAmount();
    if (!ShouldApplyZoomAmount(amount)) {
        return;
    }

    auto* fov = reinterpret_cast<float*>(
        reinterpret_cast<std::uint8_t*>(cameraOutput) + tane::offsets::camera::kCameraOutputFovOffset);
    if (!IsWritableAddress(fov, sizeof(float))) {
        return;
    }

    const float originalFov = *fov;
    if (!IsValidFov(originalFov)) {
        return;
    }

    const float zoomedFov = std::clamp(
        originalFov / amount,
        tane::offsets::camera::kMinZoomFovRadians,
        tane::offsets::camera::kMaxZoomFovRadians);
    *fov = zoomedFov;
}

float ApplyZoomToFovDegrees(float originalFov) {
    const float amount = GetEffectiveZoomAmount();
    if (!ShouldApplyZoomAmount(amount)) {
        return originalFov;
    }

    if (!IsValidFovDegrees(originalFov)) {
        return originalFov;
    }

    constexpr float radiansToDegrees = 57.2957795f;
    const float minimumFovDegrees = tane::offsets::camera::kMinZoomFovRadians * radiansToDegrees;
    const float zoomedFov = std::clamp(originalFov / amount, minimumFovDegrees, 179.0f);
    return zoomedFov;
}

float __fastcall HookCameraProjectionFovOption(void* options, float frameAlpha, int applyViewportScale) {
    const float originalFov = g_originalCameraProjectionFovOption != nullptr
        ? g_originalCameraProjectionFovOption(options, frameAlpha, applyViewportScale)
        : 70.0f;
    return ApplyZoomToFovDegrees(originalFov);
}

void __fastcall HookCameraDefaultFovApply(void* cameraOutput, const float* sourceFov) {
    if (g_originalCameraDefaultFovApply != nullptr) {
        g_originalCameraDefaultFovApply(cameraOutput, sourceFov);
    }
    ApplyZoomToCameraOutput(cameraOutput);
}

void __fastcall HookCameraCustomFovApply(
    void* cameraContext,
    void* cameraOutput,
    const float* sourceFov,
    void* customFovComponent,
    void* cameraEntityContext,
    float deltaTime) {
    if (g_originalCameraCustomFovApply != nullptr) {
        g_originalCameraCustomFovApply(
            cameraContext,
            cameraOutput,
            sourceFov,
            customFovComponent,
            cameraEntityContext,
            deltaTime);
    }
    ApplyZoomToCameraOutput(cameraOutput);
}

bool InstallHook(HMODULE module, std::uintptr_t rva, void* detour, void** original) {
    auto* target = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module) + rva);
    if (!IsExecutableAddress(target)) {
        return false;
    }

    const MH_STATUS createStatus = MH_CreateHook(target, detour, original);
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    return enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED;
}

void AdjustKeyboardSessionZoomAmountByWheel(short wheelDelta) {
    if (wheelDelta == 0) {
        return;
    }

    if (!g_keyboardSessionZoomInitialized.load(std::memory_order_relaxed)) {
        StartKeyboardZoomSession();
    }

    const float direction = wheelDelta > 0 ? kWheelZoomStep : -kWheelZoomStep;
    const float updated = ClampZoomAmount(g_keyboardSessionZoomAmount.load(std::memory_order_relaxed) + direction);
    g_keyboardSessionZoomAmount.store(updated, std::memory_order_relaxed);
}

}  // namespace

bool IsZoomEnabled() {
    EnsureZoomConfigLoaded();
    return g_zoomEnabled.load(std::memory_order_relaxed);
}

void SetZoomEnabled(bool enabled) {
    EnsureZoomConfigLoaded();
    g_zoomEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        g_zoomHeld.store(false, std::memory_order_relaxed);
        g_zoomInputMode.store(0, std::memory_order_relaxed);
        EndKeyboardZoomSession();
        ResetEffectiveZoomAmount();
    }
    SaveZoomConfig();
}

float GetZoomAmount() {
    EnsureZoomConfigLoaded();
    return g_zoomAmount.load(std::memory_order_relaxed);
}

void SetZoomAmount(float value) {
    EnsureZoomConfigLoaded();
    g_zoomAmount.store(ClampZoomAmount(value), std::memory_order_relaxed);
    if (!g_keyboardSessionZoomInitialized.load(std::memory_order_relaxed)) {
        g_keyboardSessionZoomAmount.store(g_zoomAmount.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    SaveZoomConfig();
}

bool IsZoomSmoothAnimationEnabled() {
    EnsureZoomConfigLoaded();
    return g_smoothZoomAnimationEnabled.load(std::memory_order_relaxed);
}

void SetZoomSmoothAnimationEnabled(bool enabled) {
    EnsureZoomConfigLoaded();
    g_smoothZoomAnimationEnabled.store(enabled, std::memory_order_relaxed);
    ResetEffectiveZoomAmount();
    SaveZoomConfig();
}

float GetZoomMinAmount() {
    return kMinZoomAmount;
}

float GetZoomMaxAmount() {
    return kMaxZoomAmount;
}

const char* GetZoomKeyboardComboLabel() {
    EnsureZoomConfigLoaded();
    UpdateKeyboardComboLabel();
    return g_keyboardComboLabel;
}

const char* GetZoomControllerComboLabel() {
    EnsureZoomConfigLoaded();
    UpdateControllerComboLabel();
    return g_controllerComboLabel;
}

bool IsZoomKeyboardComboCaptureActive() {
    return g_keyboardCaptureActive;
}

bool IsZoomControllerComboCaptureActive() {
    return g_controllerCaptureActive;
}

void BeginZoomKeyboardComboCapture() {
    EnsureZoomConfigLoaded();
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_keyboardCaptureWaitingForRelease = true;
    g_keyboardCaptureActive = true;
    g_controllerCaptureActive = false;
    g_controllerCaptureCandidateMask = 0;
}

void BeginZoomControllerComboCapture() {
    EnsureZoomConfigLoaded();
    g_controllerCaptureActive = true;
    g_controllerCaptureWaitingForRelease = true;
    g_controllerCaptureCandidateMask = 0;
    g_keyboardCaptureActive = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
}

void CancelZoomComboCapture() {
    g_keyboardCaptureActive = false;
    g_keyboardCaptureWaitingForRelease = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_controllerCaptureActive = false;
    g_controllerCaptureWaitingForRelease = false;
    g_controllerCaptureCandidateMask = 0;
}

bool HandleZoomKeyMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    EnsureZoomConfigLoaded();
    const bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool keyUp = message == WM_KEYUP || message == WM_SYSKEYUP;
    const bool mouseDown = message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK;
    const bool mouseUp = message == WM_XBUTTONUP;

    if (g_keyboardCaptureActive) {
        if (!keyDown && !keyUp && !mouseDown && !mouseUp) {
            return false;
        }

        const bool firstPress = mouseDown || (lParam & (1u << 30)) == 0;
        const int resolvedVirtualKey = (keyDown || keyUp)
            ? ResolveMessageVirtualKey(wParam, lParam)
            : ResolveMouseButtonVirtualKey(message, wParam);
        if (keyDown && firstPress && resolvedVirtualKey == VK_ESCAPE) {
            CancelZoomComboCapture();
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

    if (message == WM_MOUSEWHEEL &&
        g_zoomEnabled.load(std::memory_order_relaxed) &&
        tane::gui::CanRunGameplayModules() &&
        IsKeyboardComboHeld()) {
        AdjustKeyboardSessionZoomAmountByWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return true;
    }

    return false;
}

bool IsZoomControllerComboPolling() {
    return g_controllerCaptureActive;
}

void TickZoomComboCapture() {
    EnsureZoomConfigLoaded();
    TickControllerComboCapture();
}

void TickZoom(void* clientInstance) {
    (void)clientInstance;
    EnsureZoomConfigLoaded();
    TickControllerComboCapture();

    if (!g_zoomEnabled.load(std::memory_order_relaxed) ||
        !tane::gui::CanRunGameplayModules()) {
        g_zoomHeld.store(false, std::memory_order_relaxed);
        g_zoomInputMode.store(0, std::memory_order_relaxed);
        EndKeyboardZoomSession();
        ResetEffectiveZoomAmount();
        return;
    }

    const std::uint32_t controllerFlags = ReadControllerFlags();
    const bool keyboardHeld = IsKeyboardComboHeld();
    const bool controllerHeld = IsControllerComboHeld(controllerFlags);
    const bool zoomHeld = keyboardHeld || controllerHeld;
    const int previousInputMode = g_zoomInputMode.load(std::memory_order_relaxed);
    if (keyboardHeld && !controllerHeld && previousInputMode != 1 &&
        !g_keyboardSessionZoomInitialized.load(std::memory_order_relaxed)) {
        StartKeyboardZoomSession();
    }
    if (!keyboardHeld || controllerHeld) {
        EndKeyboardZoomSession();
    }
    g_zoomHeld.store(zoomHeld, std::memory_order_relaxed);
    g_zoomInputMode.store(controllerHeld ? 2 : (keyboardHeld ? 1 : 0), std::memory_order_relaxed);
    UpdateEffectiveZoomAmount();
}

bool InstallZoomHooks() {
    bool expected = false;
    if (!g_hooksInstalled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return true;
    }

    EnsureZoomConfigLoaded();
    HMODULE module = GetModuleHandleW(L"Minecraft.Windows.exe");
    if (module == nullptr) {
        module = GetModuleHandleW(nullptr);
    }
    if (module == nullptr) {
        return false;
    }

    const bool projectionFovHookInstalled = InstallHook(
        module,
        tane::offsets::camera::kCameraProjectionFovOptionRva,
        reinterpret_cast<void*>(&HookCameraProjectionFovOption),
        reinterpret_cast<void**>(&g_originalCameraProjectionFovOption));
    const bool defaultHookInstalled = InstallHook(
        module,
        tane::offsets::camera::kCameraDefaultFovApplyRva,
        reinterpret_cast<void*>(&HookCameraDefaultFovApply),
        reinterpret_cast<void**>(&g_originalCameraDefaultFovApply));
    const bool customHookInstalled = InstallHook(
        module,
        tane::offsets::camera::kCameraCustomFovApplyRva,
        reinterpret_cast<void*>(&HookCameraCustomFovApply),
        reinterpret_cast<void**>(&g_originalCameraCustomFovApply));

    return projectionFovHookInstalled || defaultHookInstalled || customHookInstalled;
}

}  // namespace tane::camera
