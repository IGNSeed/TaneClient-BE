int RenderGuiModules() {
    int visibleCount = 0;
    visibleCount += RenderModuleRowIfMatched("Item HUD", "GUI", tane::gui::IsItemHudEnabled(), &tane::gui::SetItemHudEnabled, SettingsPanel::ItemHudSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("FPS", "GUI", tane::gui::IsFpsEnabled(), &tane::gui::SetFpsEnabled, SettingsPanel::FpsSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("CPS", "GUI", tane::gui::IsCpsEnabled(), &tane::gui::SetCpsEnabled, SettingsPanel::CpsSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Ping", "GUI", tane::gui::IsPingEnabled(), &tane::gui::SetPingEnabled, SettingsPanel::PingSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("KeyStroke", "GUI", tane::gui::IsKeyStrokeEnabled(), &tane::gui::SetKeyStrokeEnabled, SettingsPanel::None) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Controller Overlay", "GUI", tane::gui::IsControllerOverlayEnabled(), &tane::gui::SetControllerOverlayEnabled, SettingsPanel::ControllerOverlaySettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Tab", "GUI", tane::gui::IsTabEnabled(), &tane::gui::SetTabEnabled, SettingsPanel::TabSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Arrow Counter", "GUI", tane::gui::IsArrowCounterEnabled(), &tane::gui::SetArrowCounterEnabled, SettingsPanel::ArrowCounterSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Pot Counter", "GUI", tane::gui::IsPotCounterEnabled(), &tane::gui::SetPotCounterEnabled, SettingsPanel::PotCounterSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Effect HUD", "GUI", tane::gui::IsEffectHudEnabled(), &tane::gui::SetEffectHudEnabled, SettingsPanel::EffectHudSettings) ? 1 : 0;
    return visibleCount;
}

int RenderRenderModules() {
    int visibleCount = 0;
    visibleCount += RenderModuleRowIfMatched("Name Tags", "Render", tane::render::IsNameTagsEnabled(), &tane::render::SetNameTagsEnabled, SettingsPanel::None) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Fullbright", "Render", tane::render::IsFullbrightEnabled(), &tane::render::SetFullbrightEnabled, SettingsPanel::None) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("NoFog", "Render", tane::render::IsNoFogEnabled(), &tane::render::SetNoFogEnabled, SettingsPanel::None) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Hitbox", "Render", tane::render::IsHitboxEnabled(), &tane::render::SetHitboxEnabled, SettingsPanel::HitboxSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("Tracer", "Render", tane::render::IsTracerEnabled(), &tane::render::SetTracerEnabled, SettingsPanel::TracerSettings) ? 1 : 0;
    return visibleCount;
}

int RenderCameraModules() {
    int visibleCount = 0;
    visibleCount += RenderModuleRowIfMatched("Zoom", "Camera", tane::camera::IsZoomEnabled(), &tane::camera::SetZoomEnabled, SettingsPanel::ZoomSettings) ? 1 : 0;
    visibleCount += RenderModuleRowIfMatched("FreeLook", "Camera", tane::camera::IsFreeLookEnabled(), &tane::camera::SetFreeLookEnabled, SettingsPanel::FreeLookSettings) ? 1 : 0;
    return visibleCount;
}

int RenderPatchModules() {
    int visibleCount = 0;
    visibleCount += RenderModuleRowIfMatched("ForceCloseOreUI", "Patch", tane::patch::IsForceCloseOreUiEnabled(), &tane::patch::SetForceCloseOreUiEnabled, SettingsPanel::None) ? 1 : 0;
    return visibleCount;
}

int RenderAllModules() {
    int visibleCount = 0;
    visibleCount += RenderModuleRowIfMatched("ForceCloseOreUI", "Patch", tane::patch::IsForceCloseOreUiEnabled(), &tane::patch::SetForceCloseOreUiEnabled, SettingsPanel::None) ? 1 : 0;
    visibleCount += RenderGuiModules();
    visibleCount += RenderRenderModules();
    visibleCount += RenderCameraModules();
    visibleCount += RenderModuleRowIfMatched("Auto Sprint", "Movement", tane::movement::IsAutoSprintEnabled(), &tane::movement::SetAutoSprintEnabled, SettingsPanel::None) ? 1 : 0;
    return visibleCount;
}

int RenderSelectedModuleList() {
    switch (g_selectedMenuTab) {
    case MenuTab::Gui:
        return RenderGuiModules();
    case MenuTab::Render:
        return RenderRenderModules();
    case MenuTab::Camera:
        return RenderCameraModules();
    case MenuTab::Patch:
        return RenderPatchModules();
    case MenuTab::All:
    default:
        return RenderAllModules();
    }
}

void RenderEmptyModuleSearchResult() {
    const float rowWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 rowPos = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(rowWidth, 76.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        rowPos,
        ImVec2(rowPos.x + rowSize.x, rowPos.y + rowSize.y),
        IM_COL32(8, 8, 8, 150),
        16.0f,
        IM_COL32(255, 255, 255, 48));

    const char* text = "No matching modules";
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + (rowSize.x - textSize.x) * 0.5f, rowPos.y + (rowSize.y - textSize.y) * 0.5f),
        ApplyStyleAlpha(IM_COL32(180, 180, 180, 220)),
        text);
    ImGui::Dummy(rowSize);
}

