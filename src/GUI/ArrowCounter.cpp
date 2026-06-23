#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace tane::payload {
ImFont* GetItemHudFont();
}

namespace tane::gui {
bool IsGuiPositionEditorActive();
bool ShouldShowGuiOverlay();
bool QueryInventoryCounterCounts(void* clientInstance, int& arrowCount, int& healingSplashCount);
bool GetNativeHudScreenSize(void* clientInstance, float& width, float& height);
bool DrawNativeCounterText(
    void* minecraftUiRenderContext,
    const char* text,
    float x,
    float y,
    float width,
    float height,
    float textSize);
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

constexpr float kMinArrowCounterScale = 0.55f;
constexpr float kMaxArrowCounterScale = 3.0f;
constexpr float kDefaultArrowCounterX = 4.0f;
constexpr float kDefaultArrowCounterY = 72.0f;
constexpr float kDefaultArrowCounterScale = 1.0f;
constexpr float kArrowNativeTextSize = 15.0f;
constexpr float kArrowEstimatedTextWidth = 6.35f;
constexpr DWORD64 kArrowCounterRefreshMs = 100;

std::atomic_bool g_arrowCounterEnabled = false;
std::atomic_bool g_arrowCounterConfigLoaded = false;
std::atomic_bool g_arrowCounterCustomPosition = false;
std::atomic_bool g_arrowCounterPositionDirty = false;
std::atomic_bool g_arrowCounterBackgroundEnabled = false;
std::atomic<float> g_arrowCounterPositionX = kDefaultArrowCounterX;
std::atomic<float> g_arrowCounterPositionY = kDefaultArrowCounterY;
std::atomic<float> g_arrowCounterScale = kDefaultArrowCounterScale;

std::atomic<int> g_cachedArrowCount = 0;
std::atomic<float> g_cachedNativeWidth = 854.0f;
std::atomic<float> g_cachedNativeHeight = 480.0f;
std::atomic<DWORD64> g_lastArrowCounterRefreshTick = 0;

bool BuildArrowCounterConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\ArrowCounter.json", guiPath) >= 0;
}

bool ParseFloatAfter(const char* section, const char* key, float& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    return found != nullptr && std::sscanf(found + 1, " %f", &value) == 1;
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

float GetClampedArrowCounterScale() {
    return std::clamp(
        g_arrowCounterScale.load(std::memory_order_relaxed),
        kMinArrowCounterScale,
        kMaxArrowCounterScale);
}

void FormatArrowCounterText(int count, char* text, std::size_t textSize) {
    if (text == nullptr || textSize == 0) {
        return;
    }

    std::snprintf(text, textSize, "%d Arrow(s)", std::clamp(count, 0, 9999));
}

ImVec2 GetArrowCounterNativeRectSize(float nativeScale, const char* countText) {
    const float textWidth = static_cast<float>(std::max<std::size_t>(1, std::strlen(countText))) * kArrowEstimatedTextWidth * nativeScale;
    return ImVec2(
        textWidth,
        kArrowNativeTextSize * nativeScale);
}

float GetArrowCounterDisplayFontSize(float nativeScale, float displayScale) {
    const float nativeFontSize = kArrowNativeTextSize * nativeScale * displayScale;
    const float imguiFontSize = ImGui::GetCurrentContext() != nullptr
        ? ImGui::GetFontSize() * nativeScale
        : nativeFontSize;
    return std::clamp(std::max(nativeFontSize, imguiFontSize), 10.0f, 36.0f);
}

ImFont* GetGuiTextFont() {
    ImFont* font = tane::payload::GetItemHudFont();
    return font != nullptr ? font : ImGui::GetFont();
}

ImVec2 ClampArrowCounterNativePosition(const ImVec2& position, const ImVec2& nativeScreen) {
    const ImVec2 rectSize = GetArrowCounterNativeRectSize(GetClampedArrowCounterScale(), "9999 Arrow(s)");
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, nativeScreen.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, nativeScreen.y - rectSize.y)));
}

ImVec2 GetEffectiveArrowCounterNativePosition(const ImVec2& nativeScreen) {
    if (!g_arrowCounterCustomPosition.load(std::memory_order_relaxed)) {
        return ClampArrowCounterNativePosition(ImVec2(kDefaultArrowCounterX, kDefaultArrowCounterY), nativeScreen);
    }

    return ClampArrowCounterNativePosition(
        ImVec2(
            g_arrowCounterPositionX.load(std::memory_order_relaxed),
            g_arrowCounterPositionY.load(std::memory_order_relaxed)),
        nativeScreen);
}

void SaveArrowCounterConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildArrowCounterConfigPath(path, MAX_PATH)) {
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
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        g_arrowCounterEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_arrowCounterBackgroundEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_arrowCounterCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_arrowCounterPositionX.load(std::memory_order_relaxed),
        g_arrowCounterPositionY.load(std::memory_order_relaxed),
        g_arrowCounterScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_arrowCounterPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsureArrowCounterConfigLoaded() {
    bool expected = false;
    if (!g_arrowCounterConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildArrowCounterConfigPath(path, MAX_PATH)) {
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
        bool showBackground = false;
        bool custom = false;
        float x = kDefaultArrowCounterX;
        float y = kDefaultArrowCounterY;
        float scale = kDefaultArrowCounterScale;

        ParseBoolAfter(json, "\"enabled\"", enabled);
        ParseBoolAfter(json, "\"showBackground\"", showBackground);
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);

        g_arrowCounterEnabled.store(enabled, std::memory_order_relaxed);
        g_arrowCounterBackgroundEnabled.store(showBackground, std::memory_order_relaxed);
        g_arrowCounterCustomPosition.store(custom, std::memory_order_relaxed);
        g_arrowCounterPositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_arrowCounterPositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_arrowCounterScale.store(std::clamp(scale, kMinArrowCounterScale, kMaxArrowCounterScale), std::memory_order_relaxed);
    }
    CloseHandle(file);
}

bool ResolveNativeScreen(void* clientInstance, ImVec2& nativeScreen) {
    float width = g_cachedNativeWidth.load(std::memory_order_relaxed);
    float height = g_cachedNativeHeight.load(std::memory_order_relaxed);
    if (GetNativeHudScreenSize(clientInstance, width, height)) {
        g_cachedNativeWidth.store(width, std::memory_order_relaxed);
        g_cachedNativeHeight.store(height, std::memory_order_relaxed);
    }

    nativeScreen = ImVec2(std::max(1.0f, width), std::max(1.0f, height));
    return nativeScreen.x > 1.0f && nativeScreen.y > 1.0f;
}

void DrawArrowCounterBackground(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float scale) {
    if (drawList == nullptr || !g_arrowCounterBackgroundEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    drawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, 142), 5.0f * scale);
    drawList->AddRect(min, max, IM_COL32(255, 255, 255, 44), 5.0f * scale, 0, 1.0f);
}

} // namespace

bool IsArrowCounterEnabled() {
    EnsureArrowCounterConfigLoaded();
    return g_arrowCounterEnabled.load(std::memory_order_relaxed);
}

void SetArrowCounterEnabled(bool enabled) {
    EnsureArrowCounterConfigLoaded();
    g_arrowCounterEnabled.store(enabled, std::memory_order_relaxed);
    SaveArrowCounterConfig();
}

bool IsArrowCounterBackgroundEnabled() {
    EnsureArrowCounterConfigLoaded();
    return g_arrowCounterBackgroundEnabled.load(std::memory_order_relaxed);
}

void SetArrowCounterBackgroundEnabled(bool enabled) {
    EnsureArrowCounterConfigLoaded();
    g_arrowCounterBackgroundEnabled.store(enabled, std::memory_order_relaxed);
    SaveArrowCounterConfig();
}

bool GetArrowCounterEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureArrowCounterConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeWidth.load(std::memory_order_relaxed) : displayWidth,
        g_cachedNativeHeight.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeHeight.load(std::memory_order_relaxed) : displayHeight);
    const ImVec2 nativePosition = GetEffectiveArrowCounterNativePosition(nativeScreen);

    char text[32]{};
    FormatArrowCounterText(64, text, sizeof(text));

    const float scale = GetClampedArrowCounterScale();
    const float scaleX = displayWidth / nativeScreen.x;
    const float scaleY = displayHeight / nativeScreen.y;
    const float displayScale = std::min(scaleX, scaleY);
    const float fontSize = GetArrowCounterDisplayFontSize(scale, displayScale);
    ImFont* font = GetGuiTextFont();
    const ImVec2 textSize = font != nullptr
        ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text)
        : ImGui::CalcTextSize(text);

    x = nativePosition.x * scaleX;
    y = nativePosition.y * scaleY;
    width = textSize.x;
    height = textSize.y;
    return true;
}

void SetArrowCounterEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureArrowCounterConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f) {
        return;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeWidth.load(std::memory_order_relaxed) : displayWidth,
        g_cachedNativeHeight.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeHeight.load(std::memory_order_relaxed) : displayHeight);
    const float scaleX = nativeScreen.x / std::max(1.0f, displayWidth);
    const float scaleY = nativeScreen.y / std::max(1.0f, displayHeight);
    const ImVec2 clamped = ClampArrowCounterNativePosition(ImVec2(displayX * scaleX, displayY * scaleY), nativeScreen);

    g_arrowCounterCustomPosition.store(true, std::memory_order_relaxed);
    g_arrowCounterPositionX.store(clamped.x, std::memory_order_relaxed);
    g_arrowCounterPositionY.store(clamped.y, std::memory_order_relaxed);
    g_arrowCounterPositionDirty.store(true, std::memory_order_relaxed);
}

void MoveArrowCounterEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetArrowCounterEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetArrowCounterEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetArrowCounterEditorScale() {
    EnsureArrowCounterConfigLoaded();
    return GetClampedArrowCounterScale();
}

void SetArrowCounterEditorScale(float scale) {
    EnsureArrowCounterConfigLoaded();
    g_arrowCounterScale.store(std::clamp(scale, kMinArrowCounterScale, kMaxArrowCounterScale), std::memory_order_relaxed);
    g_arrowCounterPositionDirty.store(true, std::memory_order_relaxed);
}

