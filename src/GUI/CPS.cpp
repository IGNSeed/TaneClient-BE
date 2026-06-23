#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace tane::payload {
ImFont* GetItemHudFont();
}

namespace tane::gui {
bool IsGuiPositionEditorActive();
bool IsMenuOpen();
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

enum class CpsDisplayMode {
    Both = 0,
    AttackOnly = 1,
    UseOnly = 2,
};

constexpr float kMinCpsUpdateInterval = 0.05f;
constexpr float kMaxCpsUpdateInterval = 2.0f;
constexpr float kMinCpsScale = 0.65f;
constexpr float kMaxCpsScale = 3.0f;
constexpr float kDefaultCpsUpdateInterval = 0.5f;
constexpr float kDefaultCpsX = 1233.0f;
constexpr float kDefaultCpsY = 1038.217f;
constexpr float kDefaultCpsScale = 1.306f;
constexpr ULONGLONG kCpsSampleWindowMs = 1000;
constexpr ULONGLONG kCpsActionDebounceMs = 20;
constexpr std::size_t kCpsSampleCapacity = 256;

std::atomic_bool g_cpsEnabled = false;
std::atomic_bool g_cpsShowBackground = true;
std::atomic_bool g_cpsConfigLoaded = false;
std::atomic_bool g_cpsCustomPosition = false;
std::atomic_bool g_cpsPositionDirty = false;
std::atomic<float> g_cpsUpdateInterval = kDefaultCpsUpdateInterval;
std::atomic<float> g_cpsPositionX = kDefaultCpsX;
std::atomic<float> g_cpsPositionY = kDefaultCpsY;
std::atomic<float> g_cpsScale = kDefaultCpsScale;
std::atomic<int> g_cpsDisplayMode = static_cast<int>(CpsDisplayMode::Both);

std::atomic<float> g_attackDisplayedCps = 0.0f;
std::atomic<float> g_useDisplayedCps = 0.0f;
std::atomic<ULONGLONG> g_attackLastActionTick = 0;
std::atomic<ULONGLONG> g_useLastActionTick = 0;
std::atomic_uint g_attackSampleCursor = 0;
std::atomic_uint g_useSampleCursor = 0;
std::array<std::atomic<ULONGLONG>, kCpsSampleCapacity> g_attackSampleTicks{};
std::array<std::atomic<ULONGLONG>, kCpsSampleCapacity> g_useSampleTicks{};
ULONGLONG g_cpsLastUpdateTick = 0;

bool BuildCpsConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\CPS.json", guiPath) >= 0;
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

bool ParseIntAfter(const char* section, const char* key, int& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    return std::sscanf(found + 1, " %d", &value) == 1;
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

void ClearCpsSamples() {
    g_attackDisplayedCps.store(0.0f, std::memory_order_relaxed);
    g_useDisplayedCps.store(0.0f, std::memory_order_relaxed);
    g_attackLastActionTick.store(0, std::memory_order_relaxed);
    g_useLastActionTick.store(0, std::memory_order_relaxed);
    g_attackSampleCursor.store(0, std::memory_order_relaxed);
    g_useSampleCursor.store(0, std::memory_order_relaxed);
    for (std::atomic<ULONGLONG>& tick : g_attackSampleTicks) {
        tick.store(0, std::memory_order_relaxed);
    }
    for (std::atomic<ULONGLONG>& tick : g_useSampleTicks) {
        tick.store(0, std::memory_order_relaxed);
    }
    g_cpsLastUpdateTick = GetTickCount64();
}

void EnsureCpsConfigLoaded();

float GetClampedCpsUpdateInterval() {
    return std::clamp(g_cpsUpdateInterval.load(std::memory_order_relaxed), kMinCpsUpdateInterval, kMaxCpsUpdateInterval);
}

int GetClampedCpsDisplayModeValue() {
    return std::clamp(g_cpsDisplayMode.load(std::memory_order_relaxed), 0, 2);
}

float GetClampedCpsScale() {
    return std::clamp(g_cpsScale.load(std::memory_order_relaxed), kMinCpsScale, kMaxCpsScale);
}

ImVec2 GetDefaultCpsPosition() {
    return ImVec2(kDefaultCpsX, kDefaultCpsY);
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

ImVec2 GetCpsTextSize(float scale) {
    const int displayMode = GetClampedCpsDisplayModeValue();
    const char* text = displayMode == static_cast<int>(CpsDisplayMode::Both) ? "000 | 000" : "000 CPS";
    return CalcGuiTextSize(text, ImGui::GetFontSize() * scale);
}

ImVec2 GetCpsRectSize(float scale) {
    const ImVec2 textSize = GetCpsTextSize(scale);
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    return ImVec2(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
}

ImVec2 ClampCpsPosition(const ImVec2& position, const ImVec2& displaySize) {
    const ImVec2 rectSize = GetCpsRectSize(GetClampedCpsScale());
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, displaySize.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, displaySize.y - rectSize.y)));
}

ImVec2 GetEffectiveCpsPosition(const ImVec2& displaySize) {
    EnsureCpsConfigLoaded();
    if (!g_cpsCustomPosition.load(std::memory_order_relaxed)) {
        return ClampCpsPosition(GetDefaultCpsPosition(), displaySize);
    }

    return ClampCpsPosition(
        ImVec2(
            g_cpsPositionX.load(std::memory_order_relaxed),
            g_cpsPositionY.load(std::memory_order_relaxed)),
        displaySize);
}

void SaveCpsConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildCpsConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[512]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 1,\n"
        "  \"enabled\": %s,\n"
        "  \"showBackground\": %s,\n"
        "  \"updateInterval\": %.3f,\n"
        "  \"displayMode\": %d,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        g_cpsEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_cpsShowBackground.load(std::memory_order_relaxed) ? "true" : "false",
        g_cpsUpdateInterval.load(std::memory_order_relaxed),
        GetClampedCpsDisplayModeValue(),
        g_cpsCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_cpsPositionX.load(std::memory_order_relaxed),
        g_cpsPositionY.load(std::memory_order_relaxed),
        g_cpsScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_cpsPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsureCpsConfigLoaded() {
    bool expected = false;
    if (!g_cpsConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildCpsConfigPath(path, MAX_PATH)) {
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
        float updateInterval = kDefaultCpsUpdateInterval;
        float x = kDefaultCpsX;
        float y = kDefaultCpsY;
        float scale = kDefaultCpsScale;
        int displayMode = static_cast<int>(CpsDisplayMode::Both);

        ParseBoolAfter(json, "\"enabled\"", enabled);
        ParseBoolAfter(json, "\"showBackground\"", showBackground);
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"updateInterval\"", updateInterval);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);
        ParseIntAfter(json, "\"displayMode\"", displayMode);

