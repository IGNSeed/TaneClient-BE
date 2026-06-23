void RenderItemHudLayoutSelector() {
    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(panelWidth, 82.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));

    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 18.0f, pos.y + 14.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Layout");

    const int currentLayout = tane::gui::GetItemHudLayout();
    const float buttonGap = 10.0f;
    const float buttonY = pos.y + 42.0f;
    const float buttonWidth = std::max(88.0f, (size.x - 36.0f - buttonGap) * 0.5f);
    const char* labels[] = {"Vertical", "Horizontal"};
    for (int layout = 0; layout < 2; ++layout) {
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 18.0f + static_cast<float>(layout) * (buttonWidth + buttonGap), buttonY));
        ImGui::PushID(layout);
        if (DrawCpsModeButton("##itemHudLayout", labels[layout], currentLayout == layout, ImVec2(buttonWidth, 28.0f))) {
            tane::gui::SetItemHudLayout(layout);
        }
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void RenderItemHudDurabilityStyleSelector() {
    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(panelWidth, 82.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));

    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 18.0f, pos.y + 14.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Vertical Style");

    const int currentStyle = tane::gui::GetItemHudDurabilityStyle();
    const float buttonGap = 10.0f;
    const float buttonY = pos.y + 42.0f;
    const float buttonWidth = std::max(88.0f, (size.x - 36.0f - buttonGap) * 0.5f);
    const char* labels[] = {"Text", "Bar"};
    for (int style = 0; style < 2; ++style) {
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 18.0f + static_cast<float>(style) * (buttonWidth + buttonGap), buttonY));
        ImGui::PushID(style);
        if (DrawCpsModeButton("##itemHudDurabilityStyle", labels[style], currentStyle == style, ImVec2(buttonWidth, 28.0f))) {
            tane::gui::SetItemHudDurabilityStyle(style);
        }
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void RenderItemHudSettings() {
    const float closeX = std::max(0.0f, ImGui::GetContentRegionAvail().x - 42.0f);
    ImGui::SetCursorPosX(closeX);
    if (DrawTabButton("X", false, ImVec2(42.0f, 32.0f))) {
        CloseSettingsPanel();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 18.0f));
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize);
    }
    ImGui::TextUnformatted("Item HUD");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Item HUD", "GUI", tane::gui::IsItemHudEnabled(), &tane::gui::SetItemHudEnabled, SettingsPanel::None);
    RenderItemHudLayoutSelector();
    RenderSettingsRow("Durability", tane::gui::IsItemHudDurabilityVisible(), &tane::gui::SetItemHudDurabilityVisible);
    RenderItemHudDurabilityStyleSelector();
}

