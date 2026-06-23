#include <Windows.h>
#include <imgui.h>
#include <MinHook.h>

#include "Offsets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <cstdint>

namespace tane::gui {
bool IsGuiPositionEditorActive();
bool ShouldShowGuiOverlay();
bool GetNativeHudScreenSize(void* clientInstance, float& width, float& height);
bool FillNativeRectangle(
    void* minecraftUiRenderContext,
    float x,
    float y,
    float width,
    float height,
    float r,
    float g,
    float b,
    float a);
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

namespace tane::payload {
ImFont* GetItemHudFont();
bool GetPngTextureFromFile(const wchar_t* path, ImTextureRef& texture, ImVec2& size);
}

namespace tane::gui {
namespace {

using namespace tane::offsets::item_hud;

constexpr float kMinEffectHudScale = 0.55f;
constexpr float kMaxEffectHudScale = 3.0f;
constexpr float kDefaultEffectHudX = 3.0f;
constexpr float kDefaultEffectHudY = 0.0f;
constexpr float kDefaultEffectHudScale = 0.570f;
constexpr DWORD64 kEffectHudRefreshMs = 100;
constexpr DWORD64 kEffectHudOverlayFreshMs = 180;
constexpr int kEffectHudConfigVersion = 1;
constexpr int kMaxEffectHudEntries = 16;
constexpr float kEffectHudIconSize = 30.0f;
constexpr float kEffectHudRowHeight = 36.0f;
constexpr float kEffectHudPaddingX = 8.0f;
constexpr float kEffectHudPaddingY = 7.0f;
constexpr float kEffectHudGap = 7.0f;
constexpr float kEffectHudLineSpacing = 2.0f;
constexpr float kEffectHudNativeTextSize = 13.5f;
constexpr float kEffectHudEstimatedTextWidth = 7.0f;
constexpr DWORD64 kEffectIconFileRecheckMs = 5000;
constexpr DWORD64 kEffectIconTextureRetryMs = 1000;

struct VfuncCache {
    void** vtable = nullptr;
    std::size_t offset = 0;
    void* function = nullptr;
};

struct EffectInfo {
    int id = 0;
    const char* name = "Effect";
    const char* iconPath = "textures/ui/effect_background";
    float r = 0.65f;
    float g = 0.65f;
    float b = 0.65f;
};

struct EffectHudEntry {
    int id = 0;
    int level = 1;
    int durationTicks = 0;
    const EffectInfo* info = nullptr;
};

struct EffectTimerState {
    int id = 0;
    int level = 0;
    int sourceDurationTicks = 0;
    DWORD64 sourceTick = 0;
    DWORD64 lastSeenTick = 0;
    bool active = false;
};

struct OverlayLine {
    char title[80]{};
    char duration[24]{};
    char iconPath[96]{};
    ImVec2 iconMin{};
    ImVec2 iconMax{};
    ImVec2 titlePos{};
    ImVec2 durationPos{};
    float fontSize = 13.0f;
    float shadowOffset = 1.0f;
    ImU32 iconColor = IM_COL32(166, 166, 166, 230);
};

using GetLocalPlayerFn = void*(__fastcall*)(void* clientInstance);
using MobEffectsRendererRenderFn = void(__fastcall*)(void* self, void* renderContext, void* a3, void* a4);

std::atomic_bool g_effectHudEnabled = false;
std::atomic_bool g_effectHudConfigLoaded = false;
std::atomic_bool g_effectHudCustomPosition = false;
std::atomic_bool g_effectHudPositionDirty = false;
std::atomic_bool g_effectHudBackgroundEnabled = true;
std::atomic<float> g_effectHudPositionX = kDefaultEffectHudX;
std::atomic<float> g_effectHudPositionY = kDefaultEffectHudY;
std::atomic<float> g_effectHudScale = kDefaultEffectHudScale;

VfuncCache g_localPlayerVfunc{};
std::array<EffectHudEntry, kMaxEffectHudEntries> g_cachedEffects{};
std::array<EffectTimerState, kMaxEffectHudEntries> g_effectTimers{};
int g_cachedEffectCount = 0;
void* g_cachedEffectClientInstance = nullptr;
DWORD64 g_lastEffectRefreshTick = 0;
DWORD64 g_lastEffectNativeDrawTick = 0;
float g_cachedNativeWidth = 854.0f;
float g_cachedNativeHeight = 480.0f;
std::array<OverlayLine, kMaxEffectHudEntries> g_overlayLines{};
int g_overlayLineCount = 0;
ImVec2 g_overlayBackgroundMin{};
ImVec2 g_overlayBackgroundMax{};
bool g_overlayBackgroundVisible = false;
std::array<EffectHudEntry, kMaxEffectHudEntries> g_lastOverlayMetricsEntries{};
ImVec2 g_lastOverlayMetricsNativePosition(-1.0f, -1.0f);
ImVec2 g_lastOverlayMetricsNativeScreen(-1.0f, -1.0f);
ImVec2 g_lastOverlayMetricsDisplaySize(-1.0f, -1.0f);
int g_lastOverlayMetricsCount = -1;
float g_lastOverlayMetricsScale = -1.0f;
bool g_lastOverlayMetricsBackgroundEnabled = false;
std::atomic_bool g_effectHudImagesInjected = false;
std::atomic_bool g_effectHudImagesInjectSucceeded = false;
std::atomic_int g_effectHudIconPreloadIndex = 0;
std::atomic_bool g_effectHudIconPreloadComplete = false;
std::atomic_bool g_effectHudHooksInstalled = false;
MobEffectsRendererRenderFn g_originalMobEffectsRendererRender = nullptr;
HMODULE g_effectHudPayloadModule = nullptr;

struct IconPathCache {
    char iconPath[96]{};
    wchar_t filePath[MAX_PATH]{};
    ImTextureRef texture{};
    ImVec2 textureSize{};
    DWORD64 lastFileCheckTick = 0;
    DWORD64 lastTextureAttemptTick = 0;
    bool attempted = false;
    bool found = false;
    bool textureReady = false;
};
std::array<IconPathCache, 64> g_iconPathCache{};

struct EmbeddedEffectIcon {
    int resourceId = 0;
    const wchar_t* fileName = nullptr;
};

constexpr EmbeddedEffectIcon kEmbeddedEffectIcons[] = {
    {3000, L"absorption_effect.png"},
    {3001, L"bad_omen_effect.png"},
    {3002, L"blindness_effect.png"},
    {3003, L"breath_of_the_nautilus_effect.png"},
    {3004, L"conduit_power_effect.png"},
    {3005, L"darkness_effect.png"},
    {3006, L"fire_resistance_effect.png"},
    {3007, L"haste_effect.png"},
    {3008, L"health_boost_effect.png"},
    {3009, L"hunger_effect.png"},
    {3010, L"infested_effect.png"},
    {3011, L"invisibility_effect.png"},
    {3012, L"jump_boost_effect.png"},
    {3013, L"levitation_effect.png"},
    {3014, L"mining_fatigue_effect.png"},
    {3015, L"nausea_effect.png"},
    {3016, L"night_vision_effect.png"},
    {3017, L"oozing_effect.png"},
    {3018, L"poison_effect.png"},
    {3019, L"raid_omen_effect.png"},
    {3020, L"regeneration_effect.png"},
    {3021, L"resistance_effect.png"},
    {3022, L"slow_falling_effect.png"},
    {3023, L"slowness_effect.png"},
    {3024, L"speed_effect.png"},
    {3025, L"strength_effect.png"},
    {3026, L"trial_omen_effect.png"},
    {3027, L"village_hero_effect.png"},
    {3028, L"water_breathing_effect.png"},
    {3029, L"weakness_effect.png"},
    {3030, L"weaving_effect.png"},
    {3031, L"wind_charged_effect.png"},
    {3032, L"wither_effect.png"},
};

constexpr EffectInfo kEffectInfos[] = {
    {1, "Speed", "textures/ui/speed_effect", 0.45f, 0.70f, 0.95f},
    {2, "Slowness", "textures/ui/slowness_effect", 0.42f, 0.48f, 0.72f},
    {3, "Haste", "textures/ui/haste_effect", 0.92f, 0.78f, 0.24f},
    {4, "Mining Fatigue", "textures/ui/mining_fatigue_effect", 0.48f, 0.42f, 0.34f},
    {5, "Strength", "textures/ui/strength_effect", 0.84f, 0.24f, 0.22f},
    {6, "Instant Health", "textures/ui/health_boost_effect", 0.92f, 0.28f, 0.34f},
    {7, "Instant Damage", "textures/ui/harm_effect", 0.42f, 0.08f, 0.12f},
    {8, "Jump Boost", "textures/ui/jump_boost_effect", 0.36f, 0.82f, 0.44f},
    {9, "Nausea", "textures/ui/nausea_effect", 0.56f, 0.42f, 0.70f},
    {10, "Regeneration", "textures/ui/regeneration_effect", 0.92f, 0.42f, 0.58f},
    {11, "Resistance", "textures/ui/resistance_effect", 0.58f, 0.58f, 0.66f},
    {12, "Fire Resistance", "textures/ui/fire_resistance_effect", 0.95f, 0.46f, 0.16f},
    {13, "Water Breathing", "textures/ui/water_breathing_effect", 0.18f, 0.58f, 0.90f},
    {14, "Invisibility", "textures/ui/invisibility_effect", 0.72f, 0.72f, 0.78f},
    {15, "Blindness", "textures/ui/blindness_effect", 0.18f, 0.18f, 0.20f},
    {16, "Night Vision", "textures/ui/night_vision_effect", 0.30f, 0.70f, 0.34f},
    {17, "Hunger", "textures/ui/hunger_effect", 0.48f, 0.36f, 0.20f},
    {18, "Weakness", "textures/ui/weakness_effect", 0.44f, 0.46f, 0.50f},
    {19, "Poison", "textures/ui/poison_effect", 0.34f, 0.68f, 0.24f},
    {20, "Wither", "textures/ui/wither_effect", 0.16f, 0.16f, 0.16f},
    {21, "Health Boost", "textures/ui/health_boost_effect", 0.92f, 0.52f, 0.35f},
    {22, "Absorption", "textures/ui/absorption_effect", 0.92f, 0.78f, 0.28f},
    {23, "Saturation", "textures/ui/saturation_effect", 0.82f, 0.48f, 0.20f},
    {24, "Levitation", "textures/ui/levitation_effect", 0.68f, 0.62f, 0.92f},
    {25, "Fatal Poison", "textures/ui/fatal_poison_effect", 0.20f, 0.54f, 0.18f},
    {26, "Conduit Power", "textures/ui/conduit_power_effect", 0.12f, 0.72f, 0.86f},
    {27, "Slow Falling", "textures/ui/slow_falling_effect", 0.72f, 0.82f, 0.96f},
    {28, "Bad Omen", "textures/ui/bad_omen_effect", 0.36f, 0.18f, 0.18f},
    {29, "Hero of the Village", "textures/ui/village_hero_effect", 0.86f, 0.64f, 0.28f},
    {30, "Darkness", "textures/ui/darkness_effect", 0.10f, 0.10f, 0.14f},
    {31, "Trial Omen", "textures/ui/trial_omen_effect", 0.42f, 0.32f, 0.66f},
    {32, "Raid Omen", "textures/ui/raid_omen_effect", 0.54f, 0.18f, 0.20f},
    {33, "Wind Charged", "textures/ui/wind_charged_effect", 0.62f, 0.82f, 0.92f},
    {34, "Weaving", "textures/ui/weaving_effect", 0.78f, 0.78f, 0.86f},
    {35, "Oozing", "textures/ui/oozing_effect", 0.30f, 0.62f, 0.28f},
    {36, "Infested", "textures/ui/infested_effect", 0.48f, 0.36f, 0.24f},
};

const EffectInfo* GetEffectInfo(int id) {
    for (const EffectInfo& info : kEffectInfos) {
        if (info.id == id) {
            return &info;
        }
    }
    return nullptr;
}

#include "EffectHUDResources.cpp"

#include "EffectHUDRender.cpp"

void InitializeEffectHudImages() {
    const bool extracted = ExtractEmbeddedEffectImagesToTemp();
    ResetEffectIconCachesForReextract();
    g_effectHudImagesInjectSucceeded.store(extracted, std::memory_order_relaxed);
    g_effectHudImagesInjected.store(true, std::memory_order_release);
}

void SetEffectHudPayloadModule(HMODULE module) {
    g_effectHudPayloadModule = module;
}

bool InstallEffectHudHooks() {
    if (g_effectHudHooksInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    HMODULE module = GetModuleHandleW(nullptr);
    const auto imageBase = reinterpret_cast<std::uintptr_t>(module);
    if (module == nullptr || imageBase == 0) {
        return false;
    }

    void* mobEffectsRendererRender = GetImageAddress(imageBase, kMobEffectsRendererRenderRva);
    if (!IsExecutableAddress(mobEffectsRendererRender)) {
        return false;
    }

    MH_STATUS createStatus = MH_CreateHook(
        mobEffectsRendererRender,
        &HookMobEffectsRendererRender,
        reinterpret_cast<void**>(&g_originalMobEffectsRendererRender));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    MH_STATUS enableStatus = MH_EnableHook(mobEffectsRendererRender);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        return false;
    }

    g_effectHudHooksInstalled.store(true, std::memory_order_release);
    return true;
}

bool IsEffectHudEnabled() {
    EnsureEffectHudConfigLoaded();
    return g_effectHudEnabled.load(std::memory_order_relaxed);
}

void SetEffectHudEnabled(bool enabled) {
    EnsureEffectHudConfigLoaded();
    g_effectHudEnabled.store(enabled, std::memory_order_relaxed);
    SaveEffectHudConfig();
}

bool IsEffectHudBackgroundEnabled() {
    EnsureEffectHudConfigLoaded();
    return g_effectHudBackgroundEnabled.load(std::memory_order_relaxed);
}

void SetEffectHudBackgroundEnabled(bool enabled) {
    EnsureEffectHudConfigLoaded();
    g_effectHudBackgroundEnabled.store(enabled, std::memory_order_relaxed);
    SaveEffectHudConfig();
}

bool GetEffectHudEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureEffectHudConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth > 1.0f ? g_cachedNativeWidth : displayWidth,
        g_cachedNativeHeight > 1.0f ? g_cachedNativeHeight : displayHeight);
    std::array<EffectHudEntry, kMaxEffectHudEntries> preview{};
    int previewCount = 0;
    FillPreviewEffects(preview, previewCount);