        g_cpsEnabled.store(enabled, std::memory_order_relaxed);
        g_cpsShowBackground.store(showBackground, std::memory_order_relaxed);
        g_cpsUpdateInterval.store(std::clamp(updateInterval, kMinCpsUpdateInterval, kMaxCpsUpdateInterval), std::memory_order_relaxed);
        g_cpsCustomPosition.store(custom, std::memory_order_relaxed);
        g_cpsPositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_cpsPositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_cpsScale.store(std::clamp(scale, kMinCpsScale, kMaxCpsScale), std::memory_order_relaxed);
        g_cpsDisplayMode.store(std::clamp(displayMode, 0, 2), std::memory_order_relaxed);
    }
    CloseHandle(file);
    ClearCpsSamples();
}

void UpdateDisplayedCps() {
    const ULONGLONG now = GetTickCount64();
    if (g_cpsLastUpdateTick == 0) {
        g_cpsLastUpdateTick = now;
        return;
    }

    const ULONGLONG elapsedMs = now - g_cpsLastUpdateTick;
    const float interval = GetClampedCpsUpdateInterval();
    if (elapsedMs < static_cast<ULONGLONG>(interval * 1000.0f)) {
        return;
    }

    const ULONGLONG oldestVisibleTick = now > kCpsSampleWindowMs ? now - kCpsSampleWindowMs : 1;
    unsigned attackClicks = 0;
    unsigned useClicks = 0;
    for (const std::atomic<ULONGLONG>& tick : g_attackSampleTicks) {
        if (tick.load(std::memory_order_relaxed) >= oldestVisibleTick) {
            ++attackClicks;
        }
    }
    for (const std::atomic<ULONGLONG>& tick : g_useSampleTicks) {
        if (tick.load(std::memory_order_relaxed) >= oldestVisibleTick) {
            ++useClicks;
        }
    }

    g_attackDisplayedCps.store(static_cast<float>(attackClicks), std::memory_order_relaxed);
    g_useDisplayedCps.store(static_cast<float>(useClicks), std::memory_order_relaxed);
    g_cpsLastUpdateTick = now;
}

