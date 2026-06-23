void RenderNavigationWindow(const ImVec2& displaySize, const MenuLayout& menuLayout, float progress, bool allowInputs, float appearProgress) {
    const float appearEased = SmoothStep(appearProgress);
    const float appearAlpha = SmoothStep(std::clamp((appearProgress - 0.08f) / 0.92f, 0.0f, 1.0f));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;
    if (!allowInputs || appearProgress < 0.94f) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(displaySize, ImGuiCond_Always);
    const float previousAlpha = ImGui::GetStyle().Alpha;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, previousAlpha * appearAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!ImGui::Begin("##TaneClientHome", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    const float eased = SmoothStep(progress);
    const NavigationLayout homeLayout = CalculateHomeNavigationLayout(displaySize);
    const NavigationLayout railLayout = CalculateRailNavigationLayout(displaySize, menuLayout);
    NavigationLayout layout = LerpNavigationLayout(homeLayout, railLayout, eased);
    NavigationLayout appearingHomeLayout = homeLayout;
    NavigationLayout appearingRailLayout = railLayout;
    const ImVec2 entryOffset(0.0f, (1.0f - appearEased) * 26.0f);
    OffsetNavigationLayout(layout, entryOffset);
    OffsetNavigationLayout(appearingHomeLayout, entryOffset);
    OffsetNavigationLayout(appearingRailLayout, entryOffset);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (eased < 0.995f) {
        AddAppearingLine(
            drawList,
            appearingHomeLayout.lineStart,
            appearingHomeLayout.lineEnd,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 232), 1.0f - eased),
            2.0f,
            appearEased);
    }
    if (eased > 0.005f) {
        AddAppearingLine(
            drawList,
            appearingRailLayout.lineStart,
            appearingRailLayout.lineEnd,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 216), eased),
            1.8f,
            appearEased);
    }

    ImTextureRef logoTexture = tane::payload::GetMenuLogoTexture();
    const ImVec2 logoTextureSize = tane::payload::GetMenuLogoTextureSize();
    float logoBottomY = layout.logoCenter.y;
    if (HasTexture(logoTexture) && logoTextureSize.x > 0.0f && logoTextureSize.y > 0.0f) {
        const float logoHeight = layout.logoHeight;
        const float minLogoWidth = LerpFloat(86.0f, 38.0f, eased);
        const float maxLogoWidth = LerpFloat(210.0f, 58.0f, eased);
        const float logoWidth = std::clamp(logoHeight * (logoTextureSize.x / logoTextureSize.y), minLogoWidth, maxLogoWidth);
        const ImVec2 logoMin(std::floor(layout.logoCenter.x - logoWidth * 0.5f), std::floor(layout.logoCenter.y - logoHeight * 0.5f));
        const ImVec2 logoMax(logoMin.x + logoWidth, logoMin.y + logoHeight);
        drawList->AddImage(logoTexture, logoMin, logoMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ApplyStyleAlpha(IM_COL32(255, 255, 255, 255)));
        logoBottomY = logoMax.y;
    } else {
        const char* fallback = "TaneClient";
        const ImVec2 textSize = ImGui::CalcTextSize(fallback);
        drawList->AddText(
            ImVec2(std::floor(layout.logoCenter.x - textSize.x * 0.5f), std::floor(layout.logoCenter.y - textSize.y)),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 238), std::max(0.35f, layout.labelAlpha)),
            fallback);
        logoBottomY = layout.logoCenter.y;
    }

    const char* logoLabel = "TaneClient";
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    const float boldFontSize = boldFont != nullptr ? boldFont->LegacySize : ImGui::GetFontSize();
    const float logoLabelFontSize = LerpFloat(boldFontSize, 13.0f, eased);
    const ImVec2 logoLabelSize = CalcFontTextSize(boldFont, logoLabelFontSize, logoLabel);
    if (layout.labelAlpha > 0.01f) {
        drawList->AddText(
            boldFont,
            logoLabelFontSize,
            ImVec2(std::floor(layout.logoCenter.x - logoLabelSize.x * 0.5f), std::floor(logoBottomY + 8.0f)),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 238), layout.labelAlpha),
            logoLabel);
    }

    if (DrawHomeIconButton("##homeSettings", layout.gearButtonPos, layout.iconButtonSize, true) && allowInputs && g_rootView == RootView::Home) {
        OpenAppSettings();
    }
    const bool modButtonEnabled = allowInputs && g_rootView == RootView::Home;
    if (DrawHomeModButton(layout.modButtonPos, layout.modButtonSize, modButtonEnabled)) {
        OpenModuleMenu();
    }
    if (DrawHomeIconButton("##homeMenu", layout.menuButtonPos, layout.iconButtonSize, false) && allowInputs && g_rootView == RootView::Home) {
        OpenPositionEditor();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void RenderNavigationRailHitboxes(const ImVec2& displaySize, const MenuLayout& menuLayout, float progress, float appearProgress, bool allowInputs) {
    if (!allowInputs || appearProgress < 0.94f || progress < 0.86f || g_rootView != RootView::Modules) {
        return;
    }

    const float eased = SmoothStep(progress);
    const float appearEased = SmoothStep(appearProgress);
    const NavigationLayout homeLayout = CalculateHomeNavigationLayout(displaySize);
    const NavigationLayout railLayout = CalculateRailNavigationLayout(displaySize, menuLayout);
    NavigationLayout layout = LerpNavigationLayout(homeLayout, railLayout, eased);
    OffsetNavigationLayout(layout, ImVec2(0.0f, (1.0f - appearEased) * 26.0f));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;

    const ImVec2 hitboxMin(
        std::min(layout.gearButtonPos.x, layout.menuButtonPos.x),
        std::min(layout.gearButtonPos.y, layout.menuButtonPos.y));
    const ImVec2 hitboxMax(
        std::max(layout.gearButtonPos.x + layout.iconButtonSize.x, layout.menuButtonPos.x + layout.iconButtonSize.x),
        std::max(layout.gearButtonPos.y + layout.iconButtonSize.y, layout.menuButtonPos.y + layout.iconButtonSize.y));

    ImGui::SetNextWindowPos(hitboxMin, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(hitboxMax.x - hitboxMin.x, hitboxMax.y - hitboxMin.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("##TaneClientRailNavigationHitboxes", nullptr, flags)) {
        if (DrawHomeIconButton("##openAppSettingsFromRail", layout.gearButtonPos, layout.iconButtonSize, true)) {
            OpenAppSettings();
        }
        if (DrawHomeIconButton("##openPositionEditorFromRail", layout.menuButtonPos, layout.iconButtonSize, false)) {
            OpenPositionEditor();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void RenderSettingsNavigationWindow(const ImVec2& displaySize, const MenuLayout& menuLayout, float appearProgress, bool allowInputs) {
    const float settingsProgress = SmoothStep(g_appSettingsProgress);
    const float menuFade = SmoothStep(appearProgress);
    const float alpha = (g_appSettingsClosing ? std::clamp(g_appSettingsProgress, 0.0f, 1.0f) : 1.0f) * menuFade;
    if (alpha <= 0.0f) {
        return;
    }

    const NavigationLayout targetLayout = CalculateSettingsNavigationLayout(displaySize);
    const NavigationLayout sourceLayout = g_appSettingsOrigin == RootView::Modules
        ? CalculateRailNavigationLayout(displaySize, menuLayout)
        : CalculateHomeNavigationLayout(displaySize);
    NavigationLayout layout = LerpNavigationLayout(sourceLayout, targetLayout, settingsProgress);
    OffsetNavigationLayout(layout, ImVec2(0.0f, (1.0f - SmoothStep(appearProgress)) * 18.0f));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;
    if (!allowInputs || g_appSettingsProgress < 0.94f) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(displaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!ImGui::Begin("##TaneClientSettingsNavigation", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddLine(
        layout.lineStart,
        layout.lineEnd,
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 218)),
        1.8f);

    ImTextureRef logoTexture = tane::payload::GetMenuLogoTexture();
    const ImVec2 logoTextureSize = tane::payload::GetMenuLogoTextureSize();
    float logoBottomY = layout.logoCenter.y;
    if (HasTexture(logoTexture) && logoTextureSize.x > 0.0f && logoTextureSize.y > 0.0f && layout.logoHeight > 1.0f) {
        const float logoWidth = std::clamp(layout.logoHeight * (logoTextureSize.x / logoTextureSize.y), 34.0f, 72.0f);
        const ImVec2 logoMin(std::floor(layout.logoCenter.x - logoWidth * 0.5f), std::floor(layout.logoCenter.y - layout.logoHeight * 0.5f));
        const ImVec2 logoMax(logoMin.x + logoWidth, logoMin.y + layout.logoHeight);
        drawList->AddImage(logoTexture, logoMin, logoMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ApplyStyleAlpha(IM_COL32(255, 255, 255, 255)));
        logoBottomY = logoMax.y;
    }

    const char* logoLabel = "TaneClient";
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    const float labelFontSize = boldFont != nullptr ? 13.0f : ImGui::GetFontSize();
    const ImVec2 labelSize = CalcFontTextSize(boldFont, labelFontSize, logoLabel);
    drawList->AddText(
        boldFont,
        labelFontSize,
        ImVec2(std::floor(layout.logoCenter.x - labelSize.x * 0.5f), std::floor(logoBottomY + 4.0f)),
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 232), layout.labelAlpha),
        logoLabel);

    if (DrawHomeIconButton("##settingsNavMenu", layout.menuButtonPos, layout.iconButtonSize, false) && allowInputs && g_appSettingsProgress >= 0.94f) {
        OpenPositionEditor();
    }
    if (DrawHomeModButton(layout.modButtonPos, layout.modButtonSize, allowInputs && g_appSettingsProgress >= 0.94f)) {
        CloseAppSettingsToModules();
    }
    DrawHomeIconButton("##settingsNavGear", layout.gearButtonPos, layout.iconButtonSize, true);

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void CloseSettingsPanel() {
    g_openSettingsPanel = SettingsPanel::None;
}

void OpenModuleMenu() {
    g_rootView = RootView::Modules;
    g_appSettingsProgress = 0.0f;
    g_appSettingsClosing = false;
    ResetModuleListScrollAnimation();
    tane::gui::CancelToggleComboCapture();
}

void OpenAppSettings() {
    if (g_rootView == RootView::Modules) {
        g_moduleMenuProgress = 1.0f;
    }
    g_appSettingsOrigin = g_rootView;
    g_appSettingsExitTarget = RootView::Home;
    g_appSettingsClosing = false;
    g_rootView = RootView::Settings;
    g_appSettingsProgress = 0.0f;
    CloseSettingsPanel();
}

void CloseAppSettings() {
    g_appSettingsExitTarget = RootView::Home;
    g_appSettingsClosing = true;
    g_appSettingsOrigin = RootView::Home;
    CloseSettingsPanel();
    tane::gui::CancelToggleComboCapture();
}

void CloseAppSettingsToModules() {
    g_appSettingsExitTarget = RootView::Modules;
    g_appSettingsClosing = true;
    g_appSettingsOrigin = RootView::Modules;
    g_moduleMenuProgress = 0.0f;
    tane::gui::CancelToggleComboCapture();
}

void OpenPositionEditor() {
    g_positionEditorReturnView = g_rootView;
    g_positionEditorActive = true;
    g_positionEditorProgress = 0.0f;
    g_positionEditorDraggingLastFrame = false;
    g_positionEditorTarget = PositionEditorTarget::ItemHud;
    tane::gui::CancelToggleComboCapture();
    tane::gui::ResetPositionEditorControllerInputState();
}

void ClosePositionEditor() {
    if (g_positionEditorActive) {
        tane::gui::CommitItemHudEditorPosition();
        tane::gui::CommitFpsEditorPosition();
        tane::gui::CommitCpsEditorPosition();
        tane::gui::CommitPingEditorPosition();
        tane::gui::CommitArrowCounterEditorPosition();
        tane::gui::CommitPotCounterEditorPosition();
        tane::gui::CommitEffectHudEditorPosition();
        tane::gui::CommitKeyStrokeEditorPosition();
        tane::gui::CommitControllerOverlayEditorPosition();
    }
    g_positionEditorActive = false;
    g_positionEditorDraggingLastFrame = false;
    tane::gui::ResetPositionEditorControllerInputState();
    if (g_positionEditorReturnView == RootView::Modules) {
        g_moduleMenuProgress = 1.0f;
    }
}

void UpdateModuleMenuAnimation() {
    const bool shouldOpen = g_rootView == RootView::Modules ||
        (g_rootView == RootView::Settings &&
            (g_appSettingsOrigin == RootView::Modules || (g_appSettingsClosing && g_appSettingsExitTarget == RootView::Modules)));
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    constexpr float kOpenDurationSeconds = 0.58f;

    if (shouldOpen) {
        g_moduleMenuProgress = std::min(1.0f, g_moduleMenuProgress + deltaTime / kOpenDurationSeconds);
    } else {
        g_moduleMenuProgress = 0.0f;
    }
}

void UpdateAppSettingsAnimation() {
    const bool shouldOpen = g_rootView == RootView::Settings;
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    constexpr float kOpenDurationSeconds = 0.34f;
    constexpr float kCloseDurationSeconds = 0.28f;
    if (shouldOpen && !g_appSettingsClosing) {
        g_appSettingsProgress = std::min(1.0f, g_appSettingsProgress + deltaTime / kOpenDurationSeconds);
    } else if (shouldOpen && g_appSettingsClosing) {
        g_appSettingsProgress = std::max(0.0f, g_appSettingsProgress - deltaTime / kCloseDurationSeconds);
        if (g_appSettingsProgress <= 0.0f) {
            const RootView target = g_appSettingsExitTarget;
            g_appSettingsClosing = false;
            g_appSettingsExitTarget = RootView::Home;
            g_rootView = target;
            if (target == RootView::Modules) {
                g_moduleMenuProgress = 1.0f;
                g_tabSwitchProgress = 1.0f;
            }
        }
    } else {
        g_appSettingsProgress = 0.0f;
    }
}

bool UpdateMenuOpenAnimation(bool shouldOpen) {
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    constexpr float kOpenDurationSeconds = 0.42f;
    constexpr float kCloseDurationSeconds = 0.30f;

    if (shouldOpen) {
        g_menuOpenProgress = std::min(1.0f, g_menuOpenProgress + deltaTime / kOpenDurationSeconds);
    } else {
        g_menuOpenProgress = std::max(0.0f, g_menuOpenProgress - deltaTime / kCloseDurationSeconds);
    }

    return shouldOpen || g_menuOpenProgress > 0.0f;
}

void UpdateTabSwitchAnimation() {
    const float animationEnd = GetModuleListAnimationEnd();
    if (g_tabSwitchProgress >= animationEnd) {
        return;
    }

    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    constexpr float kSwitchDurationSeconds = 0.46f;
    g_tabSwitchProgress = std::min(animationEnd, g_tabSwitchProgress + deltaTime / kSwitchDurationSeconds);
}

void UpdateSettingsPanelAnimation() {
    const bool shouldOpen = g_openSettingsPanel != SettingsPanel::None;
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    constexpr float kOpenDurationSeconds = 0.46f;
    constexpr float kCloseDurationSeconds = 0.36f;

    if (shouldOpen) {
        g_visibleSettingsPanel = g_openSettingsPanel;
        g_settingsPanelProgress = std::min(1.0f, g_settingsPanelProgress + deltaTime / kOpenDurationSeconds);
        g_settingsPanelContentProgress = std::min(1.0f, g_settingsPanelContentProgress + deltaTime / 0.22f);
        return;
    }

    if (g_visibleSettingsPanel != SettingsPanel::None || g_settingsPanelProgress > 0.0f) {
        g_settingsPanelProgress = std::max(0.0f, g_settingsPanelProgress - deltaTime / kCloseDurationSeconds);
        if (g_settingsPanelProgress <= 0.0f) {
            g_visibleSettingsPanel = SettingsPanel::None;
        }
    }
    g_settingsPanelContentProgress = 1.0f;
}

MenuLayout CalculateMenuLayout(const ImVec2& displaySize, float settingsPanelProgress) {
    constexpr float kOuterMargin = 16.0f;
    constexpr float kPanelGap = 16.0f;
    constexpr float kMinMainWidth = 300.0f;
    constexpr float kMinSettingsWidth = 260.0f;
    constexpr float kMaxSettingsWidth = 360.0f;
    constexpr float kPreferredWindowTop = 82.0f;
    constexpr float kBottomMargin = 34.0f;
    constexpr float kMinWindowHeight = 280.0f;

    const float availableWidth = std::max(320.0f, displaySize.x - kOuterMargin * 2.0f);
    const float maxWindowTop = std::max(kOuterMargin, displaySize.y - kMinWindowHeight - kBottomMargin);
    const float windowY = std::max(kOuterMargin, std::min(kPreferredWindowTop, maxWindowTop));
    const float windowHeight = std::max(kMinWindowHeight, displaySize.y - windowY - kBottomMargin);

    MenuLayout layout{};
    layout.settingsSize = ImVec2(
        std::clamp(displaySize.x * 0.24f, kMinSettingsWidth, kMaxSettingsWidth),
        windowHeight);

    const float mainWidth = std::clamp(displaySize.x * 0.72f, 430.0f, 860.0f);
    layout.mainSize = ImVec2(std::min(mainWidth, displaySize.x - kOuterMargin * 2.0f), windowHeight);
    layout.mainPos = ImVec2(
        std::max(kOuterMargin, (displaySize.x - layout.mainSize.x) * 0.5f),
        windowY);
    layout.settingsPos = ImVec2(layout.mainPos.x + layout.mainSize.x + kPanelGap, windowY);
    layout.settingsSize.x = std::min(layout.settingsSize.x, std::max(kMinSettingsWidth, displaySize.x - layout.settingsPos.x - kOuterMargin));

    MenuLayout expandedLayout = layout;
    if (availableWidth >= kMinMainWidth + kPanelGap + kMinSettingsWidth) {
        if (expandedLayout.settingsSize.x + kPanelGap + kMinMainWidth > availableWidth) {
            expandedLayout.settingsSize.x = std::max(kMinSettingsWidth, availableWidth - kPanelGap - kMinMainWidth);
        }

        const float mainMax = std::min(720.0f, availableWidth - expandedLayout.settingsSize.x - kPanelGap);
        expandedLayout.mainSize = ImVec2(std::clamp(displaySize.x * 0.50f, kMinMainWidth, mainMax), windowHeight);
        const float totalWidth = expandedLayout.mainSize.x + kPanelGap + expandedLayout.settingsSize.x;
        const float startX = std::max(kOuterMargin, (displaySize.x - totalWidth) * 0.5f);
        expandedLayout.mainPos = ImVec2(startX, windowY);
        expandedLayout.settingsPos = ImVec2(expandedLayout.mainPos.x + expandedLayout.mainSize.x + kPanelGap, windowY);
    }

    const float easedProgress = SmoothStep(settingsPanelProgress);
    MenuLayout blendedLayout{};
    blendedLayout.mainPos = LerpVec2(layout.mainPos, expandedLayout.mainPos, easedProgress);
    blendedLayout.mainSize = LerpVec2(layout.mainSize, expandedLayout.mainSize, easedProgress);
    blendedLayout.settingsPos = LerpVec2(layout.settingsPos, expandedLayout.settingsPos, easedProgress);
    blendedLayout.settingsSize = LerpVec2(layout.settingsSize, expandedLayout.settingsSize, easedProgress);
    return blendedLayout;
}

bool DrawTabButton(const char* label, bool selected, const ImVec2& size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    const ImU32 fill = selected
        ? (hovered ? IM_COL32(255, 255, 255, 58) : IM_COL32(255, 255, 255, 44))
        : (hovered ? IM_COL32(255, 255, 255, 30) : IM_COL32(0, 0, 0, 72));
    drawList->AddRectFilled(pos, max, ApplyStyleAlpha(fill), 13.0f);
    drawList->AddRect(
        ImVec2(pos.x + 0.75f, pos.y + 0.75f),
        ImVec2(max.x - 0.75f, max.y - 0.75f),
        ApplyStyleAlpha(selected ? IM_COL32(255, 255, 255, 120) : IM_COL32(255, 255, 255, 54)),
        12.25f,
        0,
        1.1f);

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImU32 textColor = selected ? IM_COL32(255, 255, 255, 244) : IM_COL32(190, 190, 190, 235);
    drawList->AddText(
        ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
        ApplyStyleAlpha(textColor),
        label);
    return clicked;
}