void RenderCpsModeSelector() {
    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(panelWidth, 82.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));

    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 18.0f, pos.y + 14.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Display");

    const int currentMode = tane::gui::GetCpsDisplayMode();
    const float buttonGap = 8.0f;
    const float buttonY = pos.y + 42.0f;
    const float buttonWidth = std::max(68.0f, (size.x - 36.0f - buttonGap * 2.0f) / 3.0f);
    const char* labels[] = {"Both", "Attack", "Use"};
    for (int mode = 0; mode < 3; ++mode) {
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 18.0f + static_cast<float>(mode) * (buttonWidth + buttonGap), buttonY));
        ImGui::PushID(mode);
        if (DrawCpsModeButton("##cpsMode", labels[mode], currentMode == mode, ImVec2(buttonWidth, 28.0f))) {
            tane::gui::SetCpsDisplayMode(mode);
        }
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void RenderCpsSettings() {
    const float closeX = std::max(0.0f, ImGui::GetContentRegionAvail().x - 42.0f);
    ImGui::SetCursorPosX(closeX);
    if (DrawTabButton("X", false, ImVec2(42.0f, 32.0f))) {
        CloseSettingsPanel();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 18.0f));
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize);
    }
    ImGui::TextUnformatted("CPS");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("CPS", "GUI", tane::gui::IsCpsEnabled(), &tane::gui::SetCpsEnabled, SettingsPanel::None);
    RenderSettingsRow("Black Background", tane::gui::IsCpsBackgroundEnabled(), &tane::gui::SetCpsBackgroundEnabled);
    RenderCpsModeSelector();

    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 sliderPos = ImGui::GetCursorScreenPos();
    const ImVec2 sliderSize(panelWidth, 96.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        sliderPos,
        ImVec2(sliderPos.x + sliderSize.x, sliderPos.y + sliderSize.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));

    float updateInterval = tane::gui::GetCpsUpdateInterval();
    char valueText[32] = {};
    std::snprintf(valueText, sizeof(valueText), "%.2fs", updateInterval);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 15.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Update Time");

    const ImVec2 valueBadgeSize = GetValueInputBadgeSize(valueText);
    const ImVec2 valueBadgePos(sliderPos.x + sliderSize.x - valueBadgeSize.x - 18.0f, sliderPos.y + 12.0f);
    if (DrawFloatValueInputBadge(
            "##cpsUpdateTimeInput",
            &updateInterval,
            tane::gui::GetCpsMinUpdateInterval(),
            tane::gui::GetCpsMaxUpdateInterval(),
            "%.2f",
            valueBadgePos,
            valueBadgeSize)) {
        tane::gui::SetCpsUpdateInterval(updateInterval);
    }

    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 53.0f));
    if (DrawModernSliderFloat(
            "##cpsUpdateTime",
            &updateInterval,
            tane::gui::GetCpsMinUpdateInterval(),
            tane::gui::GetCpsMaxUpdateInterval(),
            ImVec2(sliderSize.x - 36.0f, 30.0f))) {
        tane::gui::SetCpsUpdateInterval(updateInterval);
    }
    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x, sliderPos.y + sliderSize.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void RenderControllerOverlayDeadZoneSlider(
    const char* title,
    const char* sliderId,
    const char* inputId,
    float value,
    void (*setter)(float)) {
    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 sliderPos = ImGui::GetCursorScreenPos();
    const ImVec2 sliderSize(panelWidth, 96.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        sliderPos,
        ImVec2(sliderPos.x + sliderSize.x, sliderPos.y + sliderSize.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));

    char valueText[32] = {};
    std::snprintf(valueText, sizeof(valueText), "%.2f", value);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 15.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), title);

    const float minValue = tane::gui::GetControllerOverlayMinVisualDeadZone();
    const float maxValue = tane::gui::GetControllerOverlayMaxVisualDeadZone();
    const ImVec2 valueBadgeSize = GetValueInputBadgeSize(valueText);
    const ImVec2 valueBadgePos(sliderPos.x + sliderSize.x - valueBadgeSize.x - 18.0f, sliderPos.y + 12.0f);
    if (DrawFloatValueInputBadge(
            inputId,
            &value,
            minValue,
            maxValue,
            "%.2f",
            valueBadgePos,
            valueBadgeSize)) {
        setter(value);
    }

    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 53.0f));
    if (DrawModernSliderFloat(
            sliderId,
            &value,
            minValue,
            maxValue,
            ImVec2(sliderSize.x - 36.0f, 30.0f))) {
        setter(value);
    }
    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x, sliderPos.y + sliderSize.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void RenderControllerOverlaySettings() {
    const float closeX = std::max(0.0f, ImGui::GetContentRegionAvail().x - 42.0f);
    ImGui::SetCursorPosX(closeX);
    if (DrawTabButton("X", false, ImVec2(42.0f, 32.0f))) {
        CloseSettingsPanel();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 18.0f));
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize);
    }
    ImGui::TextUnformatted("Controller Overlay");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Controller Overlay", "GUI", tane::gui::IsControllerOverlayEnabled(), &tane::gui::SetControllerOverlayEnabled, SettingsPanel::None);
    RenderControllerOverlayDeadZoneSlider(
        "Left Stick Dead Zone",
        "##controllerOverlayLeftDeadZone",
        "##controllerOverlayLeftDeadZoneInput",
        tane::gui::GetControllerOverlayLeftVisualDeadZone(),
        &tane::gui::SetControllerOverlayLeftVisualDeadZone);
    RenderControllerOverlayDeadZoneSlider(
        "Right Stick Dead Zone",
        "##controllerOverlayRightDeadZone",
        "##controllerOverlayRightDeadZoneInput",
        tane::gui::GetControllerOverlayRightVisualDeadZone(),
        &tane::gui::SetControllerOverlayRightVisualDeadZone);
}

