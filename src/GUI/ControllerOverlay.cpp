#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace tane::gui {
bool IsGuiPositionEditorActive();
bool ShouldShowGuiOverlay();
bool IsControllerTogglePolling();
void SetControllerOverlayInputPolling(bool polling);
}

namespace tane::gui {
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

constexpr float kMinControllerOverlayScale = 0.55f;
constexpr float kMaxControllerOverlayScale = 3.0f;
constexpr float kDefaultControllerOverlayScale = 1.0f;
constexpr float kDefaultControllerOverlayMarginX = 24.0f;
constexpr float kDefaultControllerOverlayMarginY = 30.0f;
constexpr float kBaseRadius = 38.0f;
constexpr float kKnobRadius = 12.0f;
constexpr float kStickGap = 72.0f;
constexpr float kMinVisualDeadZone = 0.0f;
constexpr float kMaxVisualDeadZone = 0.80f;
constexpr float kDefaultLeftStickVisualDeadZone = 0.0f;
constexpr float kDefaultRightStickVisualDeadZone = 0.0f;

std::atomic_bool g_controllerOverlayEnabled = false;
std::atomic_bool g_controllerOverlayConfigLoaded = false;
std::atomic_bool g_controllerOverlayCustomPosition = false;
std::atomic_bool g_controllerOverlayPositionDirty = false;
std::atomic<float> g_controllerOverlayPositionX = 0.0f;
std::atomic<float> g_controllerOverlayPositionY = 0.0f;
std::atomic<float> g_controllerOverlayScale = kDefaultControllerOverlayScale;
std::atomic<float> g_leftStickVisualDeadZone = kDefaultLeftStickVisualDeadZone;
std::atomic<float> g_rightStickVisualDeadZone = kDefaultRightStickVisualDeadZone;

bool g_triedResolveXInput = false;
XInputGetStateFn g_xInputGetState = nullptr;

class ControllerOverlayInputPollingGuard {
public:
    ControllerOverlayInputPollingGuard() {
        wasPolling_ = IsControllerTogglePolling();
        SetControllerOverlayInputPolling(true);
    }

    ~ControllerOverlayInputPollingGuard() {
        SetControllerOverlayInputPolling(wasPolling_);
    }

    ControllerOverlayInputPollingGuard(const ControllerOverlayInputPollingGuard&) = delete;
    ControllerOverlayInputPollingGuard& operator=(const ControllerOverlayInputPollingGuard&) = delete;

private:
    bool wasPolling_ = false;
};

bool BuildControllerOverlayConfigPath(wchar_t* path, DWORD pathCount) {
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

    wchar_t guiPath[MAX_PATH]{};
    if (swprintf_s(guiPath, L"%s\\GUI", configPath) < 0) {
        return false;
    }
    CreateDirectoryW(guiPath, nullptr);

    return swprintf_s(path, pathCount, L"%s\\ControllerOverlay.json", guiPath) >= 0;
}

bool ParseFloatAfter(const char* section, const char* key, float& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    return std::sscanf(found + 1, " %f", &value) == 1;
}

bool ParseBoolAfter(const char* section, const char* key, bool& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    while (*++found == ' ' || *found == '\t') {
    }

    if (std::strncmp(found, "true", 4) == 0) {
        value = true;
        return true;
    }
    if (std::strncmp(found, "false", 5) == 0) {
        value = false;
        return true;
    }

    return false;
}

void SaveControllerOverlayConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildControllerOverlayConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[384]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 1,\n"
        "  \"enabled\": %s,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f,\n"
        "  \"left_deadzone\": %.3f,\n"
        "  \"right_deadzone\": %.3f\n"
        "}\n",
        g_controllerOverlayEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_controllerOverlayCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_controllerOverlayPositionX.load(std::memory_order_relaxed),
        g_controllerOverlayPositionY.load(std::memory_order_relaxed),
        g_controllerOverlayScale.load(std::memory_order_relaxed),
        g_leftStickVisualDeadZone.load(std::memory_order_relaxed),
        g_rightStickVisualDeadZone.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_controllerOverlayPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsureControllerOverlayConfigLoaded() {
    bool expected = false;
    if (!g_controllerOverlayConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildControllerOverlayConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SaveControllerOverlayConfig();
        return;
    }

    bool shouldRewriteConfig = false;
    char json[1024]{};
    DWORD read = 0;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';
        bool enabled = false;
        bool custom = false;
        float x = 0.0f;
        float y = 0.0f;
        float scale = kDefaultControllerOverlayScale;
        float leftDeadZone = kDefaultLeftStickVisualDeadZone;
        float rightDeadZone = kDefaultRightStickVisualDeadZone;

        if (!ParseBoolAfter(json, "\"enabled\"", enabled)) {
            shouldRewriteConfig = true;
        }
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);
        if (!ParseFloatAfter(json, "\"left_deadzone\"", leftDeadZone) ||
            !ParseFloatAfter(json, "\"right_deadzone\"", rightDeadZone)) {
            shouldRewriteConfig = true;
        }

