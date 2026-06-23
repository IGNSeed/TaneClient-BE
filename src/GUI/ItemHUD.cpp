#include <Windows.h>
#include <MinHook.h>
#include <imgui.h>

#include "Offsets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tane::movement {
void TickAutoSprint(void* clientInstance);
bool IsAutoSprintEnabled();
}

namespace tane::render {
void TickNameTags(void* clientInstance);
void TickFullbright(void* clientInstance);
void TickNoFog(void* clientInstance);
void TickHitbox(void* clientInstance);
void TickTracer(void* clientInstance);
bool IsNameTagsEnabled();
bool IsFullbrightEnabled();
bool IsNoFogEnabled();
bool IsHitboxEnabled();
bool IsTracerEnabled();
}

namespace tane::camera {
void TickZoom(void* clientInstance);
void TickFreeLook(void* clientInstance);
bool IsZoomEnabled();
bool IsFreeLookEnabled();
}

namespace tane::imgui_menu {
void TickInputBlock(void* clientInstance);
}

namespace tane::patch {
void TickForceCloseOreUi(void* clientInstance);
}

namespace tane::gui {
bool IsMenuOpen();
bool IsGuiMenuVisible();
bool IsGuiPositionEditorActive();
bool ShouldShowGuiOverlay();
bool CanRunGameplayModules();
std::uint64_t GetFpsFrameId();
void TickPing(void* clientInstance);
void TickTab(void* clientInstance);
bool IsPingEnabled();
bool IsTabEnabled();
bool IsArrowCounterEnabled();
void TickArrowCounter(void* clientInstance);
bool IsPotCounterEnabled();
void TickPotCounter(void* clientInstance);
bool IsEffectHudEnabled();
void TickEffectHud(void* clientInstance);
}