void DrawKeyComboSettingRow(
    const char* title,
    const char* value,
    bool capturing,
    const char* captureLabel,
    void (*beginCapture)());
bool DrawCompactButton(const char* label, const ImVec2& size, bool emphasized);

void RenderTabSettings() {
    tane::gui::TickTabComboCapture();

    const float closeX = std::max(0.0f, ImGui::GetContentRegionAvail().x - 42.0f);
    ImGui::SetCursorPosX(closeX);
    if (DrawTabButton("X", false, ImVec2(42.0f, 32.0f))) {
        CloseSettingsPanel();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 18.0f));
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize);
    }
    ImGui::TextUnformatted("Tab");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Tab", "GUI", tane::gui::IsTabEnabled(), &tane::gui::SetTabEnabled, SettingsPanel::None);

    ImGui::TextDisabled("Hold Combo");
    ImGui::Dummy(ImVec2(1.0f, 2.0f));
    DrawKeyComboSettingRow(
        "Keyboard",
        tane::gui::GetTabKeyboardComboLabel(),
        tane::gui::IsTabKeyboardComboCaptureActive(),
        "Press combo, release to save",
        &tane::gui::BeginTabKeyboardComboCapture);
    DrawKeyComboSettingRow(
        "Controller",
        tane::gui::GetTabControllerComboLabel(),
        tane::gui::IsTabControllerComboCaptureActive(),
        "Press combo, release to save",
        &tane::gui::BeginTabControllerComboCapture);
    if (tane::gui::IsTabKeyboardComboCaptureActive() ||
        tane::gui::IsTabControllerComboCaptureActive()) {
        if (DrawCompactButton("Cancel Capture", ImVec2(136.0f, 32.0f), false)) {
            tane::gui::CancelTabComboCapture();
        }
        ImGui::Dummy(ImVec2(1.0f, 12.0f));
    }
}

void RenderEffectHudSettings() {
    const float closeX = std::max(0.0f, ImGui::GetContentRegionAvail().x - 42.0f);
    ImGui::SetCursorPosX(closeX);
    if (DrawTabButton("X", false, ImVec2(42.0f, 32.0f))) {
        CloseSettingsPanel();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 18.0f));
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize);
    }
    ImGui::TextUnformatted("Effect HUD");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Effect HUD", "GUI", tane::gui::IsEffectHudEnabled(), &tane::gui::SetEffectHudEnabled, SettingsPanel::None);
    RenderSettingsRow("Black Background", tane::gui::IsEffectHudBackgroundEnabled(), &tane::gui::SetEffectHudBackgroundEnabled);
}

