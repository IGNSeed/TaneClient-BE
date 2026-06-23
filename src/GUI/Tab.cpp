#include <Windows.h>
#include <imgui.h>

#include "Offsets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace tane::payload {
ImFont* GetItemHudFont();
bool GetRgbaTextureFromMemory(const char* key, const unsigned char* pixels, int width, int height, ImTextureRef& texture, ImVec2& size);
std::uint32_t GetExtendedControllerFlags();
}

namespace tane::gui {
bool IsMenuOpen();
bool ShouldShowGuiOverlay();
bool CanRunGameplayModules();
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

using namespace tane::offsets::tab;

struct MceUuid {
    std::uint64_t mostSig = 0;
    std::uint64_t leastSig = 0;

    bool operator==(const MceUuid& other) const {
        return mostSig == other.mostSig && leastSig == other.leastSig;
    }
};

struct BlobView {
    void* deleter = nullptr;
    unsigned char* data = nullptr;
    std::size_t size = 0;
};

struct SkinImage {
    int imageFormat = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 0;
    unsigned char usage = 0;
    unsigned char padding[7]{};
    BlobView bytes{};
};

struct PlayerSkinPrefix {
    std::string id;
    std::string playFabId;
    std::string fullId;
    std::string resourcePatch;
    std::string defaultGeometryName;
    SkinImage skinImage;
};

struct PlayerSkinShared {
    PlayerSkinPrefix* ptr = nullptr;
    void* control = nullptr;
};

struct PlayerListEntryCompat {
    std::uint64_t actorUniqueId = 0;
    MceUuid uuid{};
    std::string name;
    std::string xuid;
    std::string platformOnlineId;
    int buildPlatform = 0;
    PlayerSkinShared skin;
};

} // namespace
} // namespace tane::gui

namespace std {
template<>
struct hash<tane::gui::MceUuid> {
    std::size_t operator()(const tane::gui::MceUuid& uuid) const noexcept {
        return std::hash<std::uint64_t>{}(uuid.mostSig) ^ (std::hash<std::uint64_t>{}(uuid.leastSig) << 1);
    }
};
} // namespace std

