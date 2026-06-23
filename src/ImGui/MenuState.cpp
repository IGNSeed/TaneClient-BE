#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace tane::gui {

bool IsGuiPositionEditorActive();

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

constexpr WORD kXInputGamepadRightThumb = 0x0080;
constexpr WORD kXInputGamepadLeftShoulder = 0x0100;
constexpr WORD kXInputGamepadRightShoulder = 0x0200;
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
constexpr std::uint32_t kDefaultControllerCombo =
    kControllerLeftShoulder | kControllerRightShoulder | kControllerRightThumb;

std::atomic_bool g_menuOpen = false;
std::atomic_bool g_controllerTogglePolling = false;
bool g_previousControllerComboHeld = false;
bool g_triedResolveXInput = false;
XInputGetStateFn g_xInputGetState = nullptr;
bool g_keyComboConfigLoaded = false;
bool g_keyboardCaptureActive = false;
bool g_controllerCaptureActive = false;
bool g_controllerCaptureWaitingForRelease = false;
std::uint32_t g_controllerCaptureCandidateMask = 0;
std::uint32_t g_positionEditorPreviousControllerFlags = 0;
DWORD64 g_positionEditorDpadHoldStartMs = 0;
constexpr int kVirtualKeyCount = 256;
bool g_keyboardComboKeys[kVirtualKeyCount] = {};
bool g_keyboardCaptureCandidateKeys[kVirtualKeyCount] = {};
bool g_keyboardCaptureDownKeys[kVirtualKeyCount] = {};
bool g_keyboardRuntimeDownKeys[kVirtualKeyCount] = {};
std::uint32_t g_controllerComboMask = kDefaultControllerCombo;
char g_keyboardComboLabel[96] = "Insert";
char g_controllerComboLabel[160] = "LB + RB + RStick";

#include "MenuStateConfig.cpp"

#include "MenuStateInput.cpp"

bool IsMenuOpen() {
    EnsureKeyComboConfigLoaded();
    return g_menuOpen.load(std::memory_order_relaxed);
}

bool HandleMenuKeyMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    EnsureKeyComboConfigLoaded();
    const bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool keyUp = message == WM_KEYUP || message == WM_SYSKEYUP;
    const bool firstPress = (lParam & (1u << 30)) == 0;
    const int resolvedVirtualKey = (keyDown || keyUp) ? ResolveMessageVirtualKey(wParam, lParam) : 0;
    if (keyDown || keyUp) {
        UpdateRuntimeKeyboardKey(resolvedVirtualKey, keyDown);
    }

    if ((keyDown || keyUp) && g_keyboardCaptureActive) {
        if (keyDown && firstPress && resolvedVirtualKey == VK_ESCAPE) {
            g_keyboardCaptureActive = false;
            ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
            ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
            return true;
        }

        if (keyDown && firstPress) {
            if (resolvedVirtualKey > 0) {
                g_keyboardCaptureCandidateKeys[resolvedVirtualKey] = true;
                g_keyboardCaptureDownKeys[resolvedVirtualKey] = true;
            }
        } else if (keyUp) {
            if (resolvedVirtualKey > 0) {
                g_keyboardCaptureDownKeys[resolvedVirtualKey] = false;
            }
            if (HasKeyboardKeySet(g_keyboardCaptureCandidateKeys) && !HasKeyboardCaptureDownKeys()) {
                CaptureKeyboardCombo(static_cast<WPARAM>(resolvedVirtualKey));
            }
        }
        return true;
    }

    if (keyDown && firstPress && IsKeyboardComboHeldByRuntimeState(resolvedVirtualKey)) {
        if (IsGuiPositionEditorActive()) {
            return true;
        }
        ToggleMenu();
        return true;
    }

    return false;
}

bool IsControllerTogglePolling() {
    return g_controllerTogglePolling.load(std::memory_order_relaxed);
}

void SetControllerOverlayInputPolling(bool polling) {
    g_controllerTogglePolling.store(polling, std::memory_order_release);
}

void ResetPositionEditorControllerInputState() {
    g_positionEditorPreviousControllerFlags = 0;
    g_positionEditorDpadHoldStartMs = 0;
}

