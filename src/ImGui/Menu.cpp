#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace tane::movement {
bool IsAutoSprintEnabled();
void SetAutoSprintEnabled(bool enabled);
}

namespace tane::render {
bool IsNameTagsEnabled();
void SetNameTagsEnabled(bool enabled);
bool IsFullbrightEnabled();
void SetFullbrightEnabled(bool enabled);
bool IsNoFogEnabled();
void SetNoFogEnabled(bool enabled);
bool IsHitboxEnabled();
void SetHitboxEnabled(bool enabled);
void GetHitboxColor(float& r, float& g, float& b, float& a);
void SetHitboxColor(float r, float g, float b, float a);
bool IsTracerEnabled();
void SetTracerEnabled(bool enabled);
void GetTracerLineColor(float& r, float& g, float& b, float& a);
void SetTracerLineColor(float r, float g, float b, float a);
}

namespace tane::camera {
bool IsZoomEnabled();
void SetZoomEnabled(bool enabled);
float GetZoomAmount();
void SetZoomAmount(float value);
bool IsZoomSmoothAnimationEnabled();
void SetZoomSmoothAnimationEnabled(bool enabled);
float GetZoomMinAmount();
float GetZoomMaxAmount();
const char* GetZoomKeyboardComboLabel();
const char* GetZoomControllerComboLabel();
bool IsZoomKeyboardComboCaptureActive();
bool IsZoomControllerComboCaptureActive();
void BeginZoomKeyboardComboCapture();
void BeginZoomControllerComboCapture();
void CancelZoomComboCapture();
void TickZoomComboCapture();
bool IsFreeLookEnabled();
void SetFreeLookEnabled(bool enabled);
const char* GetFreeLookKeyboardComboLabel();
const char* GetFreeLookControllerComboLabel();
bool IsFreeLookKeyboardComboCaptureActive();
bool IsFreeLookControllerComboCaptureActive();
void BeginFreeLookKeyboardComboCapture();
void BeginFreeLookControllerComboCapture();
void CancelFreeLookComboCapture();
void TickFreeLookComboCapture();
}

namespace tane::patch {
bool IsForceCloseOreUiEnabled();
void SetForceCloseOreUiEnabled(bool enabled);
}

namespace tane::payload {
ImTextureRef GetMenuLogoTexture();
ImVec2 GetMenuLogoTextureSize();
ImTextureRef GetItemHudPreviewTexture(int index);
ImVec2 GetItemHudPreviewTextureSize(int index);
ImFont* GetMenuRegularFont();
ImFont* GetMenuBoldFont();
}

