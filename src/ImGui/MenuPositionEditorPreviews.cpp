void DrawItemHudEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    if (drawList == nullptr) {
        return;
    }

    constexpr int kPreviewSlotCount = 5;
    constexpr float kNativeItemSpacingRatio = 20.0f / 16.0f;
    const bool horizontal = tane::gui::GetItemHudLayout() == 1;
    const float totalWidth = std::max(1.0f, visualMax.x - visualMin.x);
    const float totalHeight = std::max(1.0f, visualMax.y - visualMin.y);
    const float itemSize = horizontal
        ? std::min(totalHeight, totalWidth / (1.0f + static_cast<float>(kPreviewSlotCount - 1) * kNativeItemSpacingRatio))
        : std::min(totalWidth, totalHeight / (1.0f + static_cast<float>(kPreviewSlotCount - 1) * kNativeItemSpacingRatio));
    const float spacing = itemSize * kNativeItemSpacingRatio;

    for (int slot = 0; slot < kPreviewSlotCount; ++slot) {
        const ImTextureRef texture = tane::payload::GetItemHudPreviewTexture(slot);
        const ImVec2 textureSize = tane::payload::GetItemHudPreviewTextureSize(slot);
        const ImVec2 slotMin(
            visualMin.x + (horizontal ? static_cast<float>(slot) * spacing : 0.0f),
            visualMin.y + (horizontal ? 0.0f : static_cast<float>(slot) * spacing));
        const ImVec2 slotMax(slotMin.x + itemSize, slotMin.y + itemSize);
        if (slotMin.x >= visualMax.x || slotMin.y >= visualMax.y) {
            break;
        }

        if (HasTexture(texture) && textureSize.x > 0.0f && textureSize.y > 0.0f) {
            drawList->AddImage(
                texture,
                slotMin,
                ImVec2(std::min(slotMax.x, visualMax.x), std::min(slotMax.y, visualMax.y)),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                ApplyStyleAlpha(IM_COL32(255, 255, 255, 255)));
        } else {
            drawList->AddRectFilled(
                slotMin,
                ImVec2(std::min(slotMax.x, visualMax.x), std::min(slotMax.y, visualMax.y)),
                ApplyStyleAlpha(IM_COL32(8, 8, 8, 120)),
                0.0f);
            drawList->AddRect(
                slotMin,
                ImVec2(std::min(slotMax.x, visualMax.x), std::min(slotMax.y, visualMax.y)),
                ApplyStyleAlpha(IM_COL32(255, 255, 255, 56)),
                0.0f);
        }
    }
}

void DrawItemHudDurabilityEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    if (drawList == nullptr) {
        return;
    }

    constexpr int kPreviewSlotCount = 5;
    const bool horizontal = tane::gui::GetItemHudLayout() == 1;
    const float scale = tane::gui::GetItemHudEditorScale();
    const float fontSize = std::clamp(10.5f * scale, 8.0f, 28.0f);
    const char* labels[kPreviewSlotCount] = {
        "1561/1561",
        "1344/1561",
        "1127/1561",
        "910/1561",
        "693/1561",
    };

    ImFont* font = ImGui::GetFont();
    const float slotStep = horizontal
        ? std::max(1.0f, (visualMax.x - visualMin.x - 54.0f * scale) / static_cast<float>(kPreviewSlotCount - 1))
        : std::max(1.0f, (visualMax.y - visualMin.y - 12.0f * scale) / static_cast<float>(kPreviewSlotCount - 1));
    for (int slot = 0; slot < kPreviewSlotCount; ++slot) {
        const ImVec2 textPos(
            visualMin.x + (horizontal ? static_cast<float>(slot) * slotStep : 0.0f),
            visualMin.y + (horizontal ? 0.0f : static_cast<float>(slot) * slotStep));
        const ImU32 color = slot < 2
            ? IM_COL32(88, 255, 124, 238)
            : (slot < 4 ? IM_COL32(255, 218, 74, 238) : IM_COL32(255, 96, 76, 238));
        drawList->AddText(font, fontSize, ImVec2(textPos.x + scale, textPos.y + scale), ApplyStyleAlpha(IM_COL32(0, 0, 0, 150)), labels[slot]);
        drawList->AddText(font, fontSize, textPos, ApplyStyleAlpha(color), labels[slot]);
    }
}

void DrawFpsEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    if (drawList == nullptr) {
        return;
    }

    const float scale = tane::gui::GetFpsEditorScale();
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    const char* text = "FPS 120";
    if (tane::gui::IsFpsBackgroundEnabled()) {
        drawList->AddRectFilled(visualMin, visualMax, ApplyStyleAlpha(IM_COL32(0, 0, 0, 142)), 5.0f * scale);
        drawList->AddRect(visualMin, visualMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 44)), 5.0f * scale, 0, 1.0f);
    }

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    const ImVec2 textPos(visualMin.x + paddingX, visualMin.y + paddingY);
    drawList->AddText(font, fontSize, ImVec2(textPos.x + scale, textPos.y + scale), ApplyStyleAlpha(IM_COL32(0, 0, 0, 150)), text);
    drawList->AddText(font, fontSize, textPos, ApplyStyleAlpha(IM_COL32(246, 248, 250, 238)), text);
}

void DrawCpsEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    if (drawList == nullptr) {
        return;
    }

    const float scale = tane::gui::GetCpsEditorScale();
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    const char* text = tane::gui::GetCpsDisplayMode() == 0 ? "12 | 8" : "12 CPS";
    if (tane::gui::IsCpsBackgroundEnabled()) {
        drawList->AddRectFilled(visualMin, visualMax, ApplyStyleAlpha(IM_COL32(0, 0, 0, 142)), 5.0f * scale);
        drawList->AddRect(visualMin, visualMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 44)), 5.0f * scale, 0, 1.0f);
    }

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    const ImVec2 textPos(visualMin.x + paddingX, visualMin.y + paddingY);
    drawList->AddText(font, fontSize, ImVec2(textPos.x + scale, textPos.y + scale), ApplyStyleAlpha(IM_COL32(0, 0, 0, 150)), text);
    drawList->AddText(font, fontSize, textPos, ApplyStyleAlpha(IM_COL32(246, 248, 250, 238)), text);
}

void DrawPingEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    if (drawList == nullptr) {
        return;
    }

    const float scale = tane::gui::GetPingEditorScale();
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    const char* text = "Ping 42ms";
    if (tane::gui::IsPingBackgroundEnabled()) {
        drawList->AddRectFilled(visualMin, visualMax, ApplyStyleAlpha(IM_COL32(0, 0, 0, 142)), 5.0f * scale);
        drawList->AddRect(visualMin, visualMax, ApplyStyleAlpha(IM_COL32(255, 255, 255, 44)), 5.0f * scale, 0, 1.0f);
    }

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    const ImVec2 textPos(visualMin.x + paddingX, visualMin.y + paddingY);
    drawList->AddText(font, fontSize, ImVec2(textPos.x + scale, textPos.y + scale), ApplyStyleAlpha(IM_COL32(0, 0, 0, 150)), text);
    drawList->AddText(font, fontSize, textPos, ApplyStyleAlpha(IM_COL32(246, 248, 250, 238)), text);
}

void DrawArrowCounterEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    (void)visualMax;
    tane::gui::RenderArrowCounterPreview(drawList, visualMin);
}

void DrawPotCounterEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    (void)visualMax;
    tane::gui::RenderPotCounterPreview(drawList, visualMin);
}

void DrawEffectHudEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    (void)visualMax;
    tane::gui::RenderEffectHudPreview(drawList, visualMin);
}

void DrawKeyStrokeEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    (void)visualMax;
    if (drawList == nullptr) {
        return;
    }

    tane::gui::RenderKeyStrokePreview(drawList, visualMin, tane::gui::GetKeyStrokeEditorScale());
}

void DrawControllerOverlayEditorPreview(ImDrawList* drawList, const ImVec2& visualMin, const ImVec2& visualMax) {
    (void)visualMax;
    if (drawList == nullptr) {
        return;
    }

    tane::gui::RenderControllerOverlayPreview(drawList, visualMin, tane::gui::GetControllerOverlayEditorScale());
}
