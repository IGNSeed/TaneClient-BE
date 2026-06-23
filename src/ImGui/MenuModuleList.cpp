void RenderTopTabs(const ImVec2& displaySize, float yOffset) {
    constexpr float kTabWidth = 116.0f;
    constexpr float kTabHeight = 34.0f;
    constexpr float kTabSpacing = 10.0f;
    constexpr float kPanelPaddingX = 14.0f;
    constexpr float kPanelPaddingY = 10.0f;
    const float tabCount = static_cast<float>(kMenuTabCount);
    const float maxPanelWidth = std::max(320.0f, displaySize.x - 24.0f);
    const float availableTabWidth = maxPanelWidth - kPanelPaddingX * 2.0f - (tabCount - 1.0f) * kTabSpacing;
    const float tabWidth = std::clamp(availableTabWidth / tabCount, 74.0f, kTabWidth);
    const float totalWidth = tabCount * tabWidth + (tabCount - 1.0f) * kTabSpacing;
    const ImVec2 panelSize(totalWidth + kPanelPaddingX * 2.0f, kTabHeight + kPanelPaddingY * 2.0f);
    const ImVec2 panelPos(std::max(12.0f, (displaySize.x - panelSize.x) * 0.5f), 14.0f + yOffset);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!ImGui::Begin("##TaneClientTabs", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    DrawPanel(
        ImGui::GetWindowDrawList(),
        panelPos,
        ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y),
        IM_COL32(0, 0, 0, 166),
        18.0f,
        IM_COL32(255, 255, 255, 168));
    ImGui::SetCursorPos(ImVec2(kPanelPaddingX, kPanelPaddingY));

    for (std::size_t index = 0; index < kMenuTabCount; ++index) {
        const MenuTabInfo& tab = kMenuTabs[index];
        ImGui::PushID(tab.label);
        if (DrawTabButton(tab.label, tab.tab == g_selectedMenuTab, ImVec2(tabWidth, kTabHeight))) {
            SelectMenuTab(tab.tab);
        }
        ImGui::PopID();

        if (index + 1 < kMenuTabCount) {
            ImGui::SameLine(0.0f, kTabSpacing);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

char ToLowerAscii(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}

bool ContainsCaseInsensitive(const char* haystack, const char* needle) {
    if (haystack == nullptr || needle == nullptr || needle[0] == '\0') {
        return needle == nullptr || needle[0] == '\0';
    }

    for (const char* start = haystack; *start != '\0'; ++start) {
        const char* hay = start;
        const char* search = needle;
        while (*hay != '\0' && *search != '\0' && ToLowerAscii(*hay) == ToLowerAscii(*search)) {
            ++hay;
            ++search;
        }

        if (*search == '\0') {
            return true;
        }
    }

    return false;
}

bool MatchesModuleSearch(const char* title, const char* category) {
    if (g_moduleSearch[0] == '\0') {
        return true;
    }

    return ContainsCaseInsensitive(title, g_moduleSearch) || ContainsCaseInsensitive(category, g_moduleSearch);
}

float GetModuleRowRevealProgress(int animationIndex) {
    if (animationIndex < 0 || g_tabSwitchProgress >= GetModuleListAnimationEnd()) {
        return 1.0f;
    }

    const float linearProgress = std::clamp(
        (g_tabSwitchProgress - static_cast<float>(animationIndex) * kModuleRowRevealDelay) / kModuleRowRevealDuration,
        0.0f,
        1.0f);
    return SmoothStep(linearProgress);
}

void BeginModuleListScrollAnimationFrame(bool allowInputs) {
    const float maxScrollY = std::max(0.0f, ImGui::GetScrollMaxY());
    const float actualScrollY = std::clamp(ImGui::GetScrollY(), 0.0f, maxScrollY);

    if (!g_moduleListScrollInitialized) {
        g_moduleListTargetScrollY = actualScrollY;
        g_moduleListVisualScrollY = actualScrollY;
        g_moduleListAppliedScrollY = actualScrollY;
        g_moduleListScrollInitialized = true;
    } else if (std::fabs(actualScrollY - g_moduleListAppliedScrollY) > 1.0f &&
               std::fabs(actualScrollY - g_moduleListVisualScrollY) > 1.0f) {
        g_moduleListTargetScrollY = actualScrollY;
        g_moduleListVisualScrollY = actualScrollY;
    }

    if (allowInputs && ImGui::IsWindowHovered()) {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (std::fabs(wheel) > FLT_EPSILON) {
            constexpr float kWheelStep = 124.0f;
            g_moduleListTargetScrollY = std::clamp(g_moduleListTargetScrollY - wheel * kWheelStep, 0.0f, maxScrollY);
        }
    }

    g_moduleListTargetScrollY = std::clamp(g_moduleListTargetScrollY, 0.0f, maxScrollY);
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    constexpr float kScrollResponse = 18.0f;
    const float blend = 1.0f - std::exp(-kScrollResponse * deltaTime);
    g_moduleListVisualScrollY += (g_moduleListTargetScrollY - g_moduleListVisualScrollY) * blend;

    if (std::fabs(g_moduleListTargetScrollY - g_moduleListVisualScrollY) < 0.25f) {
        g_moduleListVisualScrollY = g_moduleListTargetScrollY;
    }

    const float remaining = std::fabs(g_moduleListTargetScrollY - g_moduleListVisualScrollY);
    g_moduleListScrollMotion = SmoothStep(std::clamp(remaining / 92.0f, 0.0f, 1.0f));
    ImGui::SetScrollY(g_moduleListVisualScrollY);
    g_moduleListAppliedScrollY = g_moduleListVisualScrollY;
}

void EndModuleListScrollAnimationFrame() {
    const float maxScrollY = std::max(0.0f, ImGui::GetScrollMaxY());
    g_moduleListTargetScrollY = std::clamp(g_moduleListTargetScrollY, 0.0f, maxScrollY);
    g_moduleListVisualScrollY = std::clamp(g_moduleListVisualScrollY, 0.0f, maxScrollY);
    g_moduleListAppliedScrollY = g_moduleListVisualScrollY;
    if (maxScrollY <= 0.0f) {
        g_moduleListScrollMotion = 0.0f;
    }
    ImGui::SetScrollY(g_moduleListVisualScrollY);
}

void RenderModuleRow(const char* title, const char* category, bool enabled, void (*setEnabled)(bool), SettingsPanel settingsPanel, int animationIndex = -1) {
    constexpr float kRowHeight = 66.0f;
    constexpr float kRowSpacing = 12.0f;
    constexpr float kToggleWidth = 56.0f;
    constexpr float kToggleHeight = 28.0f;
    constexpr float kGearSize = 34.0f;

    const float rowWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 rowPos = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(rowWidth, kRowHeight);
    const ImVec2 rowMax(rowPos.x + rowSize.x, rowPos.y + rowSize.y);
    const float rowReveal = GetModuleRowRevealProgress(animationIndex);
    float scrollReveal = 1.0f;
    float scrollOffsetY = 0.0f;
    if (g_moduleListScrollMotion > 0.001f) {
        const ImVec2 viewportPos = ImGui::GetWindowPos();
        const ImVec2 viewportSize = ImGui::GetWindowSize();
        const float viewportMinY = viewportPos.y;
        const float viewportMaxY = viewportPos.y + viewportSize.y;
        const float viewportCenterY = (viewportMinY + viewportMaxY) * 0.5f;
        const float rowCenterY = (rowPos.y + rowMax.y) * 0.5f;
        const float edgeDistance = std::min(rowCenterY - viewportMinY, viewportMaxY - rowCenterY);
        const float edgeProgress = SmoothStep(std::clamp(edgeDistance / 72.0f, 0.0f, 1.0f));
        scrollReveal = LerpFloat(1.0f, 0.46f + edgeProgress * 0.54f, g_moduleListScrollMotion);
        scrollOffsetY = (rowCenterY < viewportCenterY ? -1.0f : 1.0f) * (1.0f - edgeProgress) * 10.0f * g_moduleListScrollMotion;
    }
    const bool rowAnimating = rowReveal < 0.995f;
    const float rowOffsetY = (rowAnimating ? -(1.0f - rowReveal) * 12.0f : 0.0f) + scrollOffsetY;
    const ImVec2 visualRowPos(rowPos.x, rowPos.y + rowOffsetY);
    const ImVec2 visualRowMax(rowMax.x, rowMax.y + rowOffsetY);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGui::PushID(title);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * rowReveal * scrollReveal);
    if (rowAnimating) {
        const float revealHeight = std::clamp(kRowHeight * (0.14f + rowReveal * 0.86f), 6.0f, kRowHeight);
        ImGui::PushClipRect(rowPos, ImVec2(rowMax.x, rowPos.y + revealHeight), true);
    }

    DrawPanel(drawList, visualRowPos, visualRowMax, IM_COL32(8, 8, 8, 184), 16.0f, IM_COL32(255, 255, 255, 62));
    DrawModuleStatusStrip(drawList, visualRowPos, visualRowMax, enabled);

    const ImVec2 titlePos(visualRowPos.x + 22.0f, visualRowPos.y + 15.0f);
    drawList->AddText(titlePos, ApplyStyleAlpha(IM_COL32(248, 248, 248, 240)), title);
    drawList->AddText(ImVec2(titlePos.x, titlePos.y + 22.0f), ApplyStyleAlpha(IM_COL32(166, 166, 166, 210)), category);

    const ImVec2 togglePos(visualRowMax.x - kToggleWidth - 18.0f, visualRowPos.y + (kRowHeight - kToggleHeight) * 0.5f);
    ImGui::SetCursorScreenPos(togglePos);
    if (DrawToggleButton("##toggle", enabled, ImVec2(kToggleWidth, kToggleHeight))) {
        setEnabled(!enabled);
    }

    if (settingsPanel != SettingsPanel::None) {
        const ImVec2 gearPos(togglePos.x - kGearSize - 12.0f, visualRowPos.y + (kRowHeight - kGearSize) * 0.5f);
        ImGui::SetCursorScreenPos(gearPos);
        if (DrawGearButton("##settings", ImVec2(kGearSize, kGearSize))) {
            OpenSettingsPanel(settingsPanel);
        }
    }

    if (rowAnimating) {
        ImGui::PopClipRect();
    }
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + rowSize.y));
    ImGui::Dummy(ImVec2(1.0f, kRowSpacing));
    ImGui::PopID();
}

bool RenderModuleRowIfMatched(const char* title, const char* category, bool enabled, void (*setEnabled)(bool), SettingsPanel settingsPanel) {
    if (!MatchesModuleSearch(title, category)) {
        return false;
    }

    const int animationIndex = g_moduleListAnimating ? g_moduleListRenderIndex++ : -1;
    RenderModuleRow(title, category, enabled, setEnabled, settingsPanel, animationIndex);
    return true;
}