    const float scale = GetClampedEffectHudScale();
    const ImVec2 nativePosition = GetEffectiveEffectHudNativePosition(nativeScreen, preview, previewCount);
    const ImVec2 nativeSize = GetEffectHudNativeRectSize(scale, preview, previewCount);
    const float scaleX = displayWidth / nativeScreen.x;
    const float scaleY = displayHeight / nativeScreen.y;
    x = nativePosition.x * scaleX;
    y = nativePosition.y * scaleY;
    width = nativeSize.x * scaleX;
    height = nativeSize.y * scaleY;
    return true;
}

void SetEffectHudEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureEffectHudConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f) {
        return;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth > 1.0f ? g_cachedNativeWidth : displayWidth,
        g_cachedNativeHeight > 1.0f ? g_cachedNativeHeight : displayHeight);
    const float scaleX = nativeScreen.x / std::max(1.0f, displayWidth);
    const float scaleY = nativeScreen.y / std::max(1.0f, displayHeight);
    std::array<EffectHudEntry, kMaxEffectHudEntries> preview{};
    int previewCount = 0;
    FillPreviewEffects(preview, previewCount);
    const ImVec2 clamped =
        ClampEffectHudNativePosition(ImVec2(displayX * scaleX, displayY * scaleY), nativeScreen, preview, previewCount);

    g_effectHudCustomPosition.store(true, std::memory_order_relaxed);
    g_effectHudPositionX.store(clamped.x, std::memory_order_relaxed);
    g_effectHudPositionY.store(clamped.y, std::memory_order_relaxed);
    g_effectHudPositionDirty.store(true, std::memory_order_relaxed);
}

void MoveEffectHudEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetEffectHudEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetEffectHudEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetEffectHudEditorScale() {
    EnsureEffectHudConfigLoaded();
    return GetClampedEffectHudScale();
}

void SetEffectHudEditorScale(float scale) {
    EnsureEffectHudConfigLoaded();
    g_effectHudScale.store(std::clamp(scale, kMinEffectHudScale, kMaxEffectHudScale), std::memory_order_relaxed);
    g_effectHudPositionDirty.store(true, std::memory_order_relaxed);
}

void ResetEffectHudEditorPosition() {
    EnsureEffectHudConfigLoaded();
    g_effectHudCustomPosition.store(false, std::memory_order_relaxed);
    g_effectHudPositionX.store(kDefaultEffectHudX, std::memory_order_relaxed);
    g_effectHudPositionY.store(kDefaultEffectHudY, std::memory_order_relaxed);
    g_effectHudScale.store(kDefaultEffectHudScale, std::memory_order_relaxed);
    g_effectHudPositionDirty.store(true, std::memory_order_relaxed);
    SaveEffectHudConfig();
}

void CommitEffectHudEditorPosition() {
    EnsureEffectHudConfigLoaded();
    if (g_effectHudPositionDirty.load(std::memory_order_relaxed)) {
        SaveEffectHudConfig();
    }
}