void ResetArrowCounterEditorPosition() {
    EnsureArrowCounterConfigLoaded();
    g_arrowCounterCustomPosition.store(false, std::memory_order_relaxed);
    g_arrowCounterPositionX.store(kDefaultArrowCounterX, std::memory_order_relaxed);
    g_arrowCounterPositionY.store(kDefaultArrowCounterY, std::memory_order_relaxed);
    g_arrowCounterScale.store(kDefaultArrowCounterScale, std::memory_order_relaxed);
    g_arrowCounterPositionDirty.store(true, std::memory_order_relaxed);
    SaveArrowCounterConfig();
}

void CommitArrowCounterEditorPosition() {
    EnsureArrowCounterConfigLoaded();
    if (g_arrowCounterPositionDirty.load(std::memory_order_relaxed)) {
        SaveArrowCounterConfig();
    }
}

void TickArrowCounter(void* clientInstance) {
    EnsureArrowCounterConfigLoaded();
    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (clientInstance == nullptr ||
        (!g_arrowCounterEnabled.load(std::memory_order_relaxed) && !positionEditorActive) ||
        (!positionEditorActive && !ShouldShowGuiOverlay())) {
        return;
    }

    const DWORD64 now = GetTickCount64();
    const DWORD64 previous = g_lastArrowCounterRefreshTick.load(std::memory_order_relaxed);
    if (previous != 0 && now - previous < kArrowCounterRefreshMs) {
        return;
    }
    g_lastArrowCounterRefreshTick.store(now, std::memory_order_relaxed);

    int arrowCount = 0;
    int healingSplashCount = 0;
    if (!QueryInventoryCounterCounts(clientInstance, arrowCount, healingSplashCount)) {
        return;
    }
    (void)healingSplashCount;
    g_cachedArrowCount.store(std::clamp(arrowCount, 0, 9999), std::memory_order_relaxed);

    ImVec2 nativeScreen{};
    ResolveNativeScreen(clientInstance, nativeScreen);
}

void RenderArrowCounterOverlay() {
    EnsureArrowCounterConfigLoaded();
    if (IsGuiPositionEditorActive() ||
        !g_arrowCounterEnabled.load(std::memory_order_relaxed) ||
        !ShouldShowGuiOverlay() ||
        ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float nativeWidth = std::max(1.0f, g_cachedNativeWidth.load(std::memory_order_relaxed));
    const float nativeHeight = std::max(1.0f, g_cachedNativeHeight.load(std::memory_order_relaxed));
    const ImVec2 nativeScreen(nativeWidth, nativeHeight);
    const float scaleX = io.DisplaySize.x / nativeWidth;
    const float scaleY = io.DisplaySize.y / nativeHeight;
    const float displayScale = std::min(scaleX, scaleY);
    const float scale = GetClampedArrowCounterScale();
    const float fontSize = GetArrowCounterDisplayFontSize(scale, displayScale);
    const ImVec2 nativePosition = GetEffectiveArrowCounterNativePosition(nativeScreen);
    const ImVec2 position(nativePosition.x * scaleX, nativePosition.y * scaleY);

    char text[32]{};
    FormatArrowCounterText(g_cachedArrowCount.load(std::memory_order_relaxed), text, sizeof(text));
    ImFont* font = GetGuiTextFont();
    const ImVec2 textSize = font != nullptr
        ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text)
        : ImGui::CalcTextSize(text);
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    DrawArrowCounterBackground(drawList, position, ImVec2(position.x + textSize.x, position.y + textSize.y), scale);
    DrawGuiTextWithShadow(drawList, font, fontSize, position, IM_COL32(246, 248, 250, 238), text, scale, IM_COL32(0, 0, 0, 160));
}

void RenderArrowCounterPreview(ImDrawList* drawList, const ImVec2& min) {
    EnsureArrowCounterConfigLoaded();
    if (drawList == nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeWidth.load(std::memory_order_relaxed) : std::max(1.0f, io.DisplaySize.x),
        g_cachedNativeHeight.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeHeight.load(std::memory_order_relaxed) : std::max(1.0f, io.DisplaySize.y));
    const float scaleX = io.DisplaySize.x / std::max(1.0f, nativeScreen.x);
    const float scaleY = io.DisplaySize.y / std::max(1.0f, nativeScreen.y);
    const float displayScale = std::min(scaleX, scaleY);
    const float scale = GetClampedArrowCounterScale();
    const float fontSize = GetArrowCounterDisplayFontSize(scale, displayScale);

    char text[32]{};
    FormatArrowCounterText(64, text, sizeof(text));

    ImFont* font = GetGuiTextFont();
    const ImVec2 textSize = font != nullptr
        ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text)
        : ImGui::CalcTextSize(text);
    const ImVec2 rectMax(min.x + textSize.x, min.y + textSize.y);

    DrawArrowCounterBackground(drawList, min, rectMax, scale);
    DrawGuiTextWithShadow(
        drawList,
        font,
        fontSize,
        min,
        IM_COL32(246, 248, 250, 238),
        text,
        scale,
        IM_COL32(0, 0, 0, 160));
}

} // namespace tane::gui