bool ReadPositionEditorControllerInput(float& moveX, float& moveY, float& resize, bool& acceptPressed, bool& cancelPressed) {
    EnsureKeyComboConfigLoaded();
    moveX = 0.0f;
    moveY = 0.0f;
    resize = 0.0f;
    acceptPressed = false;
    cancelPressed = false;

    g_controllerTogglePolling.store(true, std::memory_order_release);
    std::uint32_t xInputFlags = 0;
    std::uint32_t virtualFlags = 0;
    ReadXInputPositionEditorState(moveX, moveY, resize, xInputFlags);
    ReadVirtualGamepadFlags(virtualFlags);
    const std::uint32_t flags = xInputFlags | virtualFlags;
    const bool dpadHeld = (flags & (kControllerDpadLeft | kControllerDpadRight | kControllerDpadUp | kControllerDpadDown)) != 0;
    float dpadMultiplier = 1.0f;
    if (dpadHeld) {
        const DWORD64 now = GetTickCount64();
        if (g_positionEditorDpadHoldStartMs == 0) {
            g_positionEditorDpadHoldStartMs = now;
        }
        const float heldSeconds = static_cast<float>(now - g_positionEditorDpadHoldStartMs) * 0.001f;
        dpadMultiplier = 1.0f + std::min(5.5f, heldSeconds * heldSeconds * 2.2f);
    } else {
        g_positionEditorDpadHoldStartMs = 0;
    }

    if ((flags & kControllerDpadLeft) != 0) {
        moveX -= dpadMultiplier;
    }
    if ((flags & kControllerDpadRight) != 0) {
        moveX += dpadMultiplier;
    }
    if ((flags & kControllerDpadUp) != 0) {
        moveY -= dpadMultiplier;
    }
    if ((flags & kControllerDpadDown) != 0) {
        moveY += dpadMultiplier;
    }
    if ((flags & kControllerLeftTrigger) != 0) {
        resize = std::min(resize, -1.0f);
    }
    if ((flags & kControllerRightTrigger) != 0) {
        resize = std::max(resize, 1.0f);
    }

    const std::uint32_t newlyPressed = flags & ~g_positionEditorPreviousControllerFlags;
    acceptPressed = (newlyPressed & kControllerA) != 0;
    cancelPressed = (newlyPressed & kControllerB) != 0 || (newlyPressed & kControllerBack) != 0;
    g_positionEditorPreviousControllerFlags = flags;
    g_controllerTogglePolling.store(false, std::memory_order_release);
    return moveX != 0.0f || moveY != 0.0f || resize != 0.0f || acceptPressed || cancelPressed;
}

void UpdateControllerMenuToggle() {
    EnsureKeyComboConfigLoaded();
    g_controllerTogglePolling.store(true, std::memory_order_release);
    std::uint32_t xInputFlags = 0;
    std::uint32_t virtualFlags = 0;
    const bool hasXInputFlags = ReadXInputControllerFlags(xInputFlags);
    const bool hasVirtualFlags = ReadVirtualGamepadFlags(virtualFlags);
    const std::uint32_t controllerFlags = xInputFlags | virtualFlags;
    const bool hasControllerFlags = hasXInputFlags || hasVirtualFlags;
    if (g_controllerCaptureActive) {
        if (g_controllerCaptureWaitingForRelease) {
            if (!hasControllerFlags || controllerFlags == 0) {
                g_controllerCaptureWaitingForRelease = false;
                g_controllerCaptureCandidateMask = 0;
            }
        } else if (hasControllerFlags && controllerFlags != 0) {
            g_controllerCaptureCandidateMask |= controllerFlags;
        } else if (g_controllerCaptureCandidateMask != 0) {
            g_controllerComboMask = g_controllerCaptureCandidateMask;
            g_controllerCaptureCandidateMask = 0;
            g_controllerCaptureActive = false;
            g_previousControllerComboHeld = true;
            UpdateControllerComboLabel();
            SaveKeyComboConfig();
        }
        g_controllerTogglePolling.store(false, std::memory_order_release);
        return;
    }

    const bool comboHeld =
        (hasControllerFlags && g_controllerComboMask != 0 && (controllerFlags & g_controllerComboMask) == g_controllerComboMask) ||
        IsVirtualGamepadComboHeld(g_controllerComboMask);
    g_controllerTogglePolling.store(false, std::memory_order_release);
    if (IsGuiPositionEditorActive()) {
        g_previousControllerComboHeld = comboHeld;
        return;
    }
    if (comboHeld && !g_previousControllerComboHeld) {
        ToggleMenu();
    }

    g_previousControllerComboHeld = comboHeld;
}

const char* GetKeyboardToggleComboLabel() {
    EnsureKeyComboConfigLoaded();
    return g_keyboardComboLabel;
}

const char* GetControllerToggleComboLabel() {
    EnsureKeyComboConfigLoaded();
    return g_controllerComboLabel;
}

bool IsKeyboardToggleCaptureActive() {
    return g_keyboardCaptureActive;
}

bool IsControllerToggleCaptureActive() {
    return g_controllerCaptureActive;
}

void BeginKeyboardToggleCapture() {
    EnsureKeyComboConfigLoaded();
    g_keyboardCaptureActive = true;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_controllerCaptureActive = false;
    g_controllerCaptureCandidateMask = 0;
}

void BeginControllerToggleCapture() {
    EnsureKeyComboConfigLoaded();
    g_controllerCaptureActive = true;
    g_controllerCaptureWaitingForRelease = true;
    g_controllerCaptureCandidateMask = 0;
    g_keyboardCaptureActive = false;
}

void CancelToggleComboCapture() {
    g_keyboardCaptureActive = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_controllerCaptureActive = false;
    g_controllerCaptureWaitingForRelease = false;
    g_controllerCaptureCandidateMask = 0;
}

void ResetToggleComboConfig() {
    SetDefaultKeyboardCombo();
    g_controllerComboMask = kDefaultControllerCombo;
    g_keyboardCaptureActive = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_controllerCaptureActive = false;
    g_controllerCaptureWaitingForRelease = false;
    g_controllerCaptureCandidateMask = 0;
    UpdateKeyboardComboLabel();
    UpdateControllerComboLabel();
    SaveKeyComboConfig();
}

}  // namespace tane::gui