void TickEffectHud(void* clientInstance) {
    EnsureEffectHudConfigLoaded();
    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (clientInstance == nullptr ||
        (!g_effectHudEnabled.load(std::memory_order_relaxed) && !positionEditorActive) ||
        (!positionEditorActive && !ShouldShowGuiOverlay())) {
        return;
    }

    std::array<EffectHudEntry, kMaxEffectHudEntries> entries{};
    int count = 0;
    if (positionEditorActive) {
        FillPreviewEffects(entries, count);
    } else {
        RefreshEffectCache(clientInstance);
        entries = g_cachedEffects;
        count = std::clamp(g_cachedEffectCount, 0, kMaxEffectHudEntries);
    }
    if (count <= 0) {
        g_overlayLineCount = 0;
        g_overlayBackgroundVisible = false;
        g_lastEffectNativeDrawTick = 0;
        return;
    }

    ImVec2 nativeScreen{};
    if (!ResolveNativeScreen(clientInstance, nativeScreen)) {
        return;
    }

    const float scale = GetClampedEffectHudScale();
    const ImVec2 nativePosition = GetEffectiveEffectHudNativePosition(nativeScreen, entries, count);
    const ImVec2 nativeSize = GetEffectHudNativeRectSize(scale, entries, count);
    UpdateOverlayMetrics(nativePosition, nativeScreen, entries, count);
    (void)nativeSize;
    g_lastEffectNativeDrawTick = GetTickCount64();
}

