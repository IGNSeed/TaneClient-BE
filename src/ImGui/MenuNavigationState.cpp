const char* GetSelectedTabLabel() {
    for (const MenuTabInfo& tab : kMenuTabs) {
        if (tab.tab == g_selectedMenuTab) {
            return tab.label;
        }
    }

    return "ALL";
}

int GetMenuTabIndex(MenuTab selectedTab) {
    for (std::size_t index = 0; index < kMenuTabCount; ++index) {
        if (kMenuTabs[index].tab == selectedTab) {
            return static_cast<int>(index);
        }
    }

    return 0;
}

void DrawMenuBackdrop(const ImVec2& displaySize, float alpha) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        displaySize,
        IM_COL32(0, 0, 0, static_cast<int>(150.0f * std::clamp(alpha, 0.0f, 1.0f))));
}

float SmoothStep(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

ImU32 ApplyStyleAlpha(ImU32 color) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return color;
    }

    ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
    value.w *= ImGui::GetStyle().Alpha;
    return ImGui::ColorConvertFloat4ToU32(value);
}

ImU32 ApplyStyleAlpha(ImU32 color, float alphaMultiplier) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return color;
    }

    ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
    value.w *= ImGui::GetStyle().Alpha * std::clamp(alphaMultiplier, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(value);
}

float LerpFloat(float from, float to, float progress) {
    return from + (to - from) * progress;
}

ImVec2 LerpVec2(const ImVec2& from, const ImVec2& to, float progress) {
    return ImVec2(LerpFloat(from.x, to.x, progress), LerpFloat(from.y, to.y, progress));
}

float GetModuleListAnimationEnd() {
    const int rowCount = std::max(1, g_moduleListAnimationRowCount);
    return kModuleRowRevealDuration + static_cast<float>(rowCount - 1) * kModuleRowRevealDelay;
}

void ResetModuleListScrollAnimation() {
    g_moduleListScrollInitialized = false;
    g_moduleListTargetScrollY = 0.0f;
    g_moduleListVisualScrollY = 0.0f;
    g_moduleListAppliedScrollY = 0.0f;
    g_moduleListScrollMotion = 0.0f;
}

bool HasTexture(ImTextureRef texture) {
    return texture._TexData != nullptr || texture._TexID != ImTextureID_Invalid;
}

void OpenModuleMenu();
void CloseSettingsPanel();
void OpenAppSettings();
void CloseAppSettings();
void CloseAppSettingsToModules();
void OpenPositionEditor();
void ClosePositionEditor();

NavigationLayout CalculateHomeNavigationLayout(const ImVec2& displaySize) {
    constexpr float kIconSize = 54.0f;
    constexpr float kModWidth = 124.0f;
    constexpr float kButtonHeight = 54.0f;
    constexpr float kGap = 12.0f;
    constexpr float kLineWidth = 244.0f;

    NavigationLayout layout{};
    layout.iconButtonSize = ImVec2(kIconSize, kButtonHeight);
    layout.modButtonSize = ImVec2(kModWidth, kButtonHeight);
    layout.modButtonPos = ImVec2(
        std::floor(displaySize.x * 0.5f - kModWidth * 0.5f),
        std::floor(displaySize.y * 0.5f - kButtonHeight * 0.5f));
    layout.menuButtonPos = ImVec2(layout.modButtonPos.x - kGap - kIconSize, layout.modButtonPos.y);
    layout.gearButtonPos = ImVec2(layout.modButtonPos.x + kModWidth + kGap, layout.modButtonPos.y);

    const float lineY = layout.modButtonPos.y - 28.0f;
    layout.lineStart = ImVec2(std::floor((displaySize.x - kLineWidth) * 0.5f), lineY);
    layout.lineEnd = ImVec2(layout.lineStart.x + kLineWidth, lineY);
    layout.logoCenter = ImVec2(displaySize.x * 0.5f, lineY - 86.0f);
    layout.logoHeight = 76.0f;
    layout.labelAlpha = 1.0f;
    return layout;
}