void RenderModuleHeader() {
    constexpr float kHeaderHeightWide = 76.0f;
    constexpr float kHeaderHeightNarrow = 104.0f;
    constexpr float kInnerPaddingX = 16.0f;
    constexpr float kInnerPaddingY = 12.0f;
    constexpr float kSearchHeight = 34.0f;

    const float headerWidth = std::max(280.0f, ImGui::GetContentRegionAvail().x);
    const bool narrow = headerWidth < 540.0f;
    const float headerHeight = narrow ? kHeaderHeightNarrow : kHeaderHeightWide;
    const ImVec2 headerPos = ImGui::GetCursorScreenPos();
    const ImVec2 headerMax(headerPos.x + headerWidth, headerPos.y + headerHeight);

    DrawPanel(
        ImGui::GetWindowDrawList(),
        headerPos,
        headerMax,
        IM_COL32(6, 6, 6, 184),
        18.0f,
        IM_COL32(255, 255, 255, 104));

    ImFont* boldFont = tane::payload::GetMenuBoldFont();
    const char* title = "Modules";
    const char* tabLabel = GetSelectedTabLabel();
    const float titleFontSize = boldFont != nullptr ? boldFont->LegacySize + 2.0f : ImGui::GetFontSize() + 2.0f;
    const ImVec2 titlePos(headerPos.x + kInnerPaddingX, headerPos.y + kInnerPaddingY);
    ImGui::GetWindowDrawList()->AddText(
        boldFont,
        titleFontSize,
        titlePos,
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 244)),
        title);

    const ImVec2 tabTextSize = ImGui::CalcTextSize(tabLabel);
    const ImVec2 badgePos(titlePos.x, titlePos.y + 30.0f);
    const ImVec2 badgeSize(tabTextSize.x + 20.0f, 22.0f);
    ImGui::GetWindowDrawList()->AddRectFilled(
        badgePos,
        ImVec2(badgePos.x + badgeSize.x, badgePos.y + badgeSize.y),
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 32)),
        12.0f);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(badgePos.x + 11.0f, badgePos.y + (badgeSize.y - tabTextSize.y) * 0.5f),
        ApplyStyleAlpha(IM_COL32(205, 205, 205, 232)),
        tabLabel);

    const float searchWidth = narrow ? headerWidth - kInnerPaddingX * 2.0f : std::clamp(headerWidth * 0.42f, 220.0f, 330.0f);
    const ImVec2 searchPos(
        narrow ? headerPos.x + kInnerPaddingX : headerMax.x - kInnerPaddingX - searchWidth,
        narrow ? headerPos.y + headerHeight - kInnerPaddingY - kSearchHeight : headerPos.y + (headerHeight - kSearchHeight) * 0.5f);

    ImGui::SetCursorScreenPos(searchPos);
    ImGui::SetNextItemWidth(searchWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.115f, 0.120f, 0.132f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.155f, 0.165f, 0.182f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.185f, 0.198f, 0.218f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.42f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.99f, 1.0f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.76f, 0.78f, 0.82f, 0.92f));
    if (ImGui::InputTextWithHint(
        "##ModuleSearch",
        "Search modules",
        g_moduleSearch,
        sizeof(g_moduleSearch),
        ImGuiInputTextFlags_EscapeClearsAll)) {
        ResetModuleListScrollAnimation();
    }
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    ImGui::SetCursorScreenPos(ImVec2(headerPos.x, headerPos.y + headerHeight));
    ImGui::Dummy(ImVec2(1.0f, 16.0f));
}