void RenderEffectHudOverlay() {
    EnsureEffectHudConfigLoaded();
    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (ImGui::GetCurrentContext() == nullptr ||
        (!g_effectHudEnabled.load(std::memory_order_relaxed) && !positionEditorActive) ||
        (!positionEditorActive && !ShouldShowGuiOverlay())) {
        return;
    }

    const DWORD64 now = GetTickCount64();
    if (!positionEditorActive &&
        (g_lastEffectNativeDrawTick == 0 || now - g_lastEffectNativeDrawTick > kEffectHudOverlayFreshMs)) {
        return;
    }

    if (positionEditorActive && (g_lastEffectNativeDrawTick == 0 || now - g_lastEffectNativeDrawTick > kEffectHudOverlayFreshMs)) {
        std::array<EffectHudEntry, kMaxEffectHudEntries> preview{};
        int previewCount = 0;
        FillPreviewEffects(preview, previewCount);
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 nativeScreen(
            g_cachedNativeWidth > 1.0f ? g_cachedNativeWidth : std::max(1.0f, io.DisplaySize.x),
            g_cachedNativeHeight > 1.0f ? g_cachedNativeHeight : std::max(1.0f, io.DisplaySize.y));
        const ImVec2 nativePosition = GetEffectiveEffectHudNativePosition(nativeScreen, preview, previewCount);
        UpdateOverlayMetrics(nativePosition, nativeScreen, preview, previewCount);
    }

    DrawOverlayLines();
}

void RenderEffectHudPreview(ImDrawList* drawList, const ImVec2& min) {
    EnsureEffectHudConfigLoaded();
    std::array<EffectHudEntry, kMaxEffectHudEntries> preview{};
    int previewCount = 0;
    FillPreviewEffects(preview, previewCount);
    DrawEffectHudLinesAt(drawList, min, preview, previewCount);
}

} // namespace tane::gui