int CpsToDisplayInt(float value) {
    return static_cast<int>(std::clamp(value, 0.0f, 999.0f) + 0.5f);
}

void RecordCpsSample(std::atomic_uint& cursor, std::array<std::atomic<ULONGLONG>, kCpsSampleCapacity>& samples) {
    const unsigned index = cursor.fetch_add(1, std::memory_order_relaxed) % static_cast<unsigned>(samples.size());
    samples[index].store(GetTickCount64(), std::memory_order_release);
}

bool TryReserveActionSample(std::atomic<ULONGLONG>& lastActionTick) {
    const ULONGLONG now = GetTickCount64();
    ULONGLONG previous = lastActionTick.load(std::memory_order_acquire);
    while (previous == 0 || now - previous >= kCpsActionDebounceMs) {
        if (lastActionTick.compare_exchange_weak(previous, now, std::memory_order_acq_rel)) {
            return true;
        }
    }
    return false;
}

void RecordAttackClick() {
    if (g_cpsEnabled.load(std::memory_order_relaxed) && !IsMenuOpen()) {
        RecordCpsSample(g_attackSampleCursor, g_attackSampleTicks);
    }
}

void RecordUseClick() {
    if (g_cpsEnabled.load(std::memory_order_relaxed) && !IsMenuOpen()) {
        RecordCpsSample(g_useSampleCursor, g_useSampleTicks);
    }
}

} // namespace

bool IsCpsEnabled() {
    EnsureCpsConfigLoaded();
    return g_cpsEnabled.load(std::memory_order_relaxed);
}

void SetCpsEnabled(bool enabled) {
    EnsureCpsConfigLoaded();
    g_cpsEnabled.store(enabled, std::memory_order_relaxed);
    ClearCpsSamples();
    SaveCpsConfig();
}

bool IsCpsBackgroundEnabled() {
    EnsureCpsConfigLoaded();
    return g_cpsShowBackground.load(std::memory_order_relaxed);
}

void SetCpsBackgroundEnabled(bool enabled) {
    EnsureCpsConfigLoaded();
    g_cpsShowBackground.store(enabled, std::memory_order_relaxed);
    SaveCpsConfig();
}

float GetCpsUpdateInterval() {
    EnsureCpsConfigLoaded();
    return GetClampedCpsUpdateInterval();
}

void SetCpsUpdateInterval(float seconds) {
    EnsureCpsConfigLoaded();
    g_cpsUpdateInterval.store(std::clamp(seconds, kMinCpsUpdateInterval, kMaxCpsUpdateInterval), std::memory_order_relaxed);
    ClearCpsSamples();
    SaveCpsConfig();
}

float GetCpsMinUpdateInterval() {
    return kMinCpsUpdateInterval;
}

float GetCpsMaxUpdateInterval() {
    return kMaxCpsUpdateInterval;
}

int GetCpsDisplayMode() {
    EnsureCpsConfigLoaded();
    return GetClampedCpsDisplayModeValue();
}

void SetCpsDisplayMode(int mode) {
    EnsureCpsConfigLoaded();
    g_cpsDisplayMode.store(std::clamp(mode, 0, 2), std::memory_order_relaxed);
    SaveCpsConfig();
}

