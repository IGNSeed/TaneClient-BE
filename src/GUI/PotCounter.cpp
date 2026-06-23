#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
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

constexpr float kMinPotCounterScale = 0.55f;
constexpr float kMaxPotCounterScale = 3.0f;
constexpr float kDefaultPotCounterX = 381.001f;
constexpr float kDefaultPotCounterY = 302.671f;
constexpr float kDefaultPotCounterScale = 0.667f;
constexpr int kPotCounterConfigVersion = 2;
constexpr float kPotionNativeTextSize = 15.0f;
constexpr float kPotionEstimatedTextWidth = 6.35f;
constexpr DWORD64 kPotCounterRefreshMs = 100;

std::atomic_bool g_potCounterEnabled = false;
std::atomic_bool g_potCounterConfigLoaded = false;
std::atomic_bool g_potCounterCustomPosition = false;
std::atomic_bool g_potCounterPositionDirty = false;
std::atomic_bool g_potCounterBackgroundEnabled = false;
std::atomic<float> g_potCounterPositionX = kDefaultPotCounterX;
std::atomic<float> g_potCounterPositionY = kDefaultPotCounterY;
std::atomic<float> g_potCounterScale = kDefaultPotCounterScale;

std::atomic<int> g_cachedPotCount = 0;
std::atomic<float> g_cachedNativeWidth = 854.0f;
std::atomic<float> g_cachedNativeHeight = 480.0f;
std::atomic<DWORD64> g_lastPotCounterRefreshTick = 0;

bool BuildPotCounterConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\PotCounter.json", guiPath) >= 0;
}

bool ParseFloatAfter(const char* section, const char* key, float& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    return found != nullptr && std::sscanf(found + 1, " %f", &value) == 1;
}

bool ParseIntAfter(const char* section, const char* key, int& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    return found != nullptr && std::sscanf(found + 1, " %d", &value) == 1;
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

float GetClampedPotCounterScale() {
    return std::clamp(
        g_potCounterScale.load(std::memory_order_relaxed),
        kMinPotCounterScale,
        kMaxPotCounterScale);
}

void FormatPotCounterText(int count, char* text, std::size_t textSize) {
    if (text == nullptr || textSize == 0) {
        return;
    }

    std::snprintf(text, textSize, "%d Pots", std::clamp(count, 0, 9999));
}

ImVec2 GetPotCounterNativeRectSize(float nativeScale, const char* countText) {
    const float textWidth = static_cast<float>(std::max<std::size_t>(1, std::strlen(countText))) * kPotionEstimatedTextWidth * nativeScale;
    return ImVec2(
        textWidth,
        kPotionNativeTextSize * nativeScale);
}

float GetPotCounterDisplayFontSize(float nativeScale, float displayScale) {
    const float nativeFontSize = kPotionNativeTextSize * nativeScale * displayScale;
    const float imguiFontSize = ImGui::GetCurrentContext() != nullptr
        ? ImGui::GetFontSize() * nativeScale
        : nativeFontSize;
    return std::clamp(std::max(nativeFontSize, imguiFontSize), 10.0f, 36.0f);
}

ImFont* GetGuiTextFont() {
    ImFont* font = tane::payload::GetItemHudFont();
    return font != nullptr ? font : ImGui::GetFont();
}

ImVec2 ClampPotCounterNativePosition(const ImVec2& position, const ImVec2& nativeScreen) {
    const ImVec2 rectSize = GetPotCounterNativeRectSize(GetClampedPotCounterScale(), "9999 Pots");
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, nativeScreen.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, nativeScreen.y - rectSize.y)));
}

ImVec2 GetEffectivePotCounterNativePosition(const ImVec2& nativeScreen) {
    if (!g_potCounterCustomPosition.load(std::memory_order_relaxed)) {
        return ClampPotCounterNativePosition(ImVec2(kDefaultPotCounterX, kDefaultPotCounterY), nativeScreen);
    }

    return ClampPotCounterNativePosition(
        ImVec2(
            g_potCounterPositionX.load(std::memory_order_relaxed),
            g_potCounterPositionY.load(std::memory_order_relaxed)),
        nativeScreen);
}

void SavePotCounterConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildPotCounterConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[384]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": %d,\n"
        "  \"enabled\": %s,\n"
        "  \"showBackground\": %s,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        kPotCounterConfigVersion,
        g_potCounterEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_potCounterBackgroundEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_potCounterCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_potCounterPositionX.load(std::memory_order_relaxed),
        g_potCounterPositionY.load(std::memory_order_relaxed),
        g_potCounterScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_potCounterPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsurePotCounterConfigLoaded() {
    bool expected = false;
    if (!g_potCounterConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildPotCounterConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char json[1024]{};
    DWORD read = 0;
    bool migrateDefaults = false;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';
        int version = 0;
        bool enabled = false;
        bool showBackground = false;
        bool custom = false;
        float x = kDefaultPotCounterX;
        float y = kDefaultPotCounterY;
        float scale = kDefaultPotCounterScale;

        ParseIntAfter(json, "\"version\"", version);
        ParseBoolAfter(json, "\"enabled\"", enabled);
        ParseBoolAfter(json, "\"showBackground\"", showBackground);
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);
        if (version < kPotCounterConfigVersion) {
            custom = false;
            x = kDefaultPotCounterX;
            y = kDefaultPotCounterY;
            scale = kDefaultPotCounterScale;
            migrateDefaults = true;
        }

        g_potCounterEnabled.store(enabled, std::memory_order_relaxed);
        g_potCounterBackgroundEnabled.store(showBackground, std::memory_order_relaxed);
        g_potCounterCustomPosition.store(custom, std::memory_order_relaxed);
        g_potCounterPositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_potCounterPositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_potCounterScale.store(std::clamp(scale, kMinPotCounterScale, kMaxPotCounterScale), std::memory_order_relaxed);
    }
    CloseHandle(file);
    if (migrateDefaults) {
        SavePotCounterConfig();
    }
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

void SetPotCounterBackgroundRect(
    const ImVec2& textPos,
    const ImVec2& textSize,
    float scale,
    float displayScale,
    ImVec2& min,
    ImVec2& max) {
    const float padX = std::max(3.0f, 4.0f * scale * displayScale);
    const float padY = std::max(2.0f, 3.0f * scale * displayScale);
    min = ImVec2(textPos.x - padX, textPos.y - padY);
    max = ImVec2(textPos.x + textSize.x + padX, textPos.y + textSize.y + padY);
}

void DrawPotCounterBackground(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float scale) {
    if (drawList == nullptr || !g_potCounterBackgroundEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    drawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, 142), 5.0f * scale);
    drawList->AddRect(min, max, IM_COL32(255, 255, 255, 44), 5.0f * scale, 0, 1.0f);
}

} // namespace

bool IsPotCounterEnabled() {
    EnsurePotCounterConfigLoaded();
    return g_potCounterEnabled.load(std::memory_order_relaxed);
}

void SetPotCounterEnabled(bool enabled) {
    EnsurePotCounterConfigLoaded();
    g_potCounterEnabled.store(enabled, std::memory_order_relaxed);
    SavePotCounterConfig();
}

bool IsPotCounterBackgroundEnabled() {
    EnsurePotCounterConfigLoaded();
    return g_potCounterBackgroundEnabled.load(std::memory_order_relaxed);
}

void SetPotCounterBackgroundEnabled(bool enabled) {
    EnsurePotCounterConfigLoaded();
    g_potCounterBackgroundEnabled.store(enabled, std::memory_order_relaxed);
    SavePotCounterConfig();
}

bool GetPotCounterEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsurePotCounterConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeWidth.load(std::memory_order_relaxed) : displayWidth,
        g_cachedNativeHeight.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeHeight.load(std::memory_order_relaxed) : displayHeight);
    const ImVec2 nativePosition = GetEffectivePotCounterNativePosition(nativeScreen);

    char text[32]{};
    FormatPotCounterText(64, text, sizeof(text));

    const float scale = GetClampedPotCounterScale();
    const float scaleX = displayWidth / nativeScreen.x;
    const float scaleY = displayHeight / nativeScreen.y;
    const float displayScale = std::min(scaleX, scaleY);
    const float fontSize = GetPotCounterDisplayFontSize(scale, displayScale);
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

void SetPotCounterEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsurePotCounterConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f) {
        return;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeWidth.load(std::memory_order_relaxed) : displayWidth,
        g_cachedNativeHeight.load(std::memory_order_relaxed) > 1.0f ? g_cachedNativeHeight.load(std::memory_order_relaxed) : displayHeight);
    const float scaleX = nativeScreen.x / std::max(1.0f, displayWidth);
    const float scaleY = nativeScreen.y / std::max(1.0f, displayHeight);
    const ImVec2 clamped = ClampPotCounterNativePosition(ImVec2(displayX * scaleX, displayY * scaleY), nativeScreen);

    g_potCounterCustomPosition.store(true, std::memory_order_relaxed);
    g_potCounterPositionX.store(clamped.x, std::memory_order_relaxed);
    g_potCounterPositionY.store(clamped.y, std::memory_order_relaxed);
    g_potCounterPositionDirty.store(true, std::memory_order_relaxed);
}

void MovePotCounterEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetPotCounterEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetPotCounterEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetPotCounterEditorScale() {
    EnsurePotCounterConfigLoaded();
    return GetClampedPotCounterScale();
}

void SetPotCounterEditorScale(float scale) {
    EnsurePotCounterConfigLoaded();
    g_potCounterScale.store(std::clamp(scale, kMinPotCounterScale, kMaxPotCounterScale), std::memory_order_relaxed);
    g_potCounterPositionDirty.store(true, std::memory_order_relaxed);
}

void ResetPotCounterEditorPosition() {
    EnsurePotCounterConfigLoaded();
    g_potCounterCustomPosition.store(false, std::memory_order_relaxed);
    g_potCounterPositionX.store(kDefaultPotCounterX, std::memory_order_relaxed);
    g_potCounterPositionY.store(kDefaultPotCounterY, std::memory_order_relaxed);
    g_potCounterScale.store(kDefaultPotCounterScale, std::memory_order_relaxed);
    g_potCounterPositionDirty.store(true, std::memory_order_relaxed);
    SavePotCounterConfig();
}

void CommitPotCounterEditorPosition() {
    EnsurePotCounterConfigLoaded();
    if (g_potCounterPositionDirty.load(std::memory_order_relaxed)) {
        SavePotCounterConfig();
    }
}

void TickPotCounter(void* clientInstance) {
    EnsurePotCounterConfigLoaded();
    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (clientInstance == nullptr ||
        (!g_potCounterEnabled.load(std::memory_order_relaxed) && !positionEditorActive) ||
        (!positionEditorActive && !ShouldShowGuiOverlay())) {
        return;
    }

    const DWORD64 now = GetTickCount64();
    const DWORD64 previous = g_lastPotCounterRefreshTick.load(std::memory_order_relaxed);
    if (previous != 0 && now - previous < kPotCounterRefreshMs) {
        return;
    }
    g_lastPotCounterRefreshTick.store(now, std::memory_order_relaxed);

    int arrowCount = 0;
    int healingSplashCount = 0;
    if (!QueryInventoryCounterCounts(clientInstance, arrowCount, healingSplashCount)) {
        return;
    }
    (void)arrowCount;
    g_cachedPotCount.store(std::clamp(healingSplashCount, 0, 9999), std::memory_order_relaxed);

    ImVec2 nativeScreen{};
    ResolveNativeScreen(clientInstance, nativeScreen);
}

void RenderPotCounterOverlay() {
    EnsurePotCounterConfigLoaded();
    if (IsGuiPositionEditorActive() ||
        !g_potCounterEnabled.load(std::memory_order_relaxed) ||
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
    const float scale = GetClampedPotCounterScale();
    const float fontSize = GetPotCounterDisplayFontSize(scale, displayScale);
    const ImVec2 nativePosition = GetEffectivePotCounterNativePosition(nativeScreen);
    const ImVec2 position(nativePosition.x * scaleX, nativePosition.y * scaleY);

    char text[32]{};
    FormatPotCounterText(g_cachedPotCount.load(std::memory_order_relaxed), text, sizeof(text));
    ImFont* font = GetGuiTextFont();
    const ImVec2 textSize = font != nullptr
        ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text)
        : ImGui::CalcTextSize(text);
    ImVec2 backgroundMin{};
    ImVec2 backgroundMax{};
    SetPotCounterBackgroundRect(position, textSize, scale, displayScale, backgroundMin, backgroundMax);
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    DrawPotCounterBackground(drawList, backgroundMin, backgroundMax, scale);
    DrawGuiTextWithShadow(drawList, font, fontSize, position, IM_COL32(246, 248, 250, 238), text, scale, IM_COL32(0, 0, 0, 160));
}

void RenderPotCounterPreview(ImDrawList* drawList, const ImVec2& min) {
    EnsurePotCounterConfigLoaded();
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
    const float scale = GetClampedPotCounterScale();
    const float fontSize = GetPotCounterDisplayFontSize(scale, displayScale);

    char text[32]{};
    FormatPotCounterText(64, text, sizeof(text));

    ImFont* font = GetGuiTextFont();
    const ImVec2 textSize = font != nullptr
        ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text)
        : ImGui::CalcTextSize(text);
    const ImVec2 textPos = min;
    ImVec2 backgroundMin{};
    ImVec2 backgroundMax{};
    SetPotCounterBackgroundRect(textPos, textSize, scale, displayScale, backgroundMin, backgroundMax);

    DrawPotCounterBackground(drawList, backgroundMin, backgroundMax, scale);
    DrawGuiTextWithShadow(
        drawList,
        font,
        fontSize,
        textPos,
        IM_COL32(246, 248, 250, 238),
        text,
        scale,
        IM_COL32(0, 0, 0, 160));
}

} // namespace tane::gui