NavigationLayout CalculateRailNavigationLayout(const ImVec2& displaySize, const MenuLayout& menuLayout) {
    constexpr float kIconSize = 54.0f;
    constexpr float kModWidth = 72.0f;
    constexpr float kButtonHeight = 54.0f;
    constexpr float kVerticalSpacing = 18.0f;
    constexpr float kSidePadding = 16.0f;

    NavigationLayout layout{};
    layout.iconButtonSize = ImVec2(kIconSize, kButtonHeight);
    layout.modButtonSize = ImVec2(kModWidth, kButtonHeight);

    const float leftSpace = std::max(0.0f, menuLayout.mainPos.x);
    const float minCenterX = kSidePadding + kModWidth * 0.5f;
    const float maxCenterX = std::max(minCenterX, menuLayout.mainPos.x - kSidePadding - kModWidth * 0.5f);
    const float preferredCenterX = leftSpace * 0.5f;
    const float railCenterX = std::floor(std::clamp(preferredCenterX, minCenterX, maxCenterX));
    const float modCenterY = menuLayout.mainPos.y + menuLayout.mainSize.y * 0.5f;

    layout.modButtonPos = ImVec2(std::floor(railCenterX - kModWidth * 0.5f), std::floor(modCenterY - kButtonHeight * 0.5f));
    layout.gearButtonPos = ImVec2(
        std::floor(railCenterX - kIconSize * 0.5f),
        std::floor(layout.modButtonPos.y - kVerticalSpacing - kIconSize));
    layout.menuButtonPos = ImVec2(
        std::floor(railCenterX - kIconSize * 0.5f),
        std::floor(layout.modButtonPos.y + kButtonHeight + kVerticalSpacing));

    const float lineRightX = railCenterX + kModWidth * 0.5f + 14.0f;
    const float lineLeftX = railCenterX - kModWidth * 0.5f - 14.0f;
    const float lineX = lineRightX <= menuLayout.mainPos.x - kSidePadding ? lineRightX : std::max(kSidePadding, lineLeftX);
    layout.lineStart = ImVec2(
        std::floor(lineX),
        std::floor(layout.gearButtonPos.y - 8.0f));
    layout.lineEnd = ImVec2(
        std::floor(lineX),
        std::floor(layout.menuButtonPos.y + kButtonHeight + 8.0f));

    layout.logoCenter = ImVec2(
        std::floor(railCenterX),
        std::floor(std::max(menuLayout.mainPos.y + 42.0f, layout.gearButtonPos.y - 52.0f)));
    layout.logoHeight = 38.0f;
    layout.labelAlpha = 1.0f;

    (void)displaySize;
    return layout;
}

ImVec2 CalculateAppSettingsWindowSize(const ImVec2& displaySize) {
    return ImVec2(
        std::clamp(displaySize.x * 0.58f, 560.0f, 760.0f),
        314.0f);
}

ImVec2 CalculateAppSettingsTargetPos(const ImVec2& displaySize, const ImVec2& windowSize) {
    return ImVec2(
        std::floor((displaySize.x - windowSize.x) * 0.5f),
        std::floor((displaySize.y - windowSize.y) * 0.5f));
}

NavigationLayout CalculateSettingsNavigationLayout(const ImVec2& displaySize) {
    constexpr float kIconSize = 46.0f;
    constexpr float kModWidth = 96.0f;
    constexpr float kButtonHeight = 46.0f;
    constexpr float kGap = 10.0f;
    constexpr float kLineWidth = 204.0f;

    const ImVec2 settingsSize = CalculateAppSettingsWindowSize(displaySize);
    const ImVec2 settingsPos = CalculateAppSettingsTargetPos(displaySize, settingsSize);
    const float centerX = settingsPos.x + settingsSize.x * 0.5f;
    const float buttonY = std::max(16.0f, settingsPos.y - 74.0f);

    NavigationLayout layout{};
    layout.iconButtonSize = ImVec2(kIconSize, kButtonHeight);
    layout.modButtonSize = ImVec2(kModWidth, kButtonHeight);
    layout.modButtonPos = ImVec2(std::floor(centerX - kModWidth * 0.5f), std::floor(buttonY));
    layout.menuButtonPos = ImVec2(layout.modButtonPos.x - kGap - kIconSize, layout.modButtonPos.y);
    layout.gearButtonPos = ImVec2(layout.modButtonPos.x + kModWidth + kGap, layout.modButtonPos.y);
    const float lineY = layout.modButtonPos.y - 16.0f;
    layout.lineStart = ImVec2(std::floor(centerX - kLineWidth * 0.5f), lineY);
    layout.lineEnd = ImVec2(std::floor(centerX + kLineWidth * 0.5f), lineY);
    layout.logoCenter = ImVec2(centerX, lineY - 44.0f);
    layout.logoHeight = 34.0f;
    layout.labelAlpha = 1.0f;
    return layout;
}

