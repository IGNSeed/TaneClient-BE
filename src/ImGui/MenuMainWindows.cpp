void RenderAppSettingsWindow(const ImVec2& displaySize, float appearProgress, bool allowInputs) {
    const float eased = SmoothStep(g_appSettingsProgress);
    const float menuFade = SmoothStep(appearProgress);
    const float alpha = SmoothStep(std::clamp((g_appSettingsProgress - 0.06f) / 0.94f, 0.0f, 1.0f)) * menuFade;
    if (alpha <= 0.0f) {
        return;
    }

    const ImVec2 windowSize(
        std::clamp(displaySize.x * 0.58f, 560.0f, 760.0f),
        314.0f);
    const ImVec2 targetPos(
        std::floor((displaySize.x - windowSize.x) * 0.5f),
        std::floor((displaySize.y - windowSize.y) * 0.5f));
    const ImVec2 windowPos(targetPos.x, targetPos.y + (1.0f - eased) * 26.0f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;
    if (!allowInputs || g_appSettingsProgress < 0.96f) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 18.0f));
    if (!ImGui::Begin("##TaneClientAppSettings", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    DrawPanel(
        ImGui::GetWindowDrawList(),
        windowPos,
        ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
        IM_COL32(0, 0, 0, 184),
        20.0f,
        IM_COL32(255, 255, 255, 152));

    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize + 2.0f);
    }
    ImGui::TextUnformatted("Settings");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(windowSize.x - 64.0f);
    if (DrawCompactButton("X", ImVec2(42.0f, 30.0f))) {
        CloseAppSettings();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Key Combo");
    ImGui::Spacing();

    DrawKeyComboSettingRow(
        "Keyboard",
        tane::gui::GetKeyboardToggleComboLabel(),
        tane::gui::IsKeyboardToggleCaptureActive(),
        "Press a key combination",
        &tane::gui::BeginKeyboardToggleCapture);
    DrawKeyComboSettingRow(
        "Controller",
        tane::gui::GetControllerToggleComboLabel(),
        tane::gui::IsControllerToggleCaptureActive(),
        "Release, then hold a combo",
        &tane::gui::BeginControllerToggleCapture);

    const float footerY = windowSize.y - 48.0f;
    ImGui::SetCursorPos(ImVec2(22.0f, footerY));
    if (DrawCompactButton("Reset Default", ImVec2(132.0f, 32.0f))) {
        tane::gui::ResetToggleComboConfig();
    }
    ImGui::SameLine(0.0f, 10.0f);
    if (DrawCompactButton("Cancel Capture", ImVec2(136.0f, 32.0f))) {
        tane::gui::CancelToggleComboCapture();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void RenderMainWindow(const MenuLayout& layout) {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowPos(layout.mainPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.mainSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
    if (!ImGui::Begin("##TaneClientMenu", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    DrawPanel(
        ImGui::GetWindowDrawList(),
        layout.mainPos,
        ImVec2(layout.mainPos.x + layout.mainSize.x, layout.mainPos.y + layout.mainSize.y),
        IM_COL32(0, 0, 0, 174),
        20.0f,
        IM_COL32(255, 255, 255, 168));

    RenderModuleHeader();

    const bool moduleListInputsReady = g_tabSwitchProgress >= GetModuleListAnimationEnd();
    ImGuiWindowFlags listFlags = ImGuiWindowFlags_NoScrollWithMouse;
    if (!moduleListInputsReady) {
        listFlags |= ImGuiWindowFlags_NoInputs;
    }
    if (ImGui::BeginChild("##ModuleList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, listFlags)) {
        BeginModuleListScrollAnimationFrame(moduleListInputsReady);
        g_moduleListAnimating = g_tabSwitchProgress < GetModuleListAnimationEnd();
        g_moduleListRenderIndex = 0;
        const int visibleModuleCount = RenderSelectedModuleList();
        g_moduleListAnimationRowCount = std::max(1, visibleModuleCount);
        if (visibleModuleCount == 0) {
            RenderEmptyModuleSearchResult();
        }
        g_moduleListAnimating = false;
        EndModuleListScrollAnimationFrame();
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}

void RenderSettingsWindow(const MenuLayout& layout, float menuVisibility = 1.0f, bool menuClosing = false) {
    if (g_visibleSettingsPanel == SettingsPanel::None || g_settingsPanelProgress <= 0.0f) {
        return;
    }

    const float eased = SmoothStep(g_settingsPanelProgress);
    const float closeProgress = SmoothStep(1.0f - std::clamp(menuVisibility, 0.0f, 1.0f));
    const float slideDistance = std::min(144.0f, layout.settingsSize.x * 0.42f);
    const bool openingOrOpen = g_openSettingsPanel != SettingsPanel::None;
    const float openSlideOffset = openingOrOpen ? -(1.0f - eased) * slideDistance : 0.0f;
    const float closeSlideOffset = menuClosing ? closeProgress * std::min(96.0f, layout.settingsSize.x * 0.28f) : 0.0f;
    const float slideOffset = openSlideOffset + closeSlideOffset;
    const float panelAlpha = eased * (menuClosing ? (1.0f - closeProgress * 0.34f) : 1.0f);
    const ImVec2 clipMin = layout.settingsPos;
    const ImVec2 clipMax(layout.settingsPos.x + layout.settingsSize.x, layout.settingsPos.y + layout.settingsSize.y);
    const ImVec2 panelPos(layout.settingsPos.x + slideOffset, layout.settingsPos.y);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;

    if (g_settingsPanelProgress < 0.995f || menuClosing) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.settingsSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * panelAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
    if (!ImGui::Begin("##TaneClientSettingsPanel", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    ImGui::PushClipRect(clipMin, clipMax, true);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        panelPos,
        ImVec2(panelPos.x + layout.settingsSize.x, panelPos.y + layout.settingsSize.y),
        IM_COL32(0, 0, 0, 174),
        20.0f,
        IM_COL32(255, 255, 255, 168));

    const float contentEased = SmoothStep(g_settingsPanelContentProgress);
    const float contentOffset = (1.0f - contentEased) * 28.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + contentOffset);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * contentEased);
    ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, 0.0f), false);
    if (g_visibleSettingsPanel == SettingsPanel::ItemHudSettings) {
        RenderItemHudSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::FpsSettings) {
        RenderFpsSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::CpsSettings) {
        RenderCpsSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::PingSettings) {
        RenderPingSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::ArrowCounterSettings) {
        RenderArrowCounterSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::PotCounterSettings) {
        RenderPotCounterSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::ControllerOverlaySettings) {
        RenderControllerOverlaySettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::TabSettings) {
        RenderTabSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::EffectHudSettings) {
        RenderEffectHudSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::HitboxSettings) {
        RenderHitboxSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::TracerSettings) {
        RenderTracerSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::ZoomSettings) {
        RenderZoomSettings();
    } else if (g_visibleSettingsPanel == SettingsPanel::FreeLookSettings) {
        RenderFreeLookSettings();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopClipRect();

    ImGui::End();
    ImGui::PopStyleVar(2);
}
