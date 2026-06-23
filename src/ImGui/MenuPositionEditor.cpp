bool RenderPositionEditor(const ImVec2& displaySize, bool shouldOpen) {
    if (!UpdatePositionEditorAnimation(shouldOpen)) {
        return false;
    }

    const float eased = SmoothStep(g_positionEditorProgress);
    const float frameReveal = SmoothStep(std::clamp((g_positionEditorProgress - 0.12f) / 0.88f, 0.0f, 1.0f));
    const float handleReveal = SmoothStep(std::clamp((g_positionEditorProgress - 0.24f) / 0.76f, 0.0f, 1.0f));
    const float toolbarReveal = SmoothStep(std::clamp((g_positionEditorProgress - 0.18f) / 0.82f, 0.0f, 1.0f));
    const float alpha = SmoothStep(std::clamp((g_positionEditorProgress - 0.03f) / 0.97f, 0.0f, 1.0f));
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;
    if (!shouldOpen || g_positionEditorProgress < 0.72f) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(displaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!ImGui::Begin("##TaneClientPositionEditor", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return true;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    g_positionEditorSnapState = {};
    drawList->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        displaySize,
        IM_COL32(0, 0, 0, static_cast<int>(48.0f * eased)));

    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectW = 0.0f;
    float rectH = 0.0f;
    if (tane::gui::GetItemHudEditorRect(displaySize.x, displaySize.y, rectX, rectY, rectW, rectH)) {
        const ImVec2 rectMin(std::floor(rectX), std::floor(rectY));
        const ImVec2 rectMax(std::floor(rectX + rectW), std::floor(rectY + rectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##ItemHudPositionHandle", ImVec2(std::max(12.0f, rectW), std::max(12.0f, rectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::ItemHud;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::ItemHud, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }
        if (g_positionEditorDraggingLastFrame && !active) {
            tane::gui::CommitItemHudEditorPosition();
        }
        g_positionEditorDraggingLastFrame = active;

        const bool selected = g_positionEditorTarget == PositionEditorTarget::ItemHud;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawItemHudEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex);
            ImGui::InvisibleButton("##ResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::ItemHud;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyItemHudEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "Item HUD");
    }

    float durabilityRectX = 0.0f;
    float durabilityRectY = 0.0f;
    float durabilityRectW = 0.0f;
    float durabilityRectH = 0.0f;
    if (tane::gui::GetItemHudDurabilityEditorRect(displaySize.x, displaySize.y, durabilityRectX, durabilityRectY, durabilityRectW, durabilityRectH)) {
        const ImVec2 rectMin(std::floor(durabilityRectX), std::floor(durabilityRectY));
        const ImVec2 rectMax(std::floor(durabilityRectX + durabilityRectW), std::floor(durabilityRectY + durabilityRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##ItemHudDurabilityPositionHandle", ImVec2(std::max(12.0f, durabilityRectW), std::max(12.0f, durabilityRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::ItemHudDurability;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::ItemHudDurability, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::ItemHudDurability;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawItemHudDurabilityEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "Durability");
    }

    float fpsRectX = 0.0f;
    float fpsRectY = 0.0f;
    float fpsRectW = 0.0f;
    float fpsRectH = 0.0f;
    if (tane::gui::GetFpsEditorRect(displaySize.x, displaySize.y, fpsRectX, fpsRectY, fpsRectW, fpsRectH)) {
        const ImVec2 rectMin(std::floor(fpsRectX), std::floor(fpsRectY));
        const ImVec2 rectMax(std::floor(fpsRectX + fpsRectW), std::floor(fpsRectY + fpsRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##FpsPositionHandle", ImVec2(std::max(12.0f, fpsRectW), std::max(12.0f, fpsRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::Fps;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::Fps, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::Fps;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawFpsEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 100);
            ImGui::InvisibleButton("##FpsResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::Fps;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyFpsEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "FPS");
    }

    float cpsRectX = 0.0f;
    float cpsRectY = 0.0f;
    float cpsRectW = 0.0f;
    float cpsRectH = 0.0f;
    if (tane::gui::GetCpsEditorRect(displaySize.x, displaySize.y, cpsRectX, cpsRectY, cpsRectW, cpsRectH)) {
        const ImVec2 rectMin(std::floor(cpsRectX), std::floor(cpsRectY));
        const ImVec2 rectMax(std::floor(cpsRectX + cpsRectW), std::floor(cpsRectY + cpsRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##CpsPositionHandle", ImVec2(std::max(12.0f, cpsRectW), std::max(12.0f, cpsRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::Cps;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::Cps, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::Cps;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawCpsEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 200);
            ImGui::InvisibleButton("##CpsResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::Cps;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyCpsEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "CPS");
    }

    float pingRectX = 0.0f;
    float pingRectY = 0.0f;
    float pingRectW = 0.0f;
    float pingRectH = 0.0f;
    if (tane::gui::GetPingEditorRect(displaySize.x, displaySize.y, pingRectX, pingRectY, pingRectW, pingRectH)) {
        const ImVec2 rectMin(std::floor(pingRectX), std::floor(pingRectY));
        const ImVec2 rectMax(std::floor(pingRectX + pingRectW), std::floor(pingRectY + pingRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##PingPositionHandle", ImVec2(std::max(12.0f, pingRectW), std::max(12.0f, pingRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::Ping;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::Ping, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::Ping;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawPingEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 250);
            ImGui::InvisibleButton("##PingResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::Ping;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyPingEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "Ping");
    }

    float keyStrokeRectX = 0.0f;
    float keyStrokeRectY = 0.0f;
    float keyStrokeRectW = 0.0f;
    float keyStrokeRectH = 0.0f;
    if (tane::gui::GetKeyStrokeEditorRect(displaySize.x, displaySize.y, keyStrokeRectX, keyStrokeRectY, keyStrokeRectW, keyStrokeRectH)) {
        const ImVec2 rectMin(std::floor(keyStrokeRectX), std::floor(keyStrokeRectY));
        const ImVec2 rectMax(std::floor(keyStrokeRectX + keyStrokeRectW), std::floor(keyStrokeRectY + keyStrokeRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##KeyStrokePositionHandle", ImVec2(std::max(12.0f, keyStrokeRectW), std::max(12.0f, keyStrokeRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::KeyStroke;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::KeyStroke, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::KeyStroke;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawKeyStrokeEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 500);
            ImGui::InvisibleButton("##KeyStrokeResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::KeyStroke;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyKeyStrokeEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "KeyStroke");
    }

    float controllerOverlayRectX = 0.0f;
    float controllerOverlayRectY = 0.0f;
    float controllerOverlayRectW = 0.0f;
    float controllerOverlayRectH = 0.0f;
    if (tane::gui::GetControllerOverlayEditorRect(
            displaySize.x,
            displaySize.y,
            controllerOverlayRectX,
            controllerOverlayRectY,
            controllerOverlayRectW,
            controllerOverlayRectH)) {
        const ImVec2 rectMin(std::floor(controllerOverlayRectX), std::floor(controllerOverlayRectY));
        const ImVec2 rectMax(std::floor(controllerOverlayRectX + controllerOverlayRectW), std::floor(controllerOverlayRectY + controllerOverlayRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton(
            "##ControllerOverlayPositionHandle",
            ImVec2(std::max(12.0f, controllerOverlayRectW), std::max(12.0f, controllerOverlayRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::ControllerOverlay;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::ControllerOverlay, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::ControllerOverlay;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawControllerOverlayEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 600);
            ImGui::InvisibleButton("##ControllerOverlayResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::ControllerOverlay;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyControllerOverlayEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "Controller Overlay");
    }

    float arrowRectX = 0.0f;
    float arrowRectY = 0.0f;
    float arrowRectW = 0.0f;
    float arrowRectH = 0.0f;
    if (tane::gui::GetArrowCounterEditorRect(displaySize.x, displaySize.y, arrowRectX, arrowRectY, arrowRectW, arrowRectH)) {
        const ImVec2 rectMin(std::floor(arrowRectX), std::floor(arrowRectY));
        const ImVec2 rectMax(std::floor(arrowRectX + arrowRectW), std::floor(arrowRectY + arrowRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##ArrowCounterPositionHandle", ImVec2(std::max(12.0f, arrowRectW), std::max(12.0f, arrowRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::ArrowCounter;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::ArrowCounter, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::ArrowCounter;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawArrowCounterEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 300);
            ImGui::InvisibleButton("##ArrowCounterResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::ArrowCounter;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyArrowCounterEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "Arrow Counter");
    }

    float potRectX = 0.0f;
    float potRectY = 0.0f;
    float potRectW = 0.0f;
    float potRectH = 0.0f;
    if (tane::gui::GetPotCounterEditorRect(displaySize.x, displaySize.y, potRectX, potRectY, potRectW, potRectH)) {
        const ImVec2 rectMin(std::floor(potRectX), std::floor(potRectY));
        const ImVec2 rectMax(std::floor(potRectX + potRectW), std::floor(potRectY + potRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##PotCounterPositionHandle", ImVec2(std::max(12.0f, potRectW), std::max(12.0f, potRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::PotCounter;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::PotCounter, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::PotCounter;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawPotCounterEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 400);
            ImGui::InvisibleButton("##PotCounterResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::PotCounter;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyPotCounterEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "Pot Counter");
    }

    float effectRectX = 0.0f;
    float effectRectY = 0.0f;
    float effectRectW = 0.0f;
    float effectRectH = 0.0f;
    if (tane::gui::GetEffectHudEditorRect(displaySize.x, displaySize.y, effectRectX, effectRectY, effectRectW, effectRectH)) {
        const ImVec2 rectMin(std::floor(effectRectX), std::floor(effectRectY));
        const ImVec2 rectMax(std::floor(effectRectX + effectRectW), std::floor(effectRectY + effectRectH));
        const ImVec2 visualMin = rectMin;
        const ImVec2 visualMax = rectMax;

        ImGui::SetCursorScreenPos(rectMin);
        ImGui::InvisibleButton("##EffectHudPositionHandle", ImVec2(std::max(12.0f, effectRectW), std::max(12.0f, effectRectH)));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            g_positionEditorTarget = PositionEditorTarget::EffectHud;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            MovePositionEditorTarget(PositionEditorTarget::EffectHud, io.MouseDelta.x, io.MouseDelta.y, displaySize);
        }

        const bool selected = g_positionEditorTarget == PositionEditorTarget::EffectHud;
        const ImU32 borderColor = hovered || active || selected
            ? IM_COL32(255, 255, 255, 238)
            : IM_COL32(255, 255, 255, 168);
        drawList->AddRectFilled(
            visualMin,
            visualMax,
            ApplyStyleAlpha(IM_COL32(255, 255, 255, hovered || active ? 16 : 7), frameReveal),
            0.0f);
        DrawEffectHudEditorPreview(drawList, visualMin, visualMax);
        drawList->AddRect(
            visualMin,
            visualMax,
            ApplyStyleAlpha(borderColor, frameReveal),
            0.0f,
            0,
            (hovered || active || selected ? 2.4f : 1.7f) * (0.72f + frameReveal * 0.28f));

        constexpr float kHandleSize = 13.0f;
        constexpr float kHandleHitSize = 30.0f;
        const float visibleHandleSize = kHandleSize * (0.62f + handleReveal * 0.38f);
        const float hitHandleSize = kHandleHitSize * (0.84f + handleReveal * 0.16f);
        const ImVec2 handles[] = {
            visualMin,
            ImVec2(visualMax.x, visualMin.y),
            ImVec2(visualMin.x, visualMax.y),
            visualMax,
        };
        for (int handleIndex = 0; handleIndex < 4; ++handleIndex) {
            const ImVec2 center = handles[handleIndex];
            const ImVec2 handleMin(center.x - visibleHandleSize * 0.5f, center.y - visibleHandleSize * 0.5f);
            const ImVec2 handleMax(center.x + visibleHandleSize * 0.5f, center.y + visibleHandleSize * 0.5f);
            const ImVec2 hitMin(center.x - hitHandleSize * 0.5f, center.y - hitHandleSize * 0.5f);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::PushID(handleIndex + 450);
            ImGui::InvisibleButton("##EffectHudResizeHandle", ImVec2(hitHandleSize, hitHandleSize));
            const bool handleHovered = ImGui::IsItemHovered();
            const bool handleActive = ImGui::IsItemActive();
            if (handleHovered || handleActive) {
                g_positionEditorTarget = PositionEditorTarget::EffectHud;
            }
            if (handleActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ApplyEffectHudEditorResize(handleIndex, rectMin, rectMax, io.MousePos, displaySize);
            }
            ImGui::PopID();
            if (handleHovered || handleActive) {
                drawList->AddCircleFilled(center, visibleHandleSize * 0.86f, ApplyStyleAlpha(IM_COL32(255, 255, 255, handleActive ? 34 : 22), handleReveal), 20);
            }
            drawList->AddRectFilled(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(255, 255, 255, 238) : IM_COL32(18, 18, 18, 236), handleReveal),
                3.0f);
            drawList->AddRect(
                handleMin,
                handleMax,
                ApplyStyleAlpha(handleHovered || handleActive ? IM_COL32(0, 0, 0, 220) : borderColor, handleReveal),
                3.0f,
                0,
                1.2f);
        }
        drawList->AddText(
            ImVec2(visualMin.x + 10.0f, visualMin.y - 24.0f + (1.0f - frameReveal) * 6.0f),
            ApplyStyleAlpha(IM_COL32(255, 255, 255, 224), frameReveal),
            "Effect HUD");
    }

    float keyboardMoveX = 0.0f;
    float keyboardMoveY = 0.0f;
    const float keyboardSpeed = (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) ? 260.0f : 96.0f;
    const float keyboardStep = keyboardSpeed * std::clamp(io.DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) keyboardMoveX -= keyboardStep;
    if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) keyboardMoveX += keyboardStep;
    if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) keyboardMoveY -= keyboardStep;
    if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) keyboardMoveY += keyboardStep;
    if (keyboardMoveX != 0.0f || keyboardMoveY != 0.0f) {
        MovePositionEditorTarget(g_positionEditorTarget, keyboardMoveX, keyboardMoveY, displaySize);
    }

    float controllerMoveX = 0.0f;
    float controllerMoveY = 0.0f;
    float controllerResize = 0.0f;
    bool controllerAccept = false;
    bool controllerCancel = false;
    if (tane::gui::ReadPositionEditorControllerInput(controllerMoveX, controllerMoveY, controllerResize, controllerAccept, controllerCancel)) {
        const float controllerDeltaTime = std::clamp(io.DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
        const float moveMagnitude = std::clamp(std::sqrt(controllerMoveX * controllerMoveX + controllerMoveY * controllerMoveY), 0.0f, 6.5f);
        const float controllerStep = (110.0f + moveMagnitude * moveMagnitude * 18.0f) * controllerDeltaTime;
        MovePositionEditorTarget(g_positionEditorTarget, controllerMoveX * controllerStep, controllerMoveY * controllerStep, displaySize);
        if (controllerResize != 0.0f) {
            ResizePositionEditorTarget(g_positionEditorTarget, controllerResize * controllerDeltaTime * 0.82f);
        }
        (void)controllerAccept;
        (void)controllerCancel;
    }
    DrawPositionEditorSnapGuides(drawList, displaySize, frameReveal);

    const ImVec2 barSize(356.0f, 88.0f);
    const ImVec2 barPos(
        std::floor((displaySize.x - barSize.x) * 0.5f),
        std::floor(displaySize.y - barSize.y - 22.0f + (1.0f - toolbarReveal) * 42.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * toolbarReveal);
    DrawPanel(
        drawList,
        barPos,
        ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
        IM_COL32(0, 0, 0, 188),
        14.0f,
        IM_COL32(255, 255, 255, 112));

    const char* selectedName = GetPositionEditorTargetName();
    const char* prefix = "Selected";
    const ImVec2 prefixSize = ImGui::CalcTextSize(prefix);
    const ImVec2 selectedSize = ImGui::CalcTextSize(selectedName);
    const ImVec2 badgeMin(barPos.x + 14.0f, barPos.y + 10.0f);
    const ImVec2 badgeMax(barPos.x + barSize.x - 14.0f, barPos.y + 38.0f);
    drawList->AddRectFilled(
        badgeMin,
        badgeMax,
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 30)),
        9.0f);
    drawList->AddRect(
        ImVec2(badgeMin.x + 0.75f, badgeMin.y + 0.75f),
        ImVec2(badgeMax.x - 0.75f, badgeMax.y - 0.75f),
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 104)),
        8.25f,
        0,
        1.1f);
    drawList->AddCircleFilled(
        ImVec2(badgeMin.x + 15.0f, badgeMin.y + 14.0f),
        4.0f,
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 238)));
    drawList->AddText(
        ImVec2(badgeMin.x + 27.0f, badgeMin.y + (28.0f - prefixSize.y) * 0.5f),
        ApplyStyleAlpha(IM_COL32(166, 166, 166, 224)),
        prefix);
    drawList->AddText(
        ImVec2(badgeMax.x - selectedSize.x - 13.0f, badgeMin.y + (28.0f - selectedSize.y) * 0.5f),
        ApplyStyleAlpha(IM_COL32(255, 255, 255, 244)),
        selectedName);

    ImGui::SetCursorScreenPos(ImVec2(barPos.x + 14.0f, barPos.y + 48.0f));
    if (DrawEditorButton("Done", ImVec2(160.0f, 32.0f))) {
        tane::gui::CommitItemHudEditorPosition();
        tane::gui::CommitFpsEditorPosition();
        tane::gui::CommitCpsEditorPosition();
        tane::gui::CommitPingEditorPosition();
        tane::gui::CommitArrowCounterEditorPosition();
        tane::gui::CommitPotCounterEditorPosition();
        tane::gui::CommitEffectHudEditorPosition();
        tane::gui::CommitKeyStrokeEditorPosition();
        tane::gui::CommitControllerOverlayEditorPosition();
        ClosePositionEditor();
    }
    ImGui::SameLine(0.0f, 12.0f);
    if (DrawEditorButton("Reset", ImVec2(160.0f, 32.0f))) {
        if (g_positionEditorTarget == PositionEditorTarget::ItemHudDurability) {
            tane::gui::ResetItemHudDurabilityEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::Fps) {
            tane::gui::ResetFpsEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::Cps) {
            tane::gui::ResetCpsEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::Ping) {
            tane::gui::ResetPingEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::ArrowCounter) {
            tane::gui::ResetArrowCounterEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::PotCounter) {
            tane::gui::ResetPotCounterEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::EffectHud) {
            tane::gui::ResetEffectHudEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::KeyStroke) {
            tane::gui::ResetKeyStrokeEditorPosition();
        } else if (g_positionEditorTarget == PositionEditorTarget::ControllerOverlay) {
            tane::gui::ResetControllerOverlayEditorPosition();
        } else {
            tane::gui::ResetItemHudEditorPosition();
        }
    }
    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleVar(2);
    return true;
}

}  // namespace