NavigationLayout LerpNavigationLayout(const NavigationLayout& from, const NavigationLayout& to, float progress) {
    NavigationLayout layout{};
    layout.logoCenter = LerpVec2(from.logoCenter, to.logoCenter, progress);
    layout.lineStart = LerpVec2(from.lineStart, to.lineStart, progress);
    layout.lineEnd = LerpVec2(from.lineEnd, to.lineEnd, progress);
    layout.menuButtonPos = LerpVec2(from.menuButtonPos, to.menuButtonPos, progress);
    layout.modButtonPos = LerpVec2(from.modButtonPos, to.modButtonPos, progress);
    layout.gearButtonPos = LerpVec2(from.gearButtonPos, to.gearButtonPos, progress);
    layout.iconButtonSize = LerpVec2(from.iconButtonSize, to.iconButtonSize, progress);
    layout.modButtonSize = LerpVec2(from.modButtonSize, to.modButtonSize, progress);
    layout.logoHeight = LerpFloat(from.logoHeight, to.logoHeight, progress);
    layout.labelAlpha = LerpFloat(from.labelAlpha, to.labelAlpha, progress);
    return layout;
}

void OffsetNavigationLayout(NavigationLayout& layout, const ImVec2& offset) {
    layout.logoCenter.x += offset.x;
    layout.logoCenter.y += offset.y;
    layout.lineStart.x += offset.x;
    layout.lineStart.y += offset.y;
    layout.lineEnd.x += offset.x;
    layout.lineEnd.y += offset.y;
    layout.menuButtonPos.x += offset.x;
    layout.menuButtonPos.y += offset.y;
    layout.modButtonPos.x += offset.x;
    layout.modButtonPos.y += offset.y;
    layout.gearButtonPos.x += offset.x;
    layout.gearButtonPos.y += offset.y;
}

