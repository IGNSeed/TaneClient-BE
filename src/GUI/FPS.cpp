#include "Offsets.h"

#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace tane::payload {
ImFont* GetItemHudFont();
}

namespace tane::gui {
bool IsGuiPositionEditorActive();
bool ShouldShowGuiOverlay();
void DrawGuiTextWithShadow(
    ImDrawList* drawList,
    ImFont* font,
    float fontSize,
    const ImVec2& pos,
    ImU32 color,
    const char* text,
    float shadowOffset,
    ImU32 shadowColor);
}

namespace tane::gui {
namespace {

constexpr float kMinFpsScale = 0.65f;
constexpr float kMaxFpsScale = 3.0f;
constexpr float kMinFpsUpdateInterval = 0.05f;
constexpr float kMaxFpsUpdateInterval = 2.0f;
constexpr float kDefaultFpsX = 3.0f;
constexpr float kDefaultFpsY = 42.0f;
constexpr float kDefaultFpsScale = 1.34f;

std::atomic_bool g_fpsEnabled = false;
std::atomic_bool g_fpsShowBackground = true;
std::atomic_bool g_fpsConfigLoaded = false;
std::atomic_bool g_fpsCustomPosition = false;
std::atomic_bool g_fpsPositionDirty = false;
std::atomic<float> g_fpsUpdateInterval = 0.5f;
std::atomic<float> g_fpsPositionX = kDefaultFpsX;
std::atomic<float> g_fpsPositionY = kDefaultFpsY;
std::atomic<float> g_fpsScale = kDefaultFpsScale;

std::atomic<float> g_fpsDisplayedValue = 0.0f;
std::atomic<std::uint64_t> g_fpsFrameId = 0;
double g_fpsAccumulatedSeconds = 0.0;
int g_fpsAccumulatedFrames = 0;
std::int64_t g_fpsLastCounter = 0;
std::int64_t g_fpsCounterFrequency = 0;
ULONGLONG g_fpsLastInternalReadTick = 0;

bool BuildFpsConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\FPS.json", guiPath) >= 0;
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

void SaveFpsConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildFpsConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[384]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 1,\n"
        "  \"enabled\": %s,\n"
        "  \"showBackground\": %s,\n"
        "  \"updateInterval\": %.3f,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        g_fpsEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_fpsShowBackground.load(std::memory_order_relaxed) ? "true" : "false",
        g_fpsUpdateInterval.load(std::memory_order_relaxed),
        g_fpsCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_fpsPositionX.load(std::memory_order_relaxed),
        g_fpsPositionY.load(std::memory_order_relaxed),
        g_fpsScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_fpsPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsureFpsConfigLoaded() {
    bool expected = false;
    if (!g_fpsConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildFpsConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char json[1024]{};
    DWORD read = 0;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';
        bool enabled = false;
        bool showBackground = true;
        bool custom = false;
        float updateInterval = 0.5f;
        float x = kDefaultFpsX;
        float y = kDefaultFpsY;
        float scale = kDefaultFpsScale;

        ParseBoolAfter(json, "\"enabled\"", enabled);
        ParseBoolAfter(json, "\"showBackground\"", showBackground);
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"updateInterval\"", updateInterval);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);

        g_fpsEnabled.store(enabled, std::memory_order_relaxed);
        g_fpsShowBackground.store(showBackground, std::memory_order_relaxed);
        g_fpsUpdateInterval.store(std::clamp(updateInterval, kMinFpsUpdateInterval, kMaxFpsUpdateInterval), std::memory_order_relaxed);
        g_fpsCustomPosition.store(custom, std::memory_order_relaxed);
        g_fpsPositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_fpsPositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_fpsScale.store(std::clamp(scale, kMinFpsScale, kMaxFpsScale), std::memory_order_relaxed);
    }
    CloseHandle(file);
}

float GetClampedFpsScale() {
    return std::clamp(g_fpsScale.load(std::memory_order_relaxed), kMinFpsScale, kMaxFpsScale);
}

float GetClampedFpsUpdateInterval() {
    return std::clamp(g_fpsUpdateInterval.load(std::memory_order_relaxed), kMinFpsUpdateInterval, kMaxFpsUpdateInterval);
}

ImVec2 GetDefaultFpsPosition() {
    return ImVec2(kDefaultFpsX, kDefaultFpsY);
}

ImFont* GetGuiTextFont() {
    ImFont* font = tane::payload::GetItemHudFont();
    return font != nullptr ? font : ImGui::GetFont();
}

ImVec2 CalcGuiTextSize(const char* text, float fontSize) {
    ImFont* font = GetGuiTextFont();
    if (font != nullptr) {
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    }

    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const float baseFontSize = ImGui::GetFontSize();
    const float scale = baseFontSize > 0.0f ? fontSize / baseFontSize : 1.0f;
    return ImVec2(textSize.x * scale, textSize.y * scale);
}