void RenderTracerSettings() {
    const float closeX = std::max(0.0f, ImGui::GetContentRegionAvail().x - 42.0f);
    ImGui::SetCursorPosX(closeX);
    if (DrawTabButton("X", false, ImVec2(42.0f, 32.0f))) {
        CloseSettingsPanel();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 18.0f));
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize);
    }
    ImGui::TextUnformatted("Tracer");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Tracer", "Render", tane::render::IsTracerEnabled(), &tane::render::SetTracerEnabled, SettingsPanel::None);

    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 panelPos = ImGui::GetCursorScreenPos();
    const ImVec2 panelSize(panelWidth, 372.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        panelPos,
        ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));

    float color[4]{};
    tane::render::GetTracerLineColor(color[0], color[1], color[2], color[3]);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(panelPos.x + 18.0f, panelPos.y + 15.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Line Color");

    const ImVec2 previewMin(panelPos.x + panelSize.x - 56.0f, panelPos.y + 13.0f);
    const ImVec2 previewMax(panelPos.x + panelSize.x - 18.0f, panelPos.y + 37.0f);
    drawList->AddRectFilled(previewMin, previewMax, ApplyStyleAlpha(ColorToU32(color[0], color[1], color[2], color[3])), 8.0f);
    drawList->AddRect(previewMin, previewMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 104)), 8.0f, 0, 1.0f);

    drawList->AddText(ImVec2(panelPos.x + 18.0f, panelPos.y + 50.0f), ApplyStyleAlpha(IM_COL32(180, 180, 180, 224)), "HEX");
    const ImVec2 hexPos(panelPos.x + 64.0f, panelPos.y + 43.0f);
    const float hexWidth = std::max(126.0f, panelSize.x - 82.0f);
    if (DrawHexColorInput(
            "##tracerHexColor",
            color,
            hexPos,
            hexWidth,
            g_tracerHexInput,
            sizeof(g_tracerHexInput),
            g_tracerHexInputEditing)) {
        tane::render::SetTracerLineColor(color[0], color[1], color[2], color[3]);
    }

    const ImVec2 palettePos(panelPos.x + 18.0f, panelPos.y + 88.0f);
    const float paletteWidth = panelSize.x - 36.0f;
    if (DrawTracerColorPalette("##tracerColorPalette", color, palettePos, paletteWidth)) {
        tane::render::SetTracerLineColor(color[0], color[1], color[2], color[3]);
    }

    ImGui::SetCursorScreenPos(ImVec2(panelPos.x, panelPos.y + panelSize.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void RenderHitboxSettings() {
    const float closeX = std::max(0.0f, ImGui::GetContentRegionAvail().x - 42.0f);
    ImGui::SetCursorPosX(closeX);
    if (DrawTabButton("X", false, ImVec2(42.0f, 32.0f))) {
        CloseSettingsPanel();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 18.0f));
    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    if (boldFont != nullptr) {
        ImGui::PushFont(boldFont, boldFont->LegacySize);
    }
    ImGui::TextUnformatted("Hitbox");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Hitbox", "Render", tane::render::IsHitboxEnabled(), &tane::render::SetHitboxEnabled, SettingsPanel::None);

    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 panelPos = ImGui::GetCursorScreenPos();
    const ImVec2 panelSize(panelWidth, 372.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        panelPos,
        ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));

    float color[4]{};
    tane::render::GetHitboxColor(color[0], color[1], color[2], color[3]);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(panelPos.x + 18.0f, panelPos.y + 15.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Box Color");

    const ImVec2 previewMin(panelPos.x + panelSize.x - 56.0f, panelPos.y + 13.0f);
    const ImVec2 previewMax(panelPos.x + panelSize.x - 18.0f, panelPos.y + 37.0f);
    drawList->AddRectFilled(previewMin, previewMax, ApplyStyleAlpha(ColorToU32(color[0], color[1], color[2], color[3])), 8.0f);
    drawList->AddRect(previewMin, previewMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 104)), 8.0f, 0, 1.0f);

    drawList->AddText(ImVec2(panelPos.x + 18.0f, panelPos.y + 50.0f), ApplyStyleAlpha(IM_COL32(180, 180, 180, 224)), "HEX");
    const ImVec2 hexPos(panelPos.x + 64.0f, panelPos.y + 43.0f);
    const float hexWidth = std::max(126.0f, panelSize.x - 82.0f);
    if (DrawHexColorInput(
            "##hitboxHexColor",
            color,
            hexPos,
            hexWidth,
            g_hitboxHexInput,
            sizeof(g_hitboxHexInput),
            g_hitboxHexInputEditing)) {
        tane::render::SetHitboxColor(color[0], color[1], color[2], color[3]);
    }

    const ImVec2 palettePos(panelPos.x + 18.0f, panelPos.y + 88.0f);
    const float paletteWidth = panelSize.x - 36.0f;
    if (DrawTracerColorPalette("##hitboxColorPalette", color, palettePos, paletteWidth)) {
        tane::render::SetHitboxColor(color[0], color[1], color[2], color[3]);
    }

    ImGui::SetCursorScreenPos(ImVec2(panelPos.x, panelPos.y + panelSize.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void DrawKeyComboSettingRow(
    const char* title,
    const char* value,
    bool capturing,
    const char* captureLabel,
    void (*beginCapture)());
bool DrawCompactButton(const char* label, const ImVec2& size, bool emphasized = false);