namespace tane::gui {
namespace {

using PlayerMap = std::unordered_map<MceUuid, PlayerListEntryCompat>;
using GetLocalPlayerFn = void*(__fastcall*)(void* clientInstance);
using GetPlayerListFn = PlayerMap*(__fastcall*)(void* level);

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

struct CachedSkinHead;

struct TabPlayerEntry {
    std::string name;
    std::string textureKey;
    std::vector<unsigned char> headPixels;
    const CachedSkinHead* cachedHead = nullptr;
    int headWidth = 0;
    int headHeight = 0;
    bool hasHeadPixels = false;
};

struct CachedSkinHead {
    const unsigned char* data = nullptr;
    std::size_t size = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string textureKey;
    std::vector<unsigned char> pixels;
};

constexpr WORD kXInputGamepadLeftShoulder = 0x0100;
constexpr WORD kXInputGamepadRightShoulder = 0x0200;
constexpr WORD kXInputGamepadLeftThumb = 0x0040;
constexpr WORD kXInputGamepadRightThumb = 0x0080;
constexpr WORD kXInputGamepadDpadUp = 0x0001;
constexpr WORD kXInputGamepadDpadDown = 0x0002;
constexpr WORD kXInputGamepadDpadLeft = 0x0004;
constexpr WORD kXInputGamepadDpadRight = 0x0008;
constexpr WORD kXInputGamepadStart = 0x0010;
constexpr WORD kXInputGamepadBack = 0x0020;
constexpr WORD kXInputGamepadA = 0x1000;
constexpr WORD kXInputGamepadB = 0x2000;
constexpr WORD kXInputGamepadX = 0x4000;
constexpr WORD kXInputGamepadY = 0x8000;
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
constexpr int kVirtualKeyCount = 256;
constexpr int kDefaultKeyboardKeyA = VK_TAB;
constexpr int kDefaultKeyboardKeyB = 0;
constexpr std::uint32_t kDefaultControllerMask = kControllerBack;
constexpr ULONGLONG kControllerPollIntervalMs = 16;
constexpr ULONGLONG kDisconnectedXInputPollIntervalMs = 300;
constexpr ULONGLONG kPlayerRefreshMs = 1000;
constexpr std::size_t kMaxPlayers = 96;
constexpr std::size_t kMaxDisplayedPlayers = 80;

std::atomic_bool g_tabEnabled = false;
std::atomic_bool g_tabConfigLoaded = false;
std::atomic_bool g_tabHeld = false;
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

std::vector<TabPlayerEntry> g_cachedPlayers;
std::unordered_map<MceUuid, CachedSkinHead> g_cachedSkinHeads;
void* g_cachedPlayerClientInstance = nullptr;
ULONGLONG g_lastPlayerRefreshTick = 0;

#include "TabInput.cpp"

#include "TabPlayers.cpp"

bool IsTabEnabled() {
    EnsureTabConfigLoaded();
    return g_tabEnabled.load(std::memory_order_relaxed);
}

void SetTabEnabled(bool enabled) {
    EnsureTabConfigLoaded();
    g_tabEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        g_tabHeld.store(false, std::memory_order_relaxed);
    }
    SaveTabConfig();
}

const char* GetTabKeyboardComboLabel() {
    EnsureTabConfigLoaded();
    UpdateKeyboardComboLabel();
    return g_keyboardComboLabel;
}

const char* GetTabControllerComboLabel() {
    EnsureTabConfigLoaded();
    UpdateControllerComboLabel();
    return g_controllerComboLabel;
}

bool IsTabKeyboardComboCaptureActive() {
    return g_keyboardCaptureActive;
}

bool IsTabControllerComboCaptureActive() {
    return g_controllerCaptureActive;
}

bool IsTabControllerComboPolling() {
    return g_controllerCaptureActive;
}

void BeginTabKeyboardComboCapture() {
    EnsureTabConfigLoaded();
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_keyboardCaptureWaitingForRelease = true;
    g_keyboardCaptureActive = true;
    g_controllerCaptureActive = false;
    g_controllerCaptureCandidateMask = 0;
}

void BeginTabControllerComboCapture() {
    EnsureTabConfigLoaded();
    g_controllerCaptureActive = true;
    g_controllerCaptureWaitingForRelease = true;
    g_controllerCaptureCandidateMask = 0;
    g_keyboardCaptureActive = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
}

void CancelTabComboCapture() {
    g_keyboardCaptureActive = false;
    g_keyboardCaptureWaitingForRelease = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    g_controllerCaptureActive = false;
    g_controllerCaptureWaitingForRelease = false;
    g_controllerCaptureCandidateMask = 0;
}

bool HandleTabKeyMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    EnsureTabConfigLoaded();
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
            CancelTabComboCapture();
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
            if (HasKeyboardKeySet(g_keyboardCaptureCandidateKeys) && !HasKeyboardKeySet(g_keyboardCaptureDownKeys)) {
                StoreCapturedKeyboardCombo();
                g_keyboardCaptureActive = false;
                g_keyboardCaptureWaitingForRelease = false;
                ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
                ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
            }
        }
        return true;
    }

    if (!g_tabEnabled.load(std::memory_order_relaxed) || IsMenuOpen() || !CanRunGameplayModules()) {
        return false;
    }

    if (keyDown || keyUp || mouseDown || mouseUp) {
        const int resolvedVirtualKey = (keyDown || keyUp)
            ? ResolveMessageVirtualKey(wParam, lParam)
            : ResolveMouseButtonVirtualKey(message, wParam);
        return IsConfiguredKeyboardKey(resolvedVirtualKey);
    }

    return false;
}

void TickTabComboCapture() {
    EnsureTabConfigLoaded();
    TickControllerComboCapture();
}

void TickTab(void* clientInstance) {
    EnsureTabConfigLoaded();
    TickControllerComboCapture();
    if (!g_tabEnabled.load(std::memory_order_relaxed) ||
        clientInstance == nullptr ||
        !CanRunGameplayModules()) {
        g_tabHeld.store(false, std::memory_order_relaxed);
        return;
    }

    const bool held = IsKeyboardComboHeld() || IsControllerComboHeld(ReadControllerFlags());
    g_tabHeld.store(held, std::memory_order_relaxed);
    if (held) {
        RefreshPlayerCache(clientInstance, false);
    }
}