namespace tane::gui {
bool IsMenuOpen();
void UpdateControllerMenuToggle();
bool ReadPositionEditorControllerInput(float& moveX, float& moveY, float& resize, bool& acceptPressed, bool& cancelPressed);
void ResetPositionEditorControllerInputState();
bool IsItemHudEnabled();
void SetItemHudEnabled(bool enabled);
bool IsItemHudDurabilityVisible();
void SetItemHudDurabilityVisible(bool visible);
int GetItemHudLayout();
void SetItemHudLayout(int layout);
int GetItemHudDurabilityStyle();
void SetItemHudDurabilityStyle(int style);
bool IsFpsEnabled();
void SetFpsEnabled(bool enabled);
bool IsFpsBackgroundEnabled();
void SetFpsBackgroundEnabled(bool enabled);
float GetFpsUpdateInterval();
void SetFpsUpdateInterval(float seconds);
float GetFpsMinUpdateInterval();
float GetFpsMaxUpdateInterval();
bool IsCpsEnabled();
void SetCpsEnabled(bool enabled);
bool IsCpsBackgroundEnabled();
void SetCpsBackgroundEnabled(bool enabled);
float GetCpsUpdateInterval();
void SetCpsUpdateInterval(float seconds);
float GetCpsMinUpdateInterval();
float GetCpsMaxUpdateInterval();
int GetCpsDisplayMode();
void SetCpsDisplayMode(int mode);
bool IsPingEnabled();
void SetPingEnabled(bool enabled);
bool IsPingBackgroundEnabled();
void SetPingBackgroundEnabled(bool enabled);
bool GetPingEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetPingEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MovePingEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetPingEditorScale();
void SetPingEditorScale(float scale);
void ResetPingEditorPosition();
void CommitPingEditorPosition();
bool IsArrowCounterEnabled();
void SetArrowCounterEnabled(bool enabled);
bool IsArrowCounterBackgroundEnabled();
void SetArrowCounterBackgroundEnabled(bool enabled);
bool IsPotCounterEnabled();
void SetPotCounterEnabled(bool enabled);
bool IsPotCounterBackgroundEnabled();
void SetPotCounterBackgroundEnabled(bool enabled);
bool IsEffectHudEnabled();
void SetEffectHudEnabled(bool enabled);
bool IsEffectHudBackgroundEnabled();
void SetEffectHudBackgroundEnabled(bool enabled);
bool IsKeyStrokeEnabled();
void SetKeyStrokeEnabled(bool enabled);
bool IsControllerOverlayEnabled();
void SetControllerOverlayEnabled(bool enabled);
bool IsTabEnabled();
void SetTabEnabled(bool enabled);
const char* GetTabKeyboardComboLabel();
const char* GetTabControllerComboLabel();
bool IsTabKeyboardComboCaptureActive();
bool IsTabControllerComboCaptureActive();
void BeginTabKeyboardComboCapture();
void BeginTabControllerComboCapture();
void CancelTabComboCapture();
void TickTabComboCapture();
float GetControllerOverlayMinVisualDeadZone();
float GetControllerOverlayMaxVisualDeadZone();
float GetControllerOverlayLeftVisualDeadZone();
void SetControllerOverlayLeftVisualDeadZone(float value);
float GetControllerOverlayRightVisualDeadZone();
void SetControllerOverlayRightVisualDeadZone(float value);
bool GetItemHudEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
bool GetItemHudDurabilityEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetItemHudEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void SetItemHudDurabilityEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MoveItemHudEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
void MoveItemHudDurabilityEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetItemHudEditorScale();
void SetItemHudEditorScale(float scale);
void ResetItemHudEditorPosition();
void ResetItemHudDurabilityEditorPosition();
void CommitItemHudEditorPosition();
bool GetFpsEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetFpsEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MoveFpsEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetFpsEditorScale();
void SetFpsEditorScale(float scale);
void ResetFpsEditorPosition();
void CommitFpsEditorPosition();
bool GetCpsEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetCpsEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MoveCpsEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetCpsEditorScale();
void SetCpsEditorScale(float scale);
void ResetCpsEditorPosition();
void CommitCpsEditorPosition();
bool GetArrowCounterEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetArrowCounterEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MoveArrowCounterEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetArrowCounterEditorScale();
void SetArrowCounterEditorScale(float scale);
void ResetArrowCounterEditorPosition();
void CommitArrowCounterEditorPosition();
bool GetPotCounterEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetPotCounterEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MovePotCounterEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetPotCounterEditorScale();
void SetPotCounterEditorScale(float scale);
void ResetPotCounterEditorPosition();
void CommitPotCounterEditorPosition();
bool GetEffectHudEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetEffectHudEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MoveEffectHudEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetEffectHudEditorScale();
void SetEffectHudEditorScale(float scale);
void ResetEffectHudEditorPosition();
void CommitEffectHudEditorPosition();
bool GetKeyStrokeEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetKeyStrokeEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MoveKeyStrokeEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetKeyStrokeEditorScale();
void SetKeyStrokeEditorScale(float scale);
void ResetKeyStrokeEditorPosition();
void CommitKeyStrokeEditorPosition();
bool GetControllerOverlayEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height);
void SetControllerOverlayEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight);
void MoveControllerOverlayEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight);
float GetControllerOverlayEditorScale();
void SetControllerOverlayEditorScale(float scale);
void ResetControllerOverlayEditorPosition();
void CommitControllerOverlayEditorPosition();
void RenderFpsOverlay();
void RenderCpsOverlay();
void RenderPingOverlay();
void RenderArrowCounterOverlay();
void RenderArrowCounterPreview(ImDrawList* drawList, const ImVec2& min);
void RenderPotCounterOverlay();
void RenderPotCounterPreview(ImDrawList* drawList, const ImVec2& min);
void RenderEffectHudOverlay();
void RenderEffectHudPreview(ImDrawList* drawList, const ImVec2& min);
void RenderKeyStrokeOverlay();
void RenderKeyStrokePreview(ImDrawList* drawList, const ImVec2& min, float scale);
void RenderControllerOverlay();
void RenderControllerOverlayPreview(ImDrawList* drawList, const ImVec2& min, float scale);
void RenderTabOverlay();
void RenderItemHudTextOverlay();
const char* GetKeyboardToggleComboLabel();
const char* GetControllerToggleComboLabel();
bool IsKeyboardToggleCaptureActive();
bool IsControllerToggleCaptureActive();
void BeginKeyboardToggleCapture();
void BeginControllerToggleCapture();
void CancelToggleComboCapture();
void ResetToggleComboConfig();
}