bool GetCpsEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureCpsConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 displaySize(displayWidth, displayHeight);
    const float scale = GetClampedCpsScale();
    const ImVec2 position = GetEffectiveCpsPosition(displaySize);
    const ImVec2 size = GetCpsRectSize(scale);

    x = position.x;
    y = position.y;
    width = size.x;
    height = size.y;
    return true;
}

void SetCpsEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureCpsConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const ImVec2 clamped = ClampCpsPosition(ImVec2(displayX, displayY), ImVec2(displayWidth, displayHeight));
    g_cpsCustomPosition.store(true, std::memory_order_relaxed);
    g_cpsPositionX.store(clamped.x, std::memory_order_relaxed);
    g_cpsPositionY.store(clamped.y, std::memory_order_relaxed);
    g_cpsPositionDirty.store(true, std::memory_order_relaxed);
}

void MoveCpsEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetCpsEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetCpsEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetCpsEditorScale() {
    EnsureCpsConfigLoaded();
    return GetClampedCpsScale();
}

void SetCpsEditorScale(float scale) {
    EnsureCpsConfigLoaded();
    g_cpsScale.store(std::clamp(scale, kMinCpsScale, kMaxCpsScale), std::memory_order_relaxed);
    g_cpsPositionDirty.store(true, std::memory_order_relaxed);
}

void ResetCpsEditorPosition() {
    EnsureCpsConfigLoaded();
    g_cpsCustomPosition.store(false, std::memory_order_relaxed);
    g_cpsPositionX.store(kDefaultCpsX, std::memory_order_relaxed);
    g_cpsPositionY.store(kDefaultCpsY, std::memory_order_relaxed);
    g_cpsScale.store(kDefaultCpsScale, std::memory_order_relaxed);
    g_cpsPositionDirty.store(true, std::memory_order_relaxed);
    SaveCpsConfig();
}

void CommitCpsEditorPosition() {
    EnsureCpsConfigLoaded();
    if (g_cpsPositionDirty.load(std::memory_order_relaxed)) {
        SaveCpsConfig();
    }
}

void RecordCpsAttackClick() {
    EnsureCpsConfigLoaded();
    if (TryReserveActionSample(g_attackLastActionTick)) {
        RecordAttackClick();
    }
}

void RecordCpsUseClick() {
    EnsureCpsConfigLoaded();
    if (TryReserveActionSample(g_useLastActionTick)) {
        RecordUseClick();
    }
}

void RenderCpsOverlay() {
    EnsureCpsConfigLoaded();
    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (positionEditorActive) {
        return;
    }

    if (ImGui::GetCurrentContext() == nullptr ||
        !g_cpsEnabled.load(std::memory_order_relaxed) ||
        !ShouldShowGuiOverlay()) {
        return;
    }

    UpdateDisplayedCps();

    const int displayMode = GetClampedCpsDisplayModeValue();
    const int attackCps = CpsToDisplayInt(g_attackDisplayedCps.load(std::memory_order_relaxed));
    const int useCps = CpsToDisplayInt(g_useDisplayedCps.load(std::memory_order_relaxed));
    char text[64]{};
    if (displayMode == static_cast<int>(CpsDisplayMode::Both)) {
        std::snprintf(text, sizeof(text), "%d | %d", attackCps, useCps);
    } else if (displayMode == static_cast<int>(CpsDisplayMode::AttackOnly)) {
        std::snprintf(text, sizeof(text), "%d CPS", attackCps);
    } else {
        std::snprintf(text, sizeof(text), "%d CPS", useCps);
    }

    ImGuiIO& io = ImGui::GetIO();
    const float scale = GetClampedCpsScale();
    const ImVec2 position = GetEffectiveCpsPosition(io.DisplaySize);
    ImFont* font = GetGuiTextFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    const ImVec2 textSize = CalcGuiTextSize(text, fontSize);
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    const ImVec2 rectSize(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
    const ImVec2 rectMax(position.x + rectSize.x, position.y + rectSize.y);
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if (g_cpsShowBackground.load(std::memory_order_relaxed)) {
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