void RenderTabOverlay() {
    EnsureTabConfigLoaded();
    if (ImGui::GetCurrentContext() == nullptr ||
        !g_tabEnabled.load(std::memory_order_relaxed) ||
        !g_tabHeld.load(std::memory_order_relaxed) ||
        !ShouldShowGuiOverlay() ||
        g_cachedPlayers.empty()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImFont* font = GetTabFont();
    const float scale = std::clamp(io.DisplaySize.y / 1080.0f, 0.82f, 1.20f);
    const float fontSize = 15.0f * scale;
    const float titleFontSize = 16.5f * scale;
    const float rowHeight = 34.0f * scale;
    const float iconSize = 24.0f * scale;
    const float columnWidth = 188.0f * scale;
    const float headerHeight = 34.0f * scale;
    const int rowsPerColumn = std::clamp(static_cast<int>((io.DisplaySize.y * 0.42f - headerHeight) / rowHeight), 6, 18);
    const int playerCount = static_cast<int>(std::min<std::size_t>(g_cachedPlayers.size(), kMaxDisplayedPlayers));
    const int columns = std::clamp((playerCount + rowsPerColumn - 1) / rowsPerColumn, 1, 5);
    const int rows = std::min(rowsPerColumn, playerCount);
    const float width = std::min(io.DisplaySize.x - 32.0f, columnWidth * columns + 28.0f * scale);
    const float height = headerHeight + rowHeight * rows + 16.0f * scale;
    const ImVec2 min(std::floor((io.DisplaySize.x - width) * 0.5f), 72.0f * scale);
    const ImVec2 max(min.x + width, min.y + height);

    drawList->AddRectFilled(min, max, IM_COL32(8, 9, 11, 190), 8.0f * scale);
    drawList->AddRect(min, max, IM_COL32(255, 255, 255, 45), 8.0f * scale, 0, 1.0f);
    drawList->AddLine(
        ImVec2(min.x + 12.0f * scale, min.y + headerHeight),
        ImVec2(max.x - 12.0f * scale, min.y + headerHeight),
        IM_COL32(255, 255, 255, 32),
        1.0f);

    char title[64]{};
    std::snprintf(title, sizeof(title), "Players  %d", playerCount);
    const ImVec2 titleSize = CalcText(font, titleFontSize, title);
    DrawGuiTextWithShadow(
        drawList,
        font,
        titleFontSize,
        ImVec2(min.x + (width - titleSize.x) * 0.5f, min.y + 8.0f * scale),
        IM_COL32(248, 250, 252, 238),
        title,
        1.0f,
        IM_COL32(0, 0, 0, 150));

    const float contentX = min.x + 14.0f * scale;
    const float contentY = min.y + headerHeight + 8.0f * scale;
    const float actualColumnWidth = (width - 28.0f * scale) / static_cast<float>(columns);
    for (int index = 0; index < playerCount; ++index) {
        const int column = index / rowsPerColumn;
        const int row = index % rowsPerColumn;
        if (column >= columns) {
            break;
        }

        const TabPlayerEntry& entry = g_cachedPlayers[static_cast<std::size_t>(index)];
        const ImVec2 rowMin(contentX + actualColumnWidth * column, contentY + rowHeight * row);
        const ImVec2 rowMax(rowMin.x + actualColumnWidth - 8.0f * scale, rowMin.y + rowHeight - 4.0f * scale);
        if ((row % 2) == 0) {
            drawList->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 10), 4.0f * scale);
        }

        const ImVec2 headMin(rowMin.x + 7.0f * scale, rowMin.y + (rowHeight - iconSize) * 0.5f - 2.0f * scale);
        DrawPlayerHead(drawList, entry, font, headMin, iconSize);

        const float textX = headMin.x + iconSize + 8.0f * scale;
        const float textY = rowMin.y + (rowHeight - fontSize) * 0.5f - 2.0f * scale;
        const float textMaxX = rowMax.x - 8.0f * scale;
        const std::string displayName = FitTextToWidth(font, fontSize, entry.name, textMaxX - textX);
        DrawGuiTextWithShadow(
            drawList,
            font,
            fontSize,
            ImVec2(textX, textY),
            IM_COL32(244, 246, 248, 238),
            displayName.c_str(),
            1.0f,
            IM_COL32(0, 0, 0, 155));
    }
}

} // namespace tane::gui