ImVec2 GetFpsTextSize(float scale) {
    return CalcGuiTextSize("FPS 000", ImGui::GetFontSize() * scale);
}

ImVec2 GetFpsRectSize(float scale) {
    const ImVec2 textSize = GetFpsTextSize(scale);
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    return ImVec2(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
}

ImVec2 ClampFpsPosition(const ImVec2& position, const ImVec2& displaySize) {
    const ImVec2 rectSize = GetFpsRectSize(GetClampedFpsScale());
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, displaySize.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, displaySize.y - rectSize.y)));
}

ImVec2 GetEffectiveFpsPosition(const ImVec2& displaySize) {
    EnsureFpsConfigLoaded();
    if (!g_fpsCustomPosition.load(std::memory_order_relaxed)) {
        return ClampFpsPosition(GetDefaultFpsPosition(), displaySize);
    }

    return ClampFpsPosition(
        ImVec2(
            g_fpsPositionX.load(std::memory_order_relaxed),
            g_fpsPositionY.load(std::memory_order_relaxed)),
        displaySize);
}

void RecordFpsFrameSample() {
    LARGE_INTEGER counter{};
    if (!QueryPerformanceCounter(&counter)) {
        return;
    }

    if (g_fpsCounterFrequency <= 0) {
        LARGE_INTEGER frequency{};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
            return;
        }

        g_fpsCounterFrequency = frequency.QuadPart;
        g_fpsLastCounter = counter.QuadPart;
        return;
    }

    const std::int64_t previousCounter = g_fpsLastCounter;
    g_fpsLastCounter = counter.QuadPart;
    if (previousCounter <= 0 || counter.QuadPart <= previousCounter) {
        return;
    }

    const double deltaTime = static_cast<double>(counter.QuadPart - previousCounter) /
        static_cast<double>(g_fpsCounterFrequency);
    if (deltaTime <= 0.0 || deltaTime > 1.0 || !std::isfinite(deltaTime)) {
        return;
    }

    g_fpsAccumulatedSeconds += deltaTime;
    ++g_fpsAccumulatedFrames;
    const double updateInterval = static_cast<double>(GetClampedFpsUpdateInterval());
    if (g_fpsDisplayedValue.load(std::memory_order_relaxed) <= 0.0f || g_fpsAccumulatedSeconds >= updateInterval) {
        const double fps = static_cast<double>(g_fpsAccumulatedFrames) / std::max(0.0001, g_fpsAccumulatedSeconds);
        g_fpsDisplayedValue.store(static_cast<float>(std::clamp(fps, 0.0, 5000.0)), std::memory_order_relaxed);
        g_fpsAccumulatedSeconds = 0.0;
        g_fpsAccumulatedFrames = 0;
    }
}

bool ReadInternalDebugFps(float& fps) {
    const std::uintptr_t rva = tane::offsets::gui::kDebugFpsValueRva;
    if (rva == 0) {
        return false;
    }
    HMODULE imageBase = GetModuleHandleW(nullptr);
    if (imageBase == nullptr) {
        return false;
    }

    const auto* address = reinterpret_cast<const float*>(
        reinterpret_cast<const std::uint8_t*>(imageBase) + rva);
    __try {
        fps = *address;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return std::isfinite(fps) && fps > 0.0f && fps <= 5000.0f;
}

void UpdateInternalDebugFpsDisplay() {
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG updateIntervalMs =
        static_cast<ULONGLONG>(std::max(1.0f, GetClampedFpsUpdateInterval() * 1000.0f));
    if (g_fpsLastInternalReadTick != 0 && now - g_fpsLastInternalReadTick < updateIntervalMs) {
        return;
    }

    float fps = 0.0f;
    if (!ReadInternalDebugFps(fps)) {
        return;
    }

    g_fpsLastInternalReadTick = now;
    g_fpsDisplayedValue.store(fps, std::memory_order_relaxed);
}

} // namespace

bool IsFpsEnabled() {
    EnsureFpsConfigLoaded();
    return g_fpsEnabled.load(std::memory_order_relaxed);
}

void SetFpsEnabled(bool enabled) {
    EnsureFpsConfigLoaded();
    g_fpsEnabled.store(enabled, std::memory_order_relaxed);
    SaveFpsConfig();
}

bool IsFpsBackgroundEnabled() {
    EnsureFpsConfigLoaded();
    return g_fpsShowBackground.load(std::memory_order_relaxed);
}

void SetFpsBackgroundEnabled(bool enabled) {
    EnsureFpsConfigLoaded();
    g_fpsShowBackground.store(enabled, std::memory_order_relaxed);
    SaveFpsConfig();
}