        g_controllerOverlayEnabled.store(enabled, std::memory_order_relaxed);
        g_controllerOverlayCustomPosition.store(custom, std::memory_order_relaxed);
        g_controllerOverlayPositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_controllerOverlayPositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_controllerOverlayScale.store(std::clamp(scale, kMinControllerOverlayScale, kMaxControllerOverlayScale), std::memory_order_relaxed);
        g_leftStickVisualDeadZone.store(std::clamp(leftDeadZone, kMinVisualDeadZone, kMaxVisualDeadZone), std::memory_order_relaxed);
        g_rightStickVisualDeadZone.store(std::clamp(rightDeadZone, kMinVisualDeadZone, kMaxVisualDeadZone), std::memory_order_relaxed);
    }
    CloseHandle(file);
    if (shouldRewriteConfig) {
        SaveControllerOverlayConfig();
    }
}

float GetClampedControllerOverlayScale() {
    return std::clamp(
        g_controllerOverlayScale.load(std::memory_order_relaxed),
        kMinControllerOverlayScale,
        kMaxControllerOverlayScale);
}

ImVec2 GetControllerOverlayRectSize(float scale) {
    return ImVec2(
        (kBaseRadius * 4.0f + kStickGap) * scale,
        (kBaseRadius * 2.0f) * scale);
}

ImVec2 ClampControllerOverlayPosition(const ImVec2& position, const ImVec2& displaySize) {
    const ImVec2 rectSize = GetControllerOverlayRectSize(GetClampedControllerOverlayScale());
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, displaySize.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, displaySize.y - rectSize.y)));
}

ImVec2 GetDefaultControllerOverlayPosition(const ImVec2& displaySize, float scale) {
    const ImVec2 size = GetControllerOverlayRectSize(scale);
    return ClampControllerOverlayPosition(ImVec2(
        displaySize.x - size.x - kDefaultControllerOverlayMarginX,
        displaySize.y - size.y - kDefaultControllerOverlayMarginY), displaySize);
}