void RenderSettingsRow(const char* title, bool value, void (*setValue)(bool)) {
    const float panelWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(panelWidth, 58.0f);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(8, 8, 8, 184),
        15.0f,
        IM_COL32(255, 255, 255, 62));
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 18.0f, pos.y + 18.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 235)), title);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 74.0f, pos.y + 14.0f));
    if (DrawToggleButton("##settingsToggle", value, ImVec2(56.0f, 28.0f))) {
        setValue(!value);
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

bool DrawModernSliderFloat(const char* id, float* value, float minValue, float maxValue, const ImVec2& size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    const float range = std::max(0.0001f, maxValue - minValue);
    float normalized = std::clamp((*value - minValue) / range, 0.0f, 1.0f);
    bool changed = false;
    if ((pressed || active) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float trackStart = pos.x + 4.0f;
        const float trackEnd = pos.x + size.x - 4.0f;
        normalized = std::clamp((ImGui::GetIO().MousePos.x - trackStart) / std::max(1.0f, trackEnd - trackStart), 0.0f, 1.0f);
        *value = minValue + range * normalized;
        changed = true;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float trackStart = std::floor(pos.x + 4.0f);
    const float trackEnd = std::floor(pos.x + size.x - 4.0f);
    const float trackY = std::floor(pos.y + size.y * 0.5f);
    const float trackHeight = 9.0f;
    const ImVec2 trackMin(trackStart, trackY - trackHeight * 0.5f);
    const ImVec2 trackMax(trackEnd, trackY + trackHeight * 0.5f);
    const float fillX = trackStart + (trackEnd - trackStart) * normalized;
    const ImVec2 fillMax(fillX, trackMax.y);

    drawList->AddRectFilled(trackMin, trackMax, ApplyStyleAlpha(IM_COL32(24, 25, 28, 238)), trackHeight * 0.5f);
    drawList->AddRect(trackMin, trackMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 82 : 48)), trackHeight * 0.5f, 0, 1.0f);
    if (fillX > trackMin.x + 1.0f) {
        drawList->AddRectFilled(trackMin, fillMax, ApplyStyleAlpha(active ? IM_COL32(255, 255, 255, 248) : IM_COL32(226, 226, 226, 236)), trackHeight * 0.5f);
    }

    const ImVec2 knobCenter(fillX, trackY);
    const float knobRadius = active ? 9.5f : (hovered ? 8.8f : 8.0f);
    if (hovered || active) {
        drawList->AddCircleFilled(knobCenter, knobRadius + 5.0f, ApplyStyleAlpha(IM_COL32(255, 255, 255, active ? 30 : 18)), 32);
    }
    drawList->AddCircleFilled(knobCenter, knobRadius, ApplyStyleAlpha(IM_COL32(12, 12, 12, 250)), 32);
    drawList->AddCircle(knobCenter, knobRadius - 0.5f, ApplyStyleAlpha(IM_COL32(255, 255, 255, 230)), 32, 1.7f);
    return changed;
}

ImU32 ColorToU32(float r, float g, float b, float a) {
    return IM_COL32(
        static_cast<int>(std::clamp(r, 0.0f, 1.0f) * 255.0f),
        static_cast<int>(std::clamp(g, 0.0f, 1.0f) * 255.0f),
        static_cast<int>(std::clamp(b, 0.0f, 1.0f) * 255.0f),
        static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255.0f));
}