namespace tane::imgui_menu {
void DrawPanel(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 fillColor, float rounding, ImU32 borderColor);
void DrawModuleStatusStrip(ImDrawList* drawList, const ImVec2& rowMin, const ImVec2& rowMax, bool enabled);
bool DrawToggleButton(const char* id, bool enabled, const ImVec2& size);
bool DrawGearButton(const char* id, const ImVec2& size);

namespace {

enum class MenuTab {
    All,
    Gui,
    Render,
    Camera,
    Patch,
};

enum class SettingsPanel {
    None,
    ItemHudSettings,
    FpsSettings,
    CpsSettings,
    PingSettings,
    ArrowCounterSettings,
    PotCounterSettings,
    ControllerOverlaySettings,
    TabSettings,
    EffectHudSettings,
    HitboxSettings,
    TracerSettings,
    ZoomSettings,
    FreeLookSettings,
};

enum class RootView {
    Home,
    Modules,
    Settings,
};

enum class PositionEditorTarget {
    ItemHud,
    ItemHudDurability,
    Fps,
    Cps,
    Ping,
    ArrowCounter,
    PotCounter,
    EffectHud,
    KeyStroke,
    ControllerOverlay,
};

struct MenuTabInfo {
    const char* label;
    MenuTab tab;
};

constexpr MenuTabInfo kMenuTabs[] = {
    {"ALL", MenuTab::All},
    {"GUI", MenuTab::Gui},
    {"Render", MenuTab::Render},
    {"Camera", MenuTab::Camera},
    {"Patch", MenuTab::Patch},
};
constexpr std::size_t kMenuTabCount = sizeof(kMenuTabs) / sizeof(kMenuTabs[0]);
constexpr float kModuleRowRevealDelay = 0.115f;
constexpr float kModuleRowRevealDuration = 0.52f;

RootView g_rootView = RootView::Home;
RootView g_appSettingsOrigin = RootView::Home;
RootView g_appSettingsExitTarget = RootView::Home;
MenuTab g_selectedMenuTab = MenuTab::All;
SettingsPanel g_openSettingsPanel = SettingsPanel::None;
SettingsPanel g_visibleSettingsPanel = SettingsPanel::None;
float g_settingsPanelProgress = 0.0f;
float g_settingsPanelContentProgress = 1.0f;
float g_menuOpenProgress = 0.0f;
float g_moduleMenuProgress = 0.0f;
float g_appSettingsProgress = 0.0f;
float g_tabSwitchProgress = 1.0f;
float g_tabSwitchDirection = 1.0f;
char g_moduleSearch[64] = {};
bool g_moduleListAnimating = false;
int g_moduleListRenderIndex = 0;
int g_moduleListAnimationRowCount = 1;
bool g_moduleListScrollInitialized = false;
float g_moduleListTargetScrollY = 0.0f;
float g_moduleListVisualScrollY = 0.0f;
float g_moduleListAppliedScrollY = 0.0f;
float g_moduleListScrollMotion = 0.0f;
bool g_appSettingsClosing = false;
bool g_positionEditorActive = false;
bool g_positionEditorDraggingLastFrame = false;
RootView g_positionEditorReturnView = RootView::Home;
PositionEditorTarget g_positionEditorTarget = PositionEditorTarget::ItemHud;
float g_positionEditorProgress = 0.0f;
struct PositionEditorSnapState {
    bool vertical = false;
    bool horizontal = false;
    float x = 0.0f;
    float y = 0.0f;
};
PositionEditorSnapState g_positionEditorSnapState;
char g_tracerHexInput[10] = {};
bool g_tracerHexInputEditing = false;
char g_hitboxHexInput[10] = {};
bool g_hitboxHexInputEditing = false;

struct MenuLayout {
    ImVec2 mainPos;
    ImVec2 mainSize;
    ImVec2 settingsPos;
    ImVec2 settingsSize;
};

struct NavigationLayout {
    ImVec2 logoCenter;
    ImVec2 lineStart;
    ImVec2 lineEnd;
    ImVec2 menuButtonPos;
    ImVec2 modButtonPos;
    ImVec2 gearButtonPos;
    ImVec2 iconButtonSize;
    ImVec2 modButtonSize;
    float logoHeight;
    float labelAlpha;
};

#include "MenuNavigation.cpp"

#include "MenuSettings.cpp"

#include "MenuWindows.cpp"

void ResetMenuToHome() {
    ResetToHome();
}

bool IsPositionEditorVisible() {
    return g_positionEditorActive || g_positionEditorProgress > 0.0f;
}

bool IsMenuVisible() {
    return g_menuOpenProgress > 0.0f;
}

bool RenderMenu(const ImVec2& displaySize, bool shouldOpen) {
    if (!UpdateMenuOpenAnimation(shouldOpen)) {
        return false;
    }

    if (g_positionEditorActive || g_positionEditorProgress > 0.0f) {
        return RenderPositionEditor(displaySize, g_positionEditorActive);
    }

    UpdateModuleMenuAnimation();
    UpdateAppSettingsAnimation();
    UpdateTabSwitchAnimation();
    UpdateSettingsPanelAnimation();
    const MenuLayout layout = CalculateMenuLayout(displaySize, g_settingsPanelProgress);
    const float menuAppearEased = SmoothStep(g_menuOpenProgress);
    const float closingOffsetY = shouldOpen ? 0.0f : (1.0f - menuAppearEased) * 18.0f;

    DrawMenuBackdrop(displaySize, menuAppearEased);
    if (g_rootView == RootView::Home) {
        RenderNavigationWindow(displaySize, layout, 0.0f, shouldOpen, g_menuOpenProgress);
        return true;
    }

    if (g_rootView == RootView::Settings) {
        const bool showModulesBehindSettings =
            g_appSettingsOrigin == RootView::Modules ||
            (g_appSettingsClosing && g_appSettingsExitTarget == RootView::Modules);
        if (showModulesBehindSettings) {
            const float modulesTransitionAlpha = (g_appSettingsClosing && g_appSettingsExitTarget == RootView::Modules)
                ? (1.0f - SmoothStep(g_appSettingsProgress)) * menuAppearEased
                : (1.0f - SmoothStep(std::clamp(g_appSettingsProgress / 0.72f, 0.0f, 1.0f))) * menuAppearEased;
            if (modulesTransitionAlpha > 0.0f) {
                MenuLayout exitingLayout = layout;
                const float exitShift = (g_appSettingsClosing && g_appSettingsExitTarget == RootView::Modules)
                    ? SmoothStep(g_appSettingsProgress) * 18.0f
                    : SmoothStep(g_appSettingsProgress) * 28.0f;
                exitingLayout.mainPos.y += exitShift;
                exitingLayout.settingsPos.y += exitShift;

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, modulesTransitionAlpha);
                RenderTopTabs(displaySize, closingOffsetY + exitShift);
                RenderMainWindow(exitingLayout);
                RenderSettingsWindow(exitingLayout, menuAppearEased, !shouldOpen);
                ImGui::PopStyleVar();
            }
        }
        RenderSettingsNavigationWindow(displaySize, layout, g_menuOpenProgress, shouldOpen);
        RenderAppSettingsWindow(displaySize, g_menuOpenProgress, shouldOpen);
        return true;
    }