void AddAppearingLine(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness, float reveal) {
    const ImVec2 center((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
    drawList->AddLine(
        LerpVec2(center, start, reveal),
        LerpVec2(center, end, reveal),
        color,
        thickness);
}

void ResetToHome() {
    g_rootView = RootView::Home;
    g_openSettingsPanel = SettingsPanel::None;
    g_visibleSettingsPanel = SettingsPanel::None;
    g_settingsPanelProgress = 0.0f;
    g_settingsPanelContentProgress = 1.0f;
    g_menuOpenProgress = 0.0f;
    g_moduleMenuProgress = 0.0f;
    g_appSettingsProgress = 0.0f;
    g_appSettingsExitTarget = RootView::Home;
    g_appSettingsClosing = false;
    g_tabSwitchProgress = 1.0f;
    g_tabSwitchDirection = 1.0f;
    g_moduleSearch[0] = '\0';
    ResetModuleListScrollAnimation();
    g_positionEditorActive = false;
    g_positionEditorProgress = 0.0f;
    g_positionEditorDraggingLastFrame = false;
    g_positionEditorReturnView = RootView::Home;
    g_positionEditorTarget = PositionEditorTarget::ItemHud;
}

void SelectMenuTab(MenuTab tab) {
    if (g_selectedMenuTab == tab) {
        return;
    }

    const int oldIndex = GetMenuTabIndex(g_selectedMenuTab);
    const int newIndex = GetMenuTabIndex(tab);
    g_tabSwitchDirection = newIndex >= oldIndex ? 1.0f : -1.0f;
    g_selectedMenuTab = tab;
    g_tabSwitchProgress = 0.0f;
    ResetModuleListScrollAnimation();
    CloseSettingsPanel();
}

void OpenSettingsPanel(SettingsPanel panel) {
    if (panel == SettingsPanel::None) {
        return;
    }

    if (g_visibleSettingsPanel != SettingsPanel::None && g_visibleSettingsPanel != panel) {
        g_settingsPanelContentProgress = 0.0f;
    }
    g_openSettingsPanel = panel;
    g_visibleSettingsPanel = panel;
}

bool DrawHomeIconButton(const char* id, const ImVec2& pos, const ImVec2& size, bool gearIcon) {
    ImGui::SetCursorScreenPos(pos);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    DrawPanel(
        drawList,
        pos,
        max,
        hovered ? IM_COL32(42, 42, 42, 218) : IM_COL32(10, 10, 10, 194),
        14.0f,
        hovered ? IM_COL32(255, 255, 255, 156) : IM_COL32(255, 255, 255, 98));

    const ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
    const ImU32 iconColor = ApplyStyleAlpha(IM_COL32(250, 250, 250, 238));
    if (gearIcon) {
        constexpr float kPi = 3.14159265358979323846f;
        const float outerRadius = 9.0f;
        for (int i = 0; i < 8; ++i) {
            const float angle = (static_cast<float>(i) / 8.0f) * kPi * 2.0f;
            const ImVec2 a(center.x + std::cos(angle) * (outerRadius + 1.0f), center.y + std::sin(angle) * (outerRadius + 1.0f));
            const ImVec2 b(center.x + std::cos(angle) * (outerRadius + 5.0f), center.y + std::sin(angle) * (outerRadius + 5.0f));
            drawList->AddLine(a, b, iconColor, 2.0f);
        }
        drawList->AddCircle(center, outerRadius, iconColor, 24, 2.0f);
        drawList->AddCircle(center, 3.8f, iconColor, 16, 1.8f);
    } else {
        constexpr float kCellSize = 5.2f;
        constexpr float kCellGap = 4.2f;
        const float gridSize = kCellSize * 3.0f + kCellGap * 2.0f;
        const ImVec2 gridMin(center.x - gridSize * 0.5f, center.y - gridSize * 0.5f);
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const ImVec2 cellMin(
                    gridMin.x + static_cast<float>(column) * (kCellSize + kCellGap),
                    gridMin.y + static_cast<float>(row) * (kCellSize + kCellGap));
                drawList->AddRectFilled(
                    cellMin,
                    ImVec2(cellMin.x + kCellSize, cellMin.y + kCellSize),
                    iconColor,
                    1.4f);
            }
        }
    }

    return clicked;
}

ImVec2 CalcFontTextSize(ImFont* font, float fontSize, const char* text) {
    if (font != nullptr) {
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    }

    return ImGui::CalcTextSize(text);
}

bool DrawHomeModButton(const ImVec2& pos, const ImVec2& size, bool enabled) {
    bool clicked = false;
    bool hovered = false;
    if (enabled) {
        ImGui::SetCursorScreenPos(pos);
        clicked = ImGui::InvisibleButton("##homeMod", size);
        hovered = ImGui::IsItemHovered();
    } else {
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(size);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawPanel(
        drawList,
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        hovered ? IM_COL32(42, 42, 42, 228) : (enabled ? IM_COL32(10, 10, 10, 204) : IM_COL32(8, 8, 8, 184)),
        14.0f,
        hovered ? IM_COL32(255, 255, 255, 156) : (enabled ? IM_COL32(255, 255, 255, 98) : IM_COL32(255, 255, 255, 62)));

    const char* label = "MOD";
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    const float fontSize = boldFont != nullptr ? boldFont->LegacySize : ImGui::GetFontSize();
    const ImVec2 textSize = CalcFontTextSize(boldFont, fontSize, label);
    drawList->AddText(
        boldFont,
        fontSize,
        ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
        ApplyStyleAlpha(enabled ? IM_COL32(250, 250, 250, 238) : IM_COL32(250, 250, 250, 172)),
        label);
    return clicked;
}