bool DrawTracerColorPalette(const char* id, float color[4], const ImVec2& pos, float width) {
    ImGui::PushID(id);

    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    ImGui::ColorConvertRGBtoHSV(color[0], color[1], color[2], hue, saturation, value);
    if (!std::isfinite(hue)) {
        hue = 0.0f;
    }

    const float paletteSize = std::clamp(width - 76.0f, 144.0f, 176.0f);
    constexpr float kHueWidth = 18.0f;
    constexpr float kGap = 12.0f;
    constexpr float kAlphaHeight = 18.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    bool changed = false;

    float hueR = 1.0f;
    float hueG = 1.0f;
    float hueB = 1.0f;
    ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, hueR, hueG, hueB);

    const ImVec2 svMin(pos.x, pos.y);
    const ImVec2 svMax(pos.x + paletteSize, pos.y + paletteSize);
    drawList->AddRectFilledMultiColor(
        svMin,
        svMax,
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 255)),
        ApplyStyleAlpha(ColorToU32(hueR, hueG, hueB, 1.0f)),
        ApplyStyleAlpha(IM_COL32(0, 0, 0, 255)),
        ApplyStyleAlpha(IM_COL32(0, 0, 0, 255)));
    drawList->AddRect(svMin, svMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 92)), 8.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(svMin);
    if (ImGui::InvisibleButton("##sv", ImVec2(paletteSize, paletteSize)) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        changed = true;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        saturation = std::clamp((mouse.x - svMin.x) / paletteSize, 0.0f, 1.0f);
        value = 1.0f - std::clamp((mouse.y - svMin.y) / paletteSize, 0.0f, 1.0f);
        changed = true;
    }

    const ImVec2 marker(
        svMin.x + saturation * paletteSize,
        svMin.y + (1.0f - value) * paletteSize);
    drawList->AddCircleFilled(marker, 6.0f, ApplyStyleAlpha(IM_COL32(0, 0, 0, 190)), 24);
    drawList->AddCircle(marker, 5.0f, ApplyStyleAlpha(IM_COL32(255, 255, 255, 238)), 24, 1.5f);

    const ImVec2 hueMin(svMax.x + kGap, svMin.y);
    const ImVec2 hueMax(hueMin.x + kHueWidth, svMax.y);
    constexpr int kHueSegments = 12;
    for (int i = 0; i < kHueSegments; ++i) {
        const float y0 = hueMin.y + paletteSize * (static_cast<float>(i) / kHueSegments);
        const float y1 = hueMin.y + paletteSize * (static_cast<float>(i + 1) / kHueSegments);
        float r0 = 1.0f;
        float g0 = 1.0f;
        float b0 = 1.0f;
        float r1 = 1.0f;
        float g1 = 1.0f;
        float b1 = 1.0f;
        ImGui::ColorConvertHSVtoRGB(static_cast<float>(i) / kHueSegments, 1.0f, 1.0f, r0, g0, b0);
        ImGui::ColorConvertHSVtoRGB(static_cast<float>(i + 1) / kHueSegments, 1.0f, 1.0f, r1, g1, b1);
        drawList->AddRectFilledMultiColor(
            ImVec2(hueMin.x, y0),
            ImVec2(hueMax.x, y1 + 1.0f),
            ApplyStyleAlpha(ColorToU32(r0, g0, b0, 1.0f)),
            ApplyStyleAlpha(ColorToU32(r0, g0, b0, 1.0f)),
            ApplyStyleAlpha(ColorToU32(r1, g1, b1, 1.0f)),
            ApplyStyleAlpha(ColorToU32(r1, g1, b1, 1.0f)));
    }
    drawList->AddRect(hueMin, hueMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 92)), 9.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(hueMin);
    if (ImGui::InvisibleButton("##hue", ImVec2(kHueWidth, paletteSize)) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        changed = true;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float mouseY = ImGui::GetIO().MousePos.y;
        hue = std::clamp((mouseY - hueMin.y) / paletteSize, 0.0f, 1.0f);
        changed = true;
    }

    const float hueMarkerY = hueMin.y + hue * paletteSize;
    drawList->AddLine(
        ImVec2(hueMin.x - 4.0f, hueMarkerY),
        ImVec2(hueMax.x + 4.0f, hueMarkerY),
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 240)),
        2.0f);

    if (changed) {
        ImGui::ColorConvertHSVtoRGB(hue, saturation, value, color[0], color[1], color[2]);
    }

    const ImVec2 alphaMin(pos.x, svMax.y + 16.0f);
    const ImVec2 alphaMax(pos.x + width, alphaMin.y + kAlphaHeight);
    drawList->AddRectFilledMultiColor(
        alphaMin,
        alphaMax,
        ApplyStyleAlpha(ColorToU32(color[0], color[1], color[2], 0.12f)),
        ApplyStyleAlpha(ColorToU32(color[0], color[1], color[2], 1.0f)),
        ApplyStyleAlpha(ColorToU32(color[0], color[1], color[2], 1.0f)),
        ApplyStyleAlpha(ColorToU32(color[0], color[1], color[2], 0.12f)));
    drawList->AddRect(alphaMin, alphaMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 76)), 9.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(alphaMin);
    if (ImGui::InvisibleButton("##alpha", ImVec2(width, kAlphaHeight)) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        changed = true;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        color[3] = std::clamp((ImGui::GetIO().MousePos.x - alphaMin.x) / std::max(1.0f, width), 0.0f, 1.0f);
        changed = true;
    }

    const float alphaX = alphaMin.x + std::clamp(color[3], 0.0f, 1.0f) * width;
    drawList->AddCircleFilled(ImVec2(alphaX, (alphaMin.y + alphaMax.y) * 0.5f), 7.0f, ApplyStyleAlpha(IM_COL32(12, 12, 12, 245)), 24);
    drawList->AddCircle(ImVec2(alphaX, (alphaMin.y + alphaMax.y) * 0.5f), 6.0f, ApplyStyleAlpha(IM_COL32(255, 255, 255, 230)), 24, 1.4f);

    const ImVec2 swatchStart(pos.x, alphaMax.y + 14.0f);
    constexpr float kSwatch = 22.0f;
    constexpr float kSwatchGap = 8.0f;
    const float presets[][3] = {
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.24f, 0.24f},
        {1.0f, 0.82f, 0.18f},
        {0.26f, 1.0f, 0.48f},
        {0.24f, 0.72f, 1.0f},
        {0.78f, 0.44f, 1.0f},
    };
    for (int i = 0; i < static_cast<int>(sizeof(presets) / sizeof(presets[0])); ++i) {
        const ImVec2 swatchMin(swatchStart.x + i * (kSwatch + kSwatchGap), swatchStart.y);
        const ImVec2 swatchMax(swatchMin.x + kSwatch, swatchMin.y + kSwatch);
        drawList->AddRectFilled(
            swatchMin,
            swatchMax,
            ApplyStyleAlpha(ColorToU32(presets[i][0], presets[i][1], presets[i][2], 1.0f)),
            7.0f);
        drawList->AddRect(swatchMin, swatchMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 96)), 7.0f, 0, 1.0f);

        ImGui::SetCursorScreenPos(swatchMin);
        char buttonId[16]{};
        std::snprintf(buttonId, sizeof(buttonId), "##preset%d", i);
        if (ImGui::InvisibleButton(buttonId, ImVec2(kSwatch, kSwatch))) {
            color[0] = presets[i][0];
            color[1] = presets[i][1];
            color[2] = presets[i][2];
            changed = true;
        }
    }

    ImGui::PopID();
    return changed;
}