namespace tane::gui {
void DrawGuiTextWithShadow(
    ImDrawList* drawList,
    ImFont* font,
    float fontSize,
    const ImVec2& pos,
    ImU32 color,
    const char* text,
    float shadowOffset,
    ImU32 shadowColor) {
    if (drawList == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    const float offset = std::max(1.0f, shadowOffset);
    drawList->AddText(font, fontSize, ImVec2(pos.x + offset, pos.y + offset), shadowColor, text);
    drawList->AddText(font, fontSize, pos, color, text);
}

namespace {

using namespace tane::offsets::item_hud;

struct Vec2 {
    float x;
    float y;
};

struct GuiScreenMetrics {
    Vec2 screenSize;
    Vec2 scaledSize;
    bool valid;
};

struct MceColor {
    float r;
    float g;
    float b;
    float a;
};

struct RectangleArea {
    float left;
    float right;
    float top;
    float bottom;
};

enum class TextAlignment : int {
    Left = 0,
    Right = 1,
    Center = 2,
};

struct TextMeasureData {
    float textSize = 8.0f;
    float linePadding = 0.0f;
    bool displayShadow = false;
    bool showColorSymbols = false;
    bool hideHyphen = false;
};

struct CaretMeasureData {
    int position = -1;
    bool isSingleline = true;
};

struct OptionalFloat {
    float value = 0.0f;
    bool hasValue = false;
    std::uint8_t padding[3]{};
};

struct ItemStackRenderState {
    void* itemHolder = nullptr;
    std::uint16_t aux = 0;
    std::uint8_t count = 0;
    std::uint8_t valid = 0;
};

struct HudRenderSlot {
    std::array<std::uint8_t, kItemRenderEntrySize> renderEntry{};
    void* sourceItemStack = nullptr;
    ItemStackRenderState liveState{};
    std::string countText;
    std::string durabilityText;
    MceColor durabilityTextColor{1.0f, 1.0f, 1.0f, 1.0f};
    int currentDurability = 0;
    int maxDurability = 0;
    bool hasItem = false;
    bool liveStateValid = false;
    bool durabilityStateDirty = true;
};

struct HudTextOverlayEntry {
    std::string text;
    MceColor color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec2 nativePosition{0.0f, 0.0f};
    float nativeTextSize = 8.0f;
    bool alignRight = false;
};

struct HudDurabilityBarOverlayEntry {
    Vec2 nativePosition{0.0f, 0.0f};
    float nativeScale = 1.0f;
    int currentDurability = 0;
    int maxDurability = 0;
};

enum class ItemHudLayout : int {
    Vertical = 0,
    Horizontal = 1,
};

enum class ItemHudDurabilityStyle : int {
    Text = 0,
    Bar = 1,
};

using ScreenViewSetupAndRenderFn = void(__fastcall*)(void* screenView, void* minecraftUiRenderContext);
using RenderDecoratedGuiItemFn = void(__fastcall*)(
    void* renderItemQueue,
    void* minecraftUiRenderContext,
    void* clientInstance,
    void* itemStack,
    float x,
    float y,
    float opacity,
    float scale,
    int renderVariant);
using BaseActorRenderContextCtorFn = void*(__fastcall*)(void* baseActorRenderContext, void* screenContext, void* clientInstance, void* minecraftGame);
using BaseActorRenderContextDtorFn = void(__fastcall*)(void* baseActorRenderContext);
using BaseActorRenderContextGetItemRendererFn = void*(__fastcall*)(void* baseActorRenderContext);
using ItemRendererRenderGuiItemNewFn = void(__fastcall*)(
    void* itemRenderer,
    void* baseActorRenderContext,
    void* itemStack,
    int mode,
    float x,
    float y,
    bool enchanted,
    float opacity,
    float a9,
    float scale,
    int renderVariant);
using ItemStackIsEnchantedFn = bool(__fastcall*)(void* itemStack);
using ItemStackGetDamageValueFn = short(__fastcall*)(void* itemStack);
using ItemStackGetDurabilityItemFn = void*(__fastcall*)(void* itemStack);
using GetDurabilityComponentKeyFn = void*(__fastcall*)();
using ItemGetComponentFn = void*(__fastcall*)(void* item, void* componentKey);
using ItemGetLegacyMaxDamageFn = short(__fastcall*)(void* item);
using GetArmorItemStackFn = void*(__fastcall*)(void* actor, int slot);
using GetCarriedItemFn = void*(__fastcall*)(void* actor);
using InventoryGetItemFn = void*(__fastcall*)(void* inventory, int slot);
using GetLocalPlayerFn = void*(__fastcall*)(void* clientInstance);
using ClientVfuncNoArgFn = void*(__fastcall*)(void* clientInstance);
using ItemResolveGuiRenderModeFn = int(__fastcall*)(
    void* item,
    void* guiItemContext,
    std::uint64_t unknown,
    void* itemStack,
    bool decorated);
struct TextHolder {
    union {
        char inlineText[16];
        char* heapText;
    };
    std::size_t textLength;
    std::size_t alignedTextLength;

    explicit TextHolder(const char* text) : inlineText{}, textLength(0), alignedTextLength(0) {
        if (text == nullptr) {
            return;
        }

        textLength = std::strlen(text);
        alignedTextLength = textLength | 0xF;
        if (textLength < sizeof(inlineText)) {
            std::memcpy(inlineText, text, textLength);
            if (textLength + 1 < sizeof(inlineText)) {
                inlineText[textLength] = '\0';
            }
        } else {
            std::size_t allocationSize = textLength;
            if (allocationSize + 1 >= 0x1000) {
                allocationSize += 8;
            }

            heapText = static_cast<char*>(std::malloc(allocationSize + 1));
            alignedTextLength = allocationSize;
            if (heapText == nullptr) {
                textLength = 0;
                alignedTextLength = 0;
                return;
            }

            if (allocationSize + 1 >= 0x1000) {
                *reinterpret_cast<char**>(heapText) = heapText;
                heapText += 8;
            }
            std::memcpy(heapText, text, textLength);
            heapText[textLength] = '\0';
        }
    }

    ~TextHolder() {
        if (alignedTextLength >= sizeof(inlineText) && heapText != nullptr) {
            char* allocation = heapText;
            if (alignedTextLength + 1 >= 0x1000) {
                allocation = *reinterpret_cast<char**>(heapText - 8);
            }
            std::free(allocation);
        }
    }

    TextHolder(const TextHolder&) = delete;
    TextHolder& operator=(const TextHolder&) = delete;
};
using ItemRegistryLookupByNameFn = void***(__fastcall*)(void* resultA, void* resultB, TextHolder& name);
struct GameHashedString {
    std::uint64_t hash = 0;
    std::string text;
    mutable const GameHashedString* lastMatch = nullptr;

    GameHashedString() = default;

    explicit GameHashedString(const char* value) : text(value != nullptr ? value : "") {
        computeHash();
    }

    void computeHash() {
        constexpr std::uint64_t kFnvOffsetBasis = 0xCBF29CE484222325ull;
        constexpr std::uint64_t kFnvPrime = 0x100000001B3ull;

        if (text.empty()) {
            hash = 0;
            return;
        }

        std::uint64_t value = kFnvOffsetBasis;
        for (char c : text) {
            value *= kFnvPrime;
            value ^= static_cast<unsigned char>(c);
        }
        hash = value;
    }
};

struct GameHashedStringHash {
    std::size_t operator()(const GameHashedString& value) const noexcept {
        return static_cast<std::size_t>(value.hash);
    }
};

struct GameHashedStringEqual {
    bool operator()(const GameHashedString& left, const GameHashedString& right) const noexcept {
        return left.hash == right.hash && left.text == right.text;
    }
};

struct GameItemRegistryRef {
    std::weak_ptr<void> weakRegistry;
};

using ItemNameMap = std::unordered_map<GameHashedString, std::uintptr_t, GameHashedStringHash, GameHashedStringEqual>;
using DrawDebugTextFn = void(__fastcall*)(
    void* minecraftUiRenderContext,
    const RectangleArea& rect,
    const std::string& text,
    const MceColor& color,
    float alpha,
    TextAlignment alignment,
    const TextMeasureData& textMeasureData,
    const CaretMeasureData& caretMeasureData);
using FlushTextFn = void(__fastcall*)(void* minecraftUiRenderContext, float deltaTime, OptionalFloat obfuscateSwitchTime);
using FillRectangleFn = void(__fastcall*)(void* minecraftUiRenderContext, const RectangleArea& rect, const MceColor& color, float alpha);

static_assert(sizeof(GameHashedString) == 48, "Game HashedString layout changed");
static_assert(sizeof(GameItemRegistryRef) == 16, "Game ItemRegistryRef layout changed");

ScreenViewSetupAndRenderFn g_originalScreenViewSetupAndRender = nullptr;
RenderDecoratedGuiItemFn g_renderDecoratedGuiItem = nullptr;
BaseActorRenderContextCtorFn g_baseActorRenderContextCtor = nullptr;
BaseActorRenderContextDtorFn g_baseActorRenderContextDtor = nullptr;
BaseActorRenderContextGetItemRendererFn g_baseActorRenderContextGetItemRenderer = nullptr;
ItemRendererRenderGuiItemNewFn g_itemRendererRenderGuiItemNew = nullptr;
ItemStackIsEnchantedFn g_itemStackIsEnchanted = nullptr;
ItemStackGetDamageValueFn g_itemStackGetDamageValue = nullptr;
ItemStackGetDurabilityItemFn g_itemStackGetDurabilityItem = nullptr;
GetDurabilityComponentKeyFn g_getDurabilityComponentKey = nullptr;
GetArmorItemStackFn g_getArmorItemStack = nullptr;
ItemRegistryLookupByNameFn g_itemRegistryLookupByName = nullptr;
std::uintptr_t g_getLocalPlayerVtableOffset = 0;
std::uintptr_t g_cachedLocalPlayerVtableOffset = 0;
void* g_cachedDurabilityComponentKey = nullptr;
void** g_cachedTextRenderContextVtable = nullptr;
DrawDebugTextFn g_cachedDrawDebugText = nullptr;
FlushTextFn g_cachedFlushText = nullptr;
void** g_cachedRectangleRenderContextVtable = nullptr;
FillRectangleFn g_cachedFillRectangle = nullptr;
void** g_failedTextRenderContextVtable = nullptr;
void** g_cachedInventoryVtable = nullptr;
InventoryGetItemFn g_cachedInventoryGetItem = nullptr;

struct InventoryCounterFrameCache {
    std::uint64_t frameId = 0;
    void* clientInstance = nullptr;
    void* localPlayer = nullptr;
    void* inventory = nullptr;
    DWORD64 lastFullScanMs = 0;
    int selectedSlot = -1;
    bool valid = false;
    bool hasTrackedStacks = false;
    int arrowCount = 0;
    int healingSplashCount = 0;
};

enum class InventoryCounterKind : std::uint8_t {
    None,
    Arrow,
    HealingSplash,
};

struct InventoryCounterTrackedStack {
    void* itemStack = nullptr;
    void* itemHolder = nullptr;
    std::uint16_t aux = 0;
    std::uint8_t count = 0;
    InventoryCounterKind kind = InventoryCounterKind::None;
    bool valid = false;
};

InventoryCounterFrameCache g_inventoryCounterFrameCache{};
std::array<InventoryCounterTrackedStack, kPlayerInventoryMainSlotCount> g_inventoryCounterTrackedStacks{};
std::unordered_map<std::uintptr_t, int> g_itemMaxDamageCache;

constexpr DWORD64 kGuiOverlayVisibilityGraceMs = 250;
constexpr DWORD64 kGuiOverlayBlockingGraceMs = 48;
constexpr DWORD64 kFormGuiOverlayBlockingGraceMs = 1500;
constexpr DWORD64 kClientTransitionBlockingGraceMs = 1500;
constexpr DWORD64 kHudScreenMetricsRetryMs = 500;
constexpr DWORD64 kInventoryCounterFullScanRefreshMs = 750;
constexpr DWORD64 kHudLiveStateRefreshMs = 50;
constexpr std::uint16_t kHealingSplashPotionAux = 22;

std::atomic_bool g_itemHudEnabled = true;
std::atomic_bool g_hooksInstalled = false;
std::atomic_bool g_localPlayerSeen = false;
std::atomic<DWORD64> g_guiOverlayAllowedUntilTick = 0;
std::atomic<DWORD64> g_guiOverlayBlockedUntilTick = 0;
std::uint64_t g_lastClientTickFpsFrameId = UINT64_MAX;

std::atomic_bool g_itemHudPositionConfigLoaded = false;
std::atomic_bool g_itemHudCustomPosition = false;
std::atomic_bool g_itemHudDurabilityCustomPosition = false;
std::atomic_bool g_itemHudPositionDirty = false;
std::atomic_bool g_itemHudShowDurability = true;
std::atomic<int> g_itemHudLayout = static_cast<int>(ItemHudLayout::Vertical);
std::atomic<int> g_itemHudDurabilityStyle = static_cast<int>(ItemHudDurabilityStyle::Text);
std::atomic<float> g_itemHudPositionX = 0.0f;
std::atomic<float> g_itemHudPositionY = 0.0f;
std::atomic<float> g_itemHudDurabilityPositionX = 0.0f;
std::atomic<float> g_itemHudDurabilityPositionY = 0.0f;
std::atomic<float> g_itemHudScale = 1.0f;
std::atomic<std::uint64_t> g_uiFrameId = 0;
void* g_currentUiFrameClientInstance = nullptr;
std::uint64_t g_currentUiFrameLocalPlayerFrameId = 0;
void* g_currentUiFrameLocalPlayerClientInstance = nullptr;
void* g_currentUiFrameLocalPlayer = nullptr;
bool g_currentUiFrameLocalPlayerResolved = false;
void* g_cachedMinecraftGameClientInstance = nullptr;
void* g_cachedMinecraftGame = nullptr;

std::array<HudRenderSlot, kItemSlotCount> g_cachedHudSlots{};
std::array<HudTextOverlayEntry, kItemSlotCount * 2> g_cachedHudTextOverlayEntries{};
std::array<HudDurabilityBarOverlayEntry, kItemSlotCount> g_cachedHudDurabilityBarOverlayEntries{};
std::size_t g_cachedHudTextOverlayEntryCount = 0;
std::size_t g_cachedHudDurabilityBarOverlayEntryCount = 0;
std::size_t g_cachedHudSlotCount = 0;
bool g_cachedHudCacheValid = false;
bool g_cachedHudEditorPreview = false;
Vec2 g_cachedHudScreenSize{854.0f, 480.0f};
Vec2 g_cachedHudPixelScreenSize{854.0f, 480.0f};
bool g_cachedHudScreenMetricsValid = false;
void* g_cachedHudClientInstance = nullptr;
void* g_cachedHudLocalPlayer = nullptr;
void* g_cachedHudScreenView = nullptr;
void* g_lastHudScreenMetricsClientInstance = nullptr;
DWORD64 g_lastHudScreenMetricsAttemptMs = 0;
DWORD64 g_lastHudCacheRefreshMs = 0;
DWORD64 g_lastHudLiveStateRefreshMs = 0;
DWORD64 g_lastClientWorldValidationMs = 0;
void* g_lastValidatedClientInstance = nullptr;
bool g_lastClientWorldValidationResult = false;
std::uint64_t g_lastHudDrawFrameId = 0;
DWORD64 g_lastGuiOverlayVisibilityTick = 0;
bool g_lastGuiOverlayVisibility = false;
int g_emptyHudRefreshCount = 0;
std::size_t g_cachedPlayerInventoryOffset = 0;
int g_cachedSelectedSlot = -1;

struct DirectGuiItemFrameContext {
    std::uint64_t frameId = 0;
    void* minecraftUiRenderContext = nullptr;
    void* clientInstance = nullptr;
    void* baseActorRenderContext = nullptr;
    void* itemRenderer = nullptr;
    void* guiItemContext = nullptr;
    bool active = false;
    alignas(16) std::array<std::uint8_t, 0x500> storage{};
};

DirectGuiItemFrameContext g_directGuiItemFrameContext{};

Vec2 g_cachedHudTextItemPosition{-1.0f, -1.0f};
Vec2 g_cachedHudTextDurabilityPosition{-1.0f, -1.0f};
float g_cachedHudTextScale = -1.0f;
bool g_cachedHudTextShowDurability = false;
std::size_t g_cachedHudTextSlotCountForLayout = 0;
int g_cachedHudTextLayout = -1;
int g_cachedHudTextDurabilityStyle = -1;
constexpr std::array<const char*, kItemSlotCount> kEditorPreviewItemNames = {
    "diamond_sword",
    "diamond_helmet",
    "diamond_chestplate",
    "diamond_leggings",
    "diamond_boots",
};
constexpr std::array<const char*, kItemSlotCount> kEditorPreviewNamespacedItemNames = {
    "minecraft:diamond_sword",
    "minecraft:diamond_helmet",
    "minecraft:diamond_chestplate",
    "minecraft:diamond_leggings",
    "minecraft:diamond_boots",
};
std::array<void*, kItemSlotCount> g_editorPreviewItems{};
std::array<void*, kItemSlotCount> g_editorPreviewItemHolders{};
bool g_editorPreviewItemScanAttempted = false;

#include "ItemHUDConfig.cpp"

#include "ItemHUDResolve.cpp"

#include "ItemHUDCache.cpp"

#include "ItemHUDRender.cpp"

bool IsItemHudEnabled() {
    EnsureItemHudPositionConfigLoaded();
    return g_itemHudEnabled.load(std::memory_order_relaxed);
}

bool ShouldShowGuiOverlay() {
    if (IsGuiPositionEditorActive()) {
        return true;
    }

    const DWORD64 now = GetTickCount64();
    if (g_lastGuiOverlayVisibilityTick == now) {
        return g_lastGuiOverlayVisibility;
    }

    bool visible = true;
    if (IsMenuOpen() || IsGuiMenuVisible()) {
        visible = false;
    } else if (now <= g_guiOverlayBlockedUntilTick.load(std::memory_order_relaxed)) {
        visible = false;
    } else {
        visible = now <= g_guiOverlayAllowedUntilTick.load(std::memory_order_relaxed);
    }

    g_lastGuiOverlayVisibilityTick = now;
    g_lastGuiOverlayVisibility = visible;
    return visible;
}

bool CanRunGameplayModules() {
    if (IsMenuOpen()) {
        return false;
    }

    const DWORD64 now = GetTickCount64();
    return now > g_guiOverlayBlockedUntilTick.load(std::memory_order_relaxed);
}

void SetItemHudEnabled(bool enabled) {
    EnsureItemHudPositionConfigLoaded();
    g_itemHudEnabled.store(enabled, std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
    SaveItemHudPositionConfig();
}

bool IsItemHudDurabilityVisible() {
    EnsureItemHudPositionConfigLoaded();
    return g_itemHudShowDurability.load(std::memory_order_relaxed);
}

void SetItemHudDurabilityVisible(bool visible) {
    EnsureItemHudPositionConfigLoaded();
    g_itemHudShowDurability.store(visible, std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
    SaveItemHudPositionConfig();
}

int GetItemHudLayout() {
    EnsureItemHudPositionConfigLoaded();
    return static_cast<int>(GetItemHudLayoutValue());
}

void SetItemHudLayout(int layout) {
    EnsureItemHudPositionConfigLoaded();
    g_itemHudLayout.store(std::clamp(layout, 0, 1), std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
    SaveItemHudPositionConfig();
}

int GetItemHudDurabilityStyle() {
    EnsureItemHudPositionConfigLoaded();
    return static_cast<int>(GetItemHudDurabilityStyleValue());
}

void SetItemHudDurabilityStyle(int style) {
    EnsureItemHudPositionConfigLoaded();
    g_itemHudDurabilityStyle.store(std::clamp(style, 0, 1), std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
    SaveItemHudPositionConfig();
}

bool GetItemHudEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureItemHudPositionConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f) {
        return false;
    }

    Vec2 nativeScreen = g_cachedHudScreenSize;
    if (nativeScreen.x <= 0.0f || nativeScreen.y <= 0.0f) {
        nativeScreen = Vec2{displayWidth, displayHeight};
    }

    const bool editorActive = tane::gui::IsGuiPositionEditorActive();
    const std::size_t slotCount = editorActive ? kItemSlotCount : std::max<std::size_t>(1, g_cachedHudSlotCount != 0 ? g_cachedHudSlotCount : kItemSlotCount);
    const Vec2 nativePosition = GetEffectiveItemHudPosition(slotCount, nativeScreen);
    const float scaleX = displayWidth / std::max(1.0f, nativeScreen.x);
    const float scaleY = displayHeight / std::max(1.0f, nativeScreen.y);
    const float hudScale = GetClampedItemHudScale();

    x = nativePosition.x * scaleX;
    y = nativePosition.y * scaleY;
    width = GetItemHudItemsNativeWidth(slotCount, hudScale) * scaleX;
    height = GetItemHudItemsNativeHeight(slotCount, hudScale) * scaleY;
    return true;
}

bool GetItemHudDurabilityEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureItemHudPositionConfigLoaded();
    if (ShouldUseItemHudDurabilityBar()) {
        return false;
    }
    if (displayWidth <= 0.0f || displayHeight <= 0.0f) {
        return false;
    }

    Vec2 nativeScreen = g_cachedHudScreenSize;
    if (nativeScreen.x <= 0.0f || nativeScreen.y <= 0.0f) {
        nativeScreen = Vec2{displayWidth, displayHeight};
    }

    const bool editorActive = tane::gui::IsGuiPositionEditorActive();
    const std::size_t slotCount = editorActive ? kItemSlotCount : std::max<std::size_t>(1, g_cachedHudSlotCount != 0 ? g_cachedHudSlotCount : kItemSlotCount);
    const Vec2 itemPosition = GetEffectiveItemHudPosition(slotCount, nativeScreen);
    const Vec2 durabilityPosition = GetEffectiveItemHudDurabilityPosition(slotCount, nativeScreen, itemPosition);
    const float scaleX = displayWidth / std::max(1.0f, nativeScreen.x);
    const float scaleY = displayHeight / std::max(1.0f, nativeScreen.y);
    const float hudScale = GetClampedItemHudScale();

    x = durabilityPosition.x * scaleX;
    y = durabilityPosition.y * scaleY;
    width = GetItemHudDurabilityNativeWidth(slotCount, hudScale) * scaleX;
    height = GetItemHudDurabilityNativeHeight(slotCount, hudScale) * scaleY;
    return true;
}

void SetItemHudEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureItemHudPositionConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f) {
        return;
    }

    Vec2 nativeScreen = g_cachedHudScreenSize;
    if (nativeScreen.x <= 0.0f || nativeScreen.y <= 0.0f) {
        nativeScreen = Vec2{displayWidth, displayHeight};
    }

    const bool editorActive = tane::gui::IsGuiPositionEditorActive();
    const std::size_t slotCount = editorActive ? kItemSlotCount : std::max<std::size_t>(1, g_cachedHudSlotCount != 0 ? g_cachedHudSlotCount : kItemSlotCount);
    const float scaleX = nativeScreen.x / std::max(1.0f, displayWidth);
    const float scaleY = nativeScreen.y / std::max(1.0f, displayHeight);
    const Vec2 clamped = ClampItemHudPosition(Vec2{displayX * scaleX, displayY * scaleY}, slotCount, nativeScreen);
    g_itemHudCustomPosition.store(true, std::memory_order_relaxed);
    g_itemHudPositionX.store(clamped.x, std::memory_order_relaxed);
    g_itemHudPositionY.store(clamped.y, std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
}

void SetItemHudDurabilityEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureItemHudPositionConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f) {
        return;
    }

    Vec2 nativeScreen = g_cachedHudScreenSize;
    if (nativeScreen.x <= 0.0f || nativeScreen.y <= 0.0f) {
        nativeScreen = Vec2{displayWidth, displayHeight};
    }

    const bool editorActive = tane::gui::IsGuiPositionEditorActive();
    const std::size_t slotCount = editorActive ? kItemSlotCount : std::max<std::size_t>(1, g_cachedHudSlotCount != 0 ? g_cachedHudSlotCount : kItemSlotCount);
    const float scaleX = nativeScreen.x / std::max(1.0f, displayWidth);
    const float scaleY = nativeScreen.y / std::max(1.0f, displayHeight);
    const Vec2 clamped = ClampItemHudDurabilityPosition(Vec2{displayX * scaleX, displayY * scaleY}, slotCount, nativeScreen);
    g_itemHudDurabilityCustomPosition.store(true, std::memory_order_relaxed);
    g_itemHudDurabilityPositionX.store(clamped.x, std::memory_order_relaxed);
    g_itemHudDurabilityPositionY.store(clamped.y, std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
}

float GetItemHudEditorScale() {
    EnsureItemHudPositionConfigLoaded();
    return GetClampedItemHudScale();
}

void SetItemHudEditorScale(float scale) {
    EnsureItemHudPositionConfigLoaded();
    g_itemHudScale.store(std::clamp(scale, 0.55f, 2.75f), std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
}

void MoveItemHudEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetItemHudEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetItemHudEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

void MoveItemHudDurabilityEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetItemHudDurabilityEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetItemHudDurabilityEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

void ResetItemHudEditorPosition() {
    EnsureItemHudPositionConfigLoaded();
    g_itemHudCustomPosition.store(false, std::memory_order_relaxed);
    g_itemHudDurabilityCustomPosition.store(false, std::memory_order_relaxed);
    g_itemHudPositionX.store(0.0f, std::memory_order_relaxed);
    g_itemHudPositionY.store(0.0f, std::memory_order_relaxed);
    g_itemHudDurabilityPositionX.store(0.0f, std::memory_order_relaxed);
    g_itemHudDurabilityPositionY.store(0.0f, std::memory_order_relaxed);
    g_itemHudScale.store(1.0f, std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
    SaveItemHudPositionConfig();
}

void ResetItemHudDurabilityEditorPosition() {
    EnsureItemHudPositionConfigLoaded();
    g_itemHudDurabilityCustomPosition.store(false, std::memory_order_relaxed);
    g_itemHudDurabilityPositionX.store(0.0f, std::memory_order_relaxed);
    g_itemHudDurabilityPositionY.store(0.0f, std::memory_order_relaxed);
    g_itemHudPositionDirty.store(true, std::memory_order_relaxed);
    SaveItemHudPositionConfig();
}

void CommitItemHudEditorPosition() {
    EnsureItemHudPositionConfigLoaded();
    if (g_itemHudPositionDirty.load(std::memory_order_relaxed)) {
        SaveItemHudPositionConfig();
    }
}

bool InstallItemHudHooks() {
    if (g_hooksInstalled.load(std::memory_order_relaxed)) {
        return true;
    }

    HMODULE module = GetModuleHandleW(nullptr);
    const auto imageBase = reinterpret_cast<std::uintptr_t>(module);
    if (module == nullptr || imageBase == 0) {
        return false;
    }

    void* screenViewSetupAndRender = GetImageAddress(imageBase, kScreenViewSetupAndRenderRva);
    void* renderDecoratedGuiItem = GetImageAddress(imageBase, kRenderDecoratedGuiItemRva);

    g_getArmorItemStack = reinterpret_cast<GetArmorItemStackFn>(GetImageAddress(imageBase, kGetArmorItemStackRva));
    g_renderDecoratedGuiItem = reinterpret_cast<RenderDecoratedGuiItemFn>(renderDecoratedGuiItem);
    g_baseActorRenderContextCtor =
        reinterpret_cast<BaseActorRenderContextCtorFn>(GetImageAddress(imageBase, kBaseActorRenderContextCtorRva));
    void* baseActorRenderContextDtor = GetImageAddress(imageBase, kBaseActorRenderContextDtorRva);
    g_baseActorRenderContextDtor =
        reinterpret_cast<BaseActorRenderContextDtorFn>(baseActorRenderContextDtor);
    g_baseActorRenderContextGetItemRenderer =
        reinterpret_cast<BaseActorRenderContextGetItemRendererFn>(GetImageAddress(imageBase, kBaseActorRenderContextGetItemRendererRva));
    g_itemRendererRenderGuiItemNew =
        reinterpret_cast<ItemRendererRenderGuiItemNewFn>(GetImageAddress(imageBase, kItemRendererRenderGuiItemNewRva));
    g_itemStackIsEnchanted = reinterpret_cast<ItemStackIsEnchantedFn>(GetImageAddress(imageBase, kItemStackIsEnchantedRva));
    g_itemStackGetDamageValue =
        reinterpret_cast<ItemStackGetDamageValueFn>(GetImageAddress(imageBase, kItemStackGetDamageValueRva));
    g_itemStackGetDurabilityItem =
        reinterpret_cast<ItemStackGetDurabilityItemFn>(GetImageAddress(imageBase, kItemStackGetDurabilityItemRva));
    g_getDurabilityComponentKey =
        reinterpret_cast<GetDurabilityComponentKeyFn>(GetImageAddress(imageBase, kGetDurabilityComponentKeyRva));
    g_getLocalPlayerVtableOffset = ResolveLocalPlayerVtableOffset(module);

    if (!IsExecutableAddress(screenViewSetupAndRender)) {
        return false;
    }

    if (!IsExecutableAddress(reinterpret_cast<void*>(g_getArmorItemStack))) {
        g_getArmorItemStack = nullptr;
    }
    if (!IsExecutableAddress(reinterpret_cast<void*>(g_renderDecoratedGuiItem))) {
        g_renderDecoratedGuiItem = nullptr;
    }
    const bool directItemRenderingReady =
        IsExecutableAddress(reinterpret_cast<void*>(g_baseActorRenderContextCtor)) &&
        IsExecutableAddress(reinterpret_cast<void*>(g_baseActorRenderContextDtor)) &&
        IsExecutableAddress(reinterpret_cast<void*>(g_itemRendererRenderGuiItemNew));
    if (!directItemRenderingReady) {
        g_baseActorRenderContextCtor = nullptr;
        g_baseActorRenderContextDtor = nullptr;
        g_baseActorRenderContextGetItemRenderer = nullptr;
        g_itemRendererRenderGuiItemNew = nullptr;
    }

    if (!IsExecutableAddress(reinterpret_cast<void*>(g_itemStackIsEnchanted))) {
        g_itemStackIsEnchanted = nullptr;
    }
    if (!IsExecutableAddress(reinterpret_cast<void*>(g_itemStackGetDamageValue))) {
        g_itemStackGetDamageValue = nullptr;
    }
    if (!IsExecutableAddress(reinterpret_cast<void*>(g_itemStackGetDurabilityItem))) {
        g_itemStackGetDurabilityItem = nullptr;
    }
    if (!IsExecutableAddress(reinterpret_cast<void*>(g_getDurabilityComponentKey))) {
        g_getDurabilityComponentKey = nullptr;
    }

    if (g_renderDecoratedGuiItem == nullptr) {
        return false;
    }

    if (MH_CreateHook(
            screenViewSetupAndRender,
            &HookScreenViewSetupAndRender,
            reinterpret_cast<void**>(&g_originalScreenViewSetupAndRender)) != MH_OK) {
        return false;
    }

    if (MH_EnableHook(screenViewSetupAndRender) != MH_OK) {
        MH_RemoveHook(screenViewSetupAndRender);
        return false;
    }

    g_hooksInstalled.store(true, std::memory_order_release);
    return true;
}

} // namespace tane::gui