float GetFpsUpdateInterval() {
    EnsureFpsConfigLoaded();
    return GetClampedFpsUpdateInterval();
}

void SetFpsUpdateInterval(float seconds) {
    EnsureFpsConfigLoaded();
    g_fpsUpdateInterval.store(std::clamp(seconds, kMinFpsUpdateInterval, kMaxFpsUpdateInterval), std::memory_order_relaxed);
    SaveFpsConfig();
}

float GetFpsMinUpdateInterval() {
    return kMinFpsUpdateInterval;
}

float GetFpsMaxUpdateInterval() {
    return kMaxFpsUpdateInterval;
}

bool GetFpsEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureFpsConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 displaySize(displayWidth, displayHeight);
    const float scale = GetClampedFpsScale();
    const ImVec2 position = GetEffectiveFpsPosition(displaySize);
    const ImVec2 size = GetFpsRectSize(scale);

    x = position.x;
    y = position.y;
    width = size.x;
    height = size.y;
    return true;
}

void SetFpsEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureFpsConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const ImVec2 clamped = ClampFpsPosition(ImVec2(displayX, displayY), ImVec2(displayWidth, displayHeight));
    g_fpsCustomPosition.store(true, std::memory_order_relaxed);
    g_fpsPositionX.store(clamped.x, std::memory_order_relaxed);
    g_fpsPositionY.store(clamped.y, std::memory_order_relaxed);
    g_fpsPositionDirty.store(true, std::memory_order_relaxed);
}

void MoveFpsEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetFpsEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetFpsEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetFpsEditorScale() {
    EnsureFpsConfigLoaded();
    return GetClampedFpsScale();
}

void SetFpsEditorScale(float scale) {
    EnsureFpsConfigLoaded();
    g_fpsScale.store(std::clamp(scale, kMinFpsScale, kMaxFpsScale), std::memory_order_relaxed);
    g_fpsPositionDirty.store(true, std::memory_order_relaxed);
}

void ResetFpsEditorPosition() {
    EnsureFpsConfigLoaded();
    g_fpsCustomPosition.store(false, std::memory_order_relaxed);
    g_fpsPositionX.store(kDefaultFpsX, std::memory_order_relaxed);
    g_fpsPositionY.store(kDefaultFpsY, std::memory_order_relaxed);
    g_fpsScale.store(kDefaultFpsScale, std::memory_order_relaxed);
    g_fpsPositionDirty.store(true, std::memory_order_relaxed);
    SaveFpsConfig();
}

void CommitFpsEditorPosition() {
    EnsureFpsConfigLoaded();
    if (g_fpsPositionDirty.load(std::memory_order_relaxed)) {
        SaveFpsConfig();
    }
}

void RecordFpsFrame() {
    g_fpsFrameId.fetch_add(1, std::memory_order_relaxed);
    RecordFpsFrameSample();
}

std::uint64_t GetFpsFrameId() {
    return g_fpsFrameId.load(std::memory_order_relaxed);
}

void RenderFpsOverlay() {
    EnsureFpsConfigLoaded();
    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (positionEditorActive) {
        return;
    }

    if (ImGui::GetCurrentContext() == nullptr ||
        !g_fpsEnabled.load(std::memory_order_relaxed) ||
        !ShouldShowGuiOverlay()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    UpdateInternalDebugFpsDisplay();

    char text[32]{};
    std::snprintf(text, sizeof(text), "FPS %.0f", std::max(0.0f, g_fpsDisplayedValue.load(std::memory_order_relaxed)));

    const float scale = GetClampedFpsScale();
    const ImVec2 displaySize = io.DisplaySize;
    const ImVec2 position = GetEffectiveFpsPosition(displaySize);
    ImFont* font = GetGuiTextFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    const ImVec2 textSize = CalcGuiTextSize(text, fontSize);
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    const ImVec2 rectSize(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
    const ImVec2 rectMax(position.x + rectSize.x, position.y + rectSize.y);
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if (g_fpsShowBackground.load(std::memory_order_relaxed)) {
        drawList->AddRectFilled(position, rectMax, IM_COL32(0, 0, 0, 142), 5.0f * scale);
        drawList->AddRect(position, rectMax, IM_COL32(255, 255, 255, 44), 5.0f * scale, 0, 1.0f);
    }

    const ImVec2 textPos(position.x + paddingX, position.y + paddingY);
    DrawGuiTextWithShadow(
        drawList,
        font,
        fontSize,
        textPos,
        IM_COL32(246, 248, 250, 238),
        text,
        scale,
        IM_COL32(0, 0, 0, 150));
}

} // namespace tane::gui