ImVec2 GetValueInputBadgeSize(const char* displayText) {
    const ImVec2 valueSize = ImGui::CalcTextSize(displayText);
    return ImVec2(std::max(66.0f, valueSize.x + 26.0f), 28.0f);
}

void PushValueInputBadgeStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 13.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.11f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.20f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.96f, 0.96f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(1.0f, 1.0f, 1.0f, 0.22f));
}

void PopValueInputBadgeStyle() {
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);
}

int HexDigitValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + c - 'A';
    }
    return -1;
}

void FormatHexColor(const float color[4], char* buffer, std::size_t bufferSize) {
    const int r = static_cast<int>(std::clamp(color[0], 0.0f, 1.0f) * 255.0f + 0.5f);
    const int g = static_cast<int>(std::clamp(color[1], 0.0f, 1.0f) * 255.0f + 0.5f);
    const int b = static_cast<int>(std::clamp(color[2], 0.0f, 1.0f) * 255.0f + 0.5f);
    const int a = static_cast<int>(std::clamp(color[3], 0.0f, 1.0f) * 255.0f + 0.5f);
    std::snprintf(buffer, bufferSize, "#%02X%02X%02X%02X", r, g, b, a);
}

bool ParseHexColor(const char* text, float color[4]) {
    if (text == nullptr) {
        return false;
    }

    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    if (*text == '#') {
        ++text;
    }

    char digits[9]{};
    int digitCount = 0;
    while (*text != '\0' && *text != ' ' && *text != '\t' && digitCount < 8) {
        if (HexDigitValue(*text) < 0) {
            return false;
        }
        digits[digitCount++] = *text++;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    if (*text != '\0' || (digitCount != 6 && digitCount != 8)) {
        return false;
    }

    int values[4] = {
        255,
        255,
        255,
        static_cast<int>(std::clamp(color[3], 0.0f, 1.0f) * 255.0f + 0.5f),
    };
    for (int i = 0; i < digitCount / 2; ++i) {
        const int hi = HexDigitValue(digits[i * 2]);
        const int lo = HexDigitValue(digits[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        values[i] = hi * 16 + lo;
    }

    color[0] = values[0] / 255.0f;
    color[1] = values[1] / 255.0f;
    color[2] = values[2] / 255.0f;
    color[3] = values[3] / 255.0f;
    return true;
}

bool DrawHexColorInput(
    const char* id,
    float color[4],
    const ImVec2& pos,
    float width,
    char* input,
    std::size_t inputSize,
    bool& editing) {
    if (!editing) {
        FormatHexColor(color, input, inputSize);
    }

    ImGui::SetCursorScreenPos(pos);
    ImGui::SetNextItemWidth(width);
    PushValueInputBadgeStyle();
    const bool edited = ImGui::InputText(
        id,
        input,
        inputSize,
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank);
    const bool active = ImGui::IsItemActive();
    const bool finishedEdit = ImGui::IsItemDeactivatedAfterEdit();
    PopValueInputBadgeStyle();

    editing = active;
    bool changed = false;
    if ((edited || finishedEdit) && ParseHexColor(input, color)) {
        changed = true;
    }
    if (!active && !ParseHexColor(input, color)) {
        FormatHexColor(color, input, inputSize);
    }
    return changed;
}

bool DrawFloatValueInputBadge(
    const char* id,
    float* value,
    float minValue,
    float maxValue,
    const char* format,
    const ImVec2& pos,
    const ImVec2& size) {
    float editValue = *value;
    ImGui::SetCursorScreenPos(pos);
    ImGui::SetNextItemWidth(size.x);
    PushValueInputBadgeStyle();
    const bool submitted = ImGui::InputFloat(
        id,
        &editValue,
        0.0f,
        0.0f,
        format,
        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    const bool finishedEdit = ImGui::IsItemDeactivatedAfterEdit();
    PopValueInputBadgeStyle();

    if (!submitted && !finishedEdit) {
        return false;
    }

    const float clampedValue = std::clamp(editValue, minValue, maxValue);
    if (std::fabs(clampedValue - *value) <= FLT_EPSILON) {
        return false;
    }

    *value = clampedValue;
    return true;
}

bool DrawIntValueInputBadge(
    const char* id,
    int* value,
    int minValue,
    int maxValue,
    const ImVec2& pos,
    const ImVec2& size) {
    int editValue = *value;
    ImGui::SetCursorScreenPos(pos);
    ImGui::SetNextItemWidth(size.x);
    PushValueInputBadgeStyle();
    const bool submitted = ImGui::InputInt(
        id,
        &editValue,
        0,
        0,
        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    const bool finishedEdit = ImGui::IsItemDeactivatedAfterEdit();
    PopValueInputBadgeStyle();

    if (!submitted && !finishedEdit) {
        return false;
    }

    const int clampedValue = std::clamp(editValue, minValue, maxValue);
    if (clampedValue == *value) {
        return false;
    }

    *value = clampedValue;
    return true;
}

void RenderFpsSettings() {
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
    ImGui::TextUnformatted("FPS");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("FPS", "GUI", tane::gui::IsFpsEnabled(), &tane::gui::SetFpsEnabled, SettingsPanel::None);
    RenderSettingsRow("Black Background", tane::gui::IsFpsBackgroundEnabled(), &tane::gui::SetFpsBackgroundEnabled);

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

    float updateInterval = tane::gui::GetFpsUpdateInterval();
    char valueText[32] = {};
    std::snprintf(valueText, sizeof(valueText), "%.2fs", updateInterval);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 15.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Update Time");

    const ImVec2 valueBadgeSize = GetValueInputBadgeSize(valueText);
    const ImVec2 valueBadgePos(sliderPos.x + sliderSize.x - valueBadgeSize.x - 18.0f, sliderPos.y + 12.0f);
    if (DrawFloatValueInputBadge(
            "##fpsUpdateTimeInput",
            &updateInterval,
            tane::gui::GetFpsMinUpdateInterval(),
            tane::gui::GetFpsMaxUpdateInterval(),
            "%.2f",
            valueBadgePos,
            valueBadgeSize)) {
        tane::gui::SetFpsUpdateInterval(updateInterval);
    }

    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 53.0f));
    if (DrawModernSliderFloat(
            "##fpsUpdateTime",
            &updateInterval,
            tane::gui::GetFpsMinUpdateInterval(),
            tane::gui::GetFpsMaxUpdateInterval(),
            ImVec2(sliderSize.x - 36.0f, 30.0f))) {
        tane::gui::SetFpsUpdateInterval(updateInterval);
    }
    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x, sliderPos.y + sliderSize.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));
}

void RenderPingSettings() {
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
    ImGui::TextUnformatted("Ping");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Ping", "GUI", tane::gui::IsPingEnabled(), &tane::gui::SetPingEnabled, SettingsPanel::None);
    RenderSettingsRow("Black Background", tane::gui::IsPingBackgroundEnabled(), &tane::gui::SetPingBackgroundEnabled);
}

void RenderArrowCounterSettings() {
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
    ImGui::TextUnformatted("Arrow Counter");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Arrow Counter", "GUI", tane::gui::IsArrowCounterEnabled(), &tane::gui::SetArrowCounterEnabled, SettingsPanel::None);
    RenderSettingsRow("Black Background", tane::gui::IsArrowCounterBackgroundEnabled(), &tane::gui::SetArrowCounterBackgroundEnabled);
}

void RenderPotCounterSettings() {
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
    ImGui::TextUnformatted("Pot Counter");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Pot Counter", "GUI", tane::gui::IsPotCounterEnabled(), &tane::gui::SetPotCounterEnabled, SettingsPanel::None);
    RenderSettingsRow("Black Background", tane::gui::IsPotCounterBackgroundEnabled(), &tane::gui::SetPotCounterBackgroundEnabled);
}

bool DrawCpsModeButton(const char* id, const char* label, bool selected, const ImVec2& size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    drawList->AddRectFilled(
        pos,
        max,
        ApplyStyleAlpha(selected ? IM_COL32(238, 238, 238, 232) : (hovered ? IM_COL32(48, 48, 48, 226) : IM_COL32(24, 24, 24, 218))),
        10.0f);
    drawList->AddRect(
        ImVec2(pos.x + 0.75f, pos.y + 0.75f),
        ImVec2(max.x - 0.75f, max.y - 0.75f),
        ApplyStyleAlpha(selected ? IM_COL32(255, 255, 255, 142) : IM_COL32(255, 255, 255, 64)),
        9.25f,
        0,
        1.1f);

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
        ApplyStyleAlpha(selected ? IM_COL32(12, 12, 12, 244) : IM_COL32(242, 242, 242, 230)),
        label);
    return clicked;
}
