void RenderZoomSettings() {
    tane::camera::TickZoomComboCapture();

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
    ImGui::TextUnformatted("Zoom");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("Zoom", "Camera", tane::camera::IsZoomEnabled(), &tane::camera::SetZoomEnabled, SettingsPanel::None);
    RenderSettingsRow("Smooth Animation", tane::camera::IsZoomSmoothAnimationEnabled(), &tane::camera::SetZoomSmoothAnimationEnabled);

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
    float zoomAmount = tane::camera::GetZoomAmount();
    char valueText[32] = {};
    std::snprintf(valueText, sizeof(valueText), "%.2fx", zoomAmount);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 15.0f), ApplyStyleAlpha(IM_COL32(240, 242, 245, 238)), "Zoom Amount");

    const ImVec2 valueBadgeSize = GetValueInputBadgeSize(valueText);
    const ImVec2 valueBadgePos(sliderPos.x + sliderSize.x - valueBadgeSize.x - 18.0f, sliderPos.y + 12.0f);
    if (DrawFloatValueInputBadge(
            "##zoomAmountInput",
            &zoomAmount,
            tane::camera::GetZoomMinAmount(),
            tane::camera::GetZoomMaxAmount(),
            "%.2f",
            valueBadgePos,
            valueBadgeSize)) {
        tane::camera::SetZoomAmount(zoomAmount);
    }

    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x + 18.0f, sliderPos.y + 53.0f));
    if (DrawModernSliderFloat(
            "##zoomAmount",
            &zoomAmount,
            tane::camera::GetZoomMinAmount(),
            tane::camera::GetZoomMaxAmount(),
            ImVec2(sliderSize.x - 36.0f, 30.0f))) {
        tane::camera::SetZoomAmount(zoomAmount);
    }
    ImGui::SetCursorScreenPos(ImVec2(sliderPos.x, sliderPos.y + sliderSize.y));
    ImGui::Dummy(ImVec2(1.0f, 14.0f));

    ImGui::TextDisabled("Hold Combo");
    ImGui::Dummy(ImVec2(1.0f, 2.0f));
    DrawKeyComboSettingRow(
        "Keyboard",
        tane::camera::GetZoomKeyboardComboLabel(),
        tane::camera::IsZoomKeyboardComboCaptureActive(),
        "Press combo, release to save",
        &tane::camera::BeginZoomKeyboardComboCapture);
    DrawKeyComboSettingRow(
        "Controller",
        tane::camera::GetZoomControllerComboLabel(),
        tane::camera::IsZoomControllerComboCaptureActive(),
        "Press combo, release to save",
        &tane::camera::BeginZoomControllerComboCapture);
    if (tane::camera::IsZoomKeyboardComboCaptureActive() ||
        tane::camera::IsZoomControllerComboCaptureActive()) {
        if (DrawCompactButton("Cancel Capture", ImVec2(136.0f, 32.0f))) {
            tane::camera::CancelZoomComboCapture();
        }
        ImGui::Dummy(ImVec2(1.0f, 12.0f));
    }
}

void RenderFreeLookSettings() {
    tane::camera::TickFreeLookComboCapture();

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
    ImGui::TextUnformatted("FreeLook");
    if (boldFont != nullptr) {
        ImGui::PopFont();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    RenderModuleRow("FreeLook", "Camera", tane::camera::IsFreeLookEnabled(), &tane::camera::SetFreeLookEnabled, SettingsPanel::None);

    ImGui::TextDisabled("Hold Combo");
    ImGui::Dummy(ImVec2(1.0f, 2.0f));
    DrawKeyComboSettingRow(
        "Keyboard",
        tane::camera::GetFreeLookKeyboardComboLabel(),
        tane::camera::IsFreeLookKeyboardComboCaptureActive(),
        "Press combo, release to save",
        &tane::camera::BeginFreeLookKeyboardComboCapture);
    DrawKeyComboSettingRow(
        "Controller",
        tane::camera::GetFreeLookControllerComboLabel(),
        tane::camera::IsFreeLookControllerComboCaptureActive(),
        "Press combo, release to save",
        &tane::camera::BeginFreeLookControllerComboCapture);
    if (tane::camera::IsFreeLookKeyboardComboCaptureActive() ||
        tane::camera::IsFreeLookControllerComboCaptureActive()) {
        if (DrawCompactButton("Cancel Capture", ImVec2(136.0f, 32.0f))) {
            tane::camera::CancelFreeLookComboCapture();
        }
        ImGui::Dummy(ImVec2(1.0f, 12.0f));
    }
}

bool DrawCompactButton(const char* label, const ImVec2& size, bool emphasized) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    const ImU32 fill = emphasized
        ? (hovered ? IM_COL32(255, 255, 255, 76) : IM_COL32(255, 255, 255, 56))
        : (hovered ? IM_COL32(255, 255, 255, 34) : IM_COL32(10, 10, 10, 144));
    const ImU32 border = emphasized
        ? IM_COL32(255, 255, 255, 132)
        : IM_COL32(255, 255, 255, 70);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(pos, max, ApplyStyleAlpha(fill), 10.0f);
    drawList->AddRect(
        ImVec2(pos.x + 0.75f, pos.y + 0.75f),
        ImVec2(max.x - 0.75f, max.y - 0.75f),
        ApplyStyleAlpha(border),
        9.25f,
        0,
        1.0f);

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
        ApplyStyleAlpha(emphasized ? IM_COL32(255, 255, 255, 248) : IM_COL32(220, 220, 220, 232)),
        label);
    return clicked;
}

void DrawKeyComboSettingRow(
    const char* title,
    const char* value,
    bool capturing,
    const char* captureLabel,
    void (*beginCapture)()) {
    constexpr float kRowHeight = 64.0f;
    constexpr float kButtonHeight = 34.0f;
    constexpr float kPaddingX = 18.0f;

    const float rowWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const float buttonWidth = std::clamp(rowWidth * 0.28f, 92.0f, 112.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 max(pos.x + rowWidth, pos.y + kRowHeight);
    ImGui::PushID(title);
    DrawPanel(
        ImGui::GetWindowDrawList(),
        pos,
        max,
        IM_COL32(8, 8, 8, 174),
        14.0f,
        capturing ? IM_COL32(255, 255, 255, 118) : IM_COL32(255, 255, 255, 58));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float textClipRight = std::max(pos.x + kPaddingX + 48.0f, max.x - buttonWidth - 26.0f);
    drawList->PushClipRect(ImVec2(pos.x + kPaddingX, pos.y), ImVec2(textClipRight, max.y), true);
    drawList->AddText(ImVec2(pos.x + kPaddingX, pos.y + 13.0f), ApplyStyleAlpha(IM_COL32(245, 245, 245, 238)), title);
    drawList->AddText(
        ImVec2(pos.x + kPaddingX, pos.y + 36.0f),
        ApplyStyleAlpha(capturing ? IM_COL32(255, 255, 255, 238) : IM_COL32(165, 165, 165, 214)),
        capturing ? captureLabel : value);
    drawList->PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(max.x - buttonWidth - 16.0f, pos.y + (kRowHeight - kButtonHeight) * 0.5f));
    if (DrawCompactButton(capturing ? "Listening" : "Change", ImVec2(buttonWidth, kButtonHeight), capturing)) {
        beginCapture();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, max.y));
    ImGui::Dummy(ImVec2(1.0f, 10.0f));
    ImGui::PopID();
}