    const float easedProgress = SmoothStep(g_moduleMenuProgress);
    const float menuAlpha = SmoothStep(std::clamp((g_moduleMenuProgress - 0.12f) / 0.88f, 0.0f, 1.0f)) * menuAppearEased;
    if (menuAlpha > 0.0f) {
        MenuLayout animatedLayout = layout;
        animatedLayout.mainPos.x += (1.0f - easedProgress) * 42.0f;
        animatedLayout.settingsPos.x += (1.0f - easedProgress) * 42.0f;
        animatedLayout.mainPos.y += closingOffsetY;
        animatedLayout.settingsPos.y += closingOffsetY;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menuAlpha);
        RenderTopTabs(displaySize, closingOffsetY);
        RenderMainWindow(animatedLayout);
        RenderSettingsWindow(animatedLayout, menuAppearEased, !shouldOpen);
        ImGui::PopStyleVar();
    }
    RenderNavigationWindow(displaySize, layout, g_moduleMenuProgress, false, g_menuOpenProgress);
    RenderNavigationRailHitboxes(displaySize, layout, g_moduleMenuProgress, g_menuOpenProgress, shouldOpen);
    return true;
}

}  // namespace tane::imgui_menu

namespace tane::gui {

bool IsGuiPositionEditorActive() {
    return tane::imgui_menu::IsPositionEditorVisible();
}

bool IsGuiMenuVisible() {
    return tane::imgui_menu::IsMenuVisible();
}

void RenderOverlay() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    UpdateControllerMenuToggle();

    ImGuiIO& io = ImGui::GetIO();
    const bool menuOpen = IsMenuOpen();
    static bool menuWasVisible = false;
    if (!menuOpen && !menuWasVisible) {
        RenderFpsOverlay();
        RenderCpsOverlay();
        RenderPingOverlay();
        RenderKeyStrokeOverlay();
        RenderControllerOverlay();
        RenderTabOverlay();
        RenderArrowCounterOverlay();
        RenderPotCounterOverlay();
        RenderEffectHudOverlay();
        RenderItemHudTextOverlay();
    }

    if (menuOpen && !menuWasVisible) {
        tane::imgui_menu::ResetMenuToHome();
        menuWasVisible = true;
    }
    if (!menuOpen && !menuWasVisible) {
        io.MouseDrawCursor = false;
        return;
    }

    io.MouseDrawCursor = menuOpen;
    if (menuOpen) {
        io.WantCaptureMouse = true;
        io.WantCaptureKeyboard = true;
    }

    const bool stillVisible = tane::imgui_menu::RenderMenu(io.DisplaySize, menuOpen);
    if (!menuOpen && !stillVisible) {
        tane::gui::CancelToggleComboCapture();
        menuWasVisible = false;
    }
}

}  // namespace tane::gui