ImVec2 GetEffectiveControllerOverlayPosition(const ImVec2& displaySize) {
    EnsureControllerOverlayConfigLoaded();
    const float scale = GetClampedControllerOverlayScale();
    if (!g_controllerOverlayCustomPosition.load(std::memory_order_relaxed)) {
        return GetDefaultControllerOverlayPosition(displaySize, scale);
    }

    return ClampControllerOverlayPosition(
        ImVec2(
            g_controllerOverlayPositionX.load(std::memory_order_relaxed),
            g_controllerOverlayPositionY.load(std::memory_order_relaxed)),
        displaySize);
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

float NormalizeThumb(SHORT value) {
    if (value < 0) {
        return std::clamp(static_cast<float>(value) / 32768.0f, -1.0f, 0.0f);
    }
    return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
}

void ApplyStickDeadZone(float& x, float& y, float deadZone) {
    deadZone = std::clamp(deadZone, kMinVisualDeadZone, kMaxVisualDeadZone);
    const float length = std::sqrt(x * x + y * y);
    if (length <= deadZone || length <= 0.0001f) {
        x = 0.0f;
        y = 0.0f;
        return;
    }

    const float normalizedLength = std::clamp((length - deadZone) / std::max(0.0001f, 1.0f - deadZone), 0.0f, 1.0f);
    const float scale = normalizedLength / length;
    x = std::clamp(x * scale, -1.0f, 1.0f);
    y = std::clamp(y * scale, -1.0f, 1.0f);
}

bool ReadControllerSticks(float& leftX, float& leftY, float& rightX, float& rightY) {
    XInputGetStateFn getState = ResolveXInputGetState();
    if (getState == nullptr) {
        return false;
    }

    ControllerOverlayInputPollingGuard pollingGuard;

    bool found = false;
    for (DWORD userIndex = 0; userIndex < 4; ++userIndex) {
        XInputState state{};
        if (getState(userIndex, &state) != ERROR_SUCCESS) {
            continue;
        }

        leftX = NormalizeThumb(state.gamepad.leftThumbX);
        leftY = NormalizeThumb(state.gamepad.leftThumbY);
        rightX = NormalizeThumb(state.gamepad.rightThumbX);
        rightY = NormalizeThumb(state.gamepad.rightThumbY);
        ApplyStickDeadZone(leftX, leftY, g_leftStickVisualDeadZone.load(std::memory_order_relaxed));
        ApplyStickDeadZone(rightX, rightY, g_rightStickVisualDeadZone.load(std::memory_order_relaxed));
        found = true;
        break;
    }

    return found;
}

void DrawStick(
    ImDrawList* drawList,
    const ImVec2& center,
    float x,
    float y,
    float scale,
    float alphaScale) {
    const int alpha = static_cast<int>(std::clamp(alphaScale, 0.0f, 1.0f) * 255.0f);
    const float baseRadius = kBaseRadius * scale;
    const float knobRadius = kKnobRadius * scale;
    const ImVec2 knob(center.x + x * baseRadius, center.y - y * baseRadius);
    const ImU32 baseColor = IM_COL32(0, 0, 0, static_cast<int>(150.0f * alpha / 255.0f));
    const ImU32 crossColor = IM_COL32(255, 255, 255, static_cast<int>(72.0f * alpha / 255.0f));
    const ImU32 outlineColor = IM_COL32(255, 255, 255, static_cast<int>(230.0f * alpha / 255.0f));
    const ImU32 knobColor = IM_COL32(255, 255, 255, static_cast<int>(245.0f * alpha / 255.0f));

    drawList->AddCircleFilled(center, baseRadius, baseColor, 48);
    drawList->AddCircle(center, baseRadius, outlineColor, 48, std::max(1.0f, 2.0f * scale));
    drawList->AddLine(
        ImVec2(center.x - baseRadius, center.y),
        ImVec2(center.x + baseRadius, center.y),
        crossColor,
        std::max(1.0f, 1.0f * scale));
    drawList->AddLine(
        ImVec2(center.x, center.y - baseRadius),
        ImVec2(center.x, center.y + baseRadius),
        crossColor,
        std::max(1.0f, 1.0f * scale));
    drawList->AddCircleFilled(knob, knobRadius, knobColor, 32);
    drawList->AddCircle(knob, knobRadius, outlineColor, 32, std::max(1.0f, 1.5f * scale));
}

void DrawControllerOverlayAt(ImDrawList* drawList, const ImVec2& position, float scale, bool useLiveInput, float alphaScale) {
    if (drawList == nullptr) {
        return;
    }

    float leftX = 0.0f;
    float leftY = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    if (useLiveInput) {
        ReadControllerSticks(leftX, leftY, rightX, rightY);
    }

    const float baseRadius = kBaseRadius * scale;
    const ImVec2 leftCenter(position.x + baseRadius, position.y + baseRadius);
    const ImVec2 rightCenter(leftCenter.x + (kBaseRadius * 2.0f + kStickGap) * scale, leftCenter.y);
    DrawStick(drawList, leftCenter, leftX, leftY, scale, alphaScale);
    DrawStick(drawList, rightCenter, rightX, rightY, scale, alphaScale);
}

} // namespace

bool IsControllerOverlayEnabled() {
    EnsureControllerOverlayConfigLoaded();
    return g_controllerOverlayEnabled.load(std::memory_order_relaxed);
}

void SetControllerOverlayEnabled(bool enabled) {
    EnsureControllerOverlayConfigLoaded();
    g_controllerOverlayEnabled.store(enabled, std::memory_order_relaxed);
    SaveControllerOverlayConfig();
}

bool GetControllerOverlayEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureControllerOverlayConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 displaySize(displayWidth, displayHeight);
    const float scale = GetClampedControllerOverlayScale();
    const ImVec2 position = GetEffectiveControllerOverlayPosition(displaySize);
    const ImVec2 size = GetControllerOverlayRectSize(scale);

    x = position.x;
    y = position.y;
    width = size.x;
    height = size.y;
    return true;
}

void SetControllerOverlayEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureControllerOverlayConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const ImVec2 clamped = ClampControllerOverlayPosition(ImVec2(displayX, displayY), ImVec2(displayWidth, displayHeight));
    g_controllerOverlayCustomPosition.store(true, std::memory_order_relaxed);
    g_controllerOverlayPositionX.store(clamped.x, std::memory_order_relaxed);
    g_controllerOverlayPositionY.store(clamped.y, std::memory_order_relaxed);
    g_controllerOverlayPositionDirty.store(true, std::memory_order_relaxed);
}

void MoveControllerOverlayEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetControllerOverlayEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetControllerOverlayEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetControllerOverlayEditorScale() {
    EnsureControllerOverlayConfigLoaded();
    return GetClampedControllerOverlayScale();
}

void SetControllerOverlayEditorScale(float scale) {
    EnsureControllerOverlayConfigLoaded();
    g_controllerOverlayScale.store(
        std::clamp(scale, kMinControllerOverlayScale, kMaxControllerOverlayScale),
        std::memory_order_relaxed);
    g_controllerOverlayPositionDirty.store(true, std::memory_order_relaxed);
}

float GetControllerOverlayMinVisualDeadZone() {
    return kMinVisualDeadZone;
}

float GetControllerOverlayMaxVisualDeadZone() {
    return kMaxVisualDeadZone;
}

float GetControllerOverlayLeftVisualDeadZone() {
    EnsureControllerOverlayConfigLoaded();
    return std::clamp(
        g_leftStickVisualDeadZone.load(std::memory_order_relaxed),
        kMinVisualDeadZone,
        kMaxVisualDeadZone);
}

void SetControllerOverlayLeftVisualDeadZone(float value) {
    EnsureControllerOverlayConfigLoaded();
    g_leftStickVisualDeadZone.store(std::clamp(value, kMinVisualDeadZone, kMaxVisualDeadZone), std::memory_order_relaxed);
    SaveControllerOverlayConfig();
}

float GetControllerOverlayRightVisualDeadZone() {
    EnsureControllerOverlayConfigLoaded();
    return std::clamp(
        g_rightStickVisualDeadZone.load(std::memory_order_relaxed),
        kMinVisualDeadZone,
        kMaxVisualDeadZone);
}

void SetControllerOverlayRightVisualDeadZone(float value) {
    EnsureControllerOverlayConfigLoaded();
    g_rightStickVisualDeadZone.store(std::clamp(value, kMinVisualDeadZone, kMaxVisualDeadZone), std::memory_order_relaxed);
    SaveControllerOverlayConfig();
}

void ResetControllerOverlayEditorPosition() {
    EnsureControllerOverlayConfigLoaded();
    g_controllerOverlayCustomPosition.store(false, std::memory_order_relaxed);
    g_controllerOverlayPositionX.store(0.0f, std::memory_order_relaxed);
    g_controllerOverlayPositionY.store(0.0f, std::memory_order_relaxed);
    g_controllerOverlayScale.store(kDefaultControllerOverlayScale, std::memory_order_relaxed);
    g_controllerOverlayPositionDirty.store(true, std::memory_order_relaxed);
    SaveControllerOverlayConfig();
}

void CommitControllerOverlayEditorPosition() {
    EnsureControllerOverlayConfigLoaded();
    if (g_controllerOverlayPositionDirty.load(std::memory_order_relaxed)) {
        SaveControllerOverlayConfig();
    }
}

void RenderControllerOverlayPreview(ImDrawList* drawList, const ImVec2& min, float scale) {
    DrawControllerOverlayAt(drawList, min, scale, true, 1.0f);
}

void RenderControllerOverlay() {
    EnsureControllerOverlayConfigLoaded();
    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (positionEditorActive) {
        return;
    }

    if (ImGui::GetCurrentContext() == nullptr ||
        !g_controllerOverlayEnabled.load(std::memory_order_relaxed) ||
        !ShouldShowGuiOverlay()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float scale = GetClampedControllerOverlayScale();
    const ImVec2 position = GetEffectiveControllerOverlayPosition(io.DisplaySize);
    DrawControllerOverlayAt(ImGui::GetForegroundDrawList(), position, scale, true, 1.0f);
}

} // namespace tane::gui
