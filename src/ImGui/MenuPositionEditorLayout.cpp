bool UpdatePositionEditorAnimation(bool shouldOpen) {
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    constexpr float kOpenDurationSeconds = 0.36f;
    constexpr float kCloseDurationSeconds = 0.24f;

    if (shouldOpen) {
        g_positionEditorProgress = std::min(1.0f, g_positionEditorProgress + deltaTime / kOpenDurationSeconds);
    } else {
        g_positionEditorProgress = std::max(0.0f, g_positionEditorProgress - deltaTime / kCloseDurationSeconds);
    }

    return shouldOpen || g_positionEditorProgress > 0.0f;
}

bool DrawEditorButton(const char* label, const ImVec2& size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawPanel(
        drawList,
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        hovered ? IM_COL32(48, 48, 48, 224) : IM_COL32(12, 12, 12, 204),
        10.0f,
        hovered ? IM_COL32(255, 255, 255, 176) : IM_COL32(255, 255, 255, 98));

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
        ApplyStyleAlpha(IM_COL32(248, 248, 248, 238)),
        label);
    return clicked;
}

void ApplyItemHudEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetItemHudEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextRatio = std::clamp(
        std::min(horizontalRatio, verticalRatio),
        0.55f / currentScale,
        2.75f / currentScale);
    const float nextScale = std::clamp(currentScale * nextRatio, 0.55f, 2.75f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetItemHudEditorScale(nextScale);
    tane::gui::SetItemHudEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyFpsEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetFpsEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.65f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetFpsEditorScale(nextScale);
    tane::gui::SetFpsEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyCpsEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetCpsEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.65f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetCpsEditorScale(nextScale);
    tane::gui::SetCpsEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyPingEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetPingEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.65f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetPingEditorScale(nextScale);
    tane::gui::SetPingEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyArrowCounterEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetArrowCounterEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.55f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetArrowCounterEditorScale(nextScale);
    tane::gui::SetArrowCounterEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyPotCounterEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetPotCounterEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.55f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetPotCounterEditorScale(nextScale);
    tane::gui::SetPotCounterEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyEffectHudEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetEffectHudEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.55f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetEffectHudEditorScale(nextScale);
    tane::gui::SetEffectHudEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyKeyStrokeEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetKeyStrokeEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.55f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetKeyStrokeEditorScale(nextScale);
    tane::gui::SetKeyStrokeEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

void ApplyControllerOverlayEditorResize(
    int handleIndex,
    const ImVec2& rectMin,
    const ImVec2& rectMax,
    const ImVec2& mousePos,
    const ImVec2& displaySize) {
    const float currentScale = tane::gui::GetControllerOverlayEditorScale();
    const float currentWidth = std::max(1.0f, rectMax.x - rectMin.x);
    const float currentHeight = std::max(1.0f, rectMax.y - rectMin.y);
    const bool left = handleIndex == 0 || handleIndex == 2;
    const bool top = handleIndex == 0 || handleIndex == 1;
    const ImVec2 fixedCorner(left ? rectMax.x : rectMin.x, top ? rectMax.y : rectMin.y);
    const float desiredWidth = left ? fixedCorner.x - mousePos.x : mousePos.x - fixedCorner.x;
    const float desiredHeight = top ? fixedCorner.y - mousePos.y : mousePos.y - fixedCorner.y;
    if (desiredWidth <= 1.0f || desiredHeight <= 1.0f) {
        return;
    }

    const float horizontalRatio = desiredWidth / currentWidth;
    const float verticalRatio = desiredHeight / currentHeight;
    const float nextScale = std::clamp(currentScale * std::min(horizontalRatio, verticalRatio), 0.55f, 3.0f);
    if (std::fabs(nextScale - currentScale) <= 0.0001f) {
        return;
    }

    const float ratio = nextScale / currentScale;
    const float nextWidth = currentWidth * ratio;
    const float nextHeight = currentHeight * ratio;
    ImVec2 nextPos = rectMin;
    if (left) {
        nextPos.x = rectMax.x - nextWidth;
    }
    if (top) {
        nextPos.y = rectMax.y - nextHeight;
    }

    tane::gui::SetControllerOverlayEditorScale(nextScale);
    tane::gui::SetControllerOverlayEditorDisplayPosition(nextPos.x, nextPos.y, displaySize.x, displaySize.y);
}

bool GetPositionEditorTargetRect(
    PositionEditorTarget target,
    const ImVec2& displaySize,
    float& x,
    float& y,
    float& width,
    float& height) {
    if (target == PositionEditorTarget::ItemHudDurability) {
        return tane::gui::GetItemHudDurabilityEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::Fps) {
        return tane::gui::GetFpsEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::Cps) {
        return tane::gui::GetCpsEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::Ping) {
        return tane::gui::GetPingEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::ArrowCounter) {
        return tane::gui::GetArrowCounterEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::PotCounter) {
        return tane::gui::GetPotCounterEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::EffectHud) {
        return tane::gui::GetEffectHudEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::KeyStroke) {
        return tane::gui::GetKeyStrokeEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    if (target == PositionEditorTarget::ControllerOverlay) {
        return tane::gui::GetControllerOverlayEditorRect(displaySize.x, displaySize.y, x, y, width, height);
    }
    return tane::gui::GetItemHudEditorRect(displaySize.x, displaySize.y, x, y, width, height);
}

void SetPositionEditorTargetPosition(PositionEditorTarget target, float x, float y, const ImVec2& displaySize) {
    if (target == PositionEditorTarget::ItemHudDurability) {
        tane::gui::SetItemHudDurabilityEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::Fps) {
        tane::gui::SetFpsEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::Cps) {
        tane::gui::SetCpsEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::Ping) {
        tane::gui::SetPingEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::ArrowCounter) {
        tane::gui::SetArrowCounterEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::PotCounter) {
        tane::gui::SetPotCounterEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::EffectHud) {
        tane::gui::SetEffectHudEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::KeyStroke) {
        tane::gui::SetKeyStrokeEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else if (target == PositionEditorTarget::ControllerOverlay) {
        tane::gui::SetControllerOverlayEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    } else {
        tane::gui::SetItemHudEditorDisplayPosition(x, y, displaySize.x, displaySize.y);
    }
}

float ApplyPositionEditorAxisSnap(float current, float desired, float size, float displayExtent, bool& snapped, float& guideLine) {
    constexpr float kSnapDistance = 8.0f;
    constexpr float kAlreadySnappedDistance = 0.5f;
    const float candidateOffsets[] = {0.0f, size * 0.5f, size};
    const float candidateLines[] = {0.0f, displayExtent * 0.5f, displayExtent};
    float bestDelta = 0.0f;
    float bestDistance = kSnapDistance + 1.0f;
    float bestGuideLine = 0.0f;

    for (int i = 0; i < 3; ++i) {
        const float currentDistance = std::fabs(candidateLines[i] - (current + candidateOffsets[i]));
        if (currentDistance <= kAlreadySnappedDistance && std::fabs(desired - current) > 0.01f) {
            continue;
        }

        const float delta = candidateLines[i] - (desired + candidateOffsets[i]);
        const float distance = std::fabs(delta);
        if (distance < bestDistance) {
            bestDelta = delta;
            bestDistance = distance;
            bestGuideLine = candidateLines[i];
        }
    }

    if (bestDistance <= kSnapDistance) {
        snapped = true;
        guideLine = bestGuideLine;
        return desired + bestDelta;
    }

    snapped = false;
    guideLine = 0.0f;
    return desired;
}

void MovePositionEditorTarget(PositionEditorTarget target, float deltaX, float deltaY, const ImVec2& displaySize) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetPositionEditorTargetRect(target, displaySize, x, y, width, height)) {
        return;
    }

    bool snappedX = false;
    bool snappedY = false;
    float guideX = 0.0f;
    float guideY = 0.0f;
    const float snappedPositionX = ApplyPositionEditorAxisSnap(x, x + deltaX, width, displaySize.x, snappedX, guideX);
    const float snappedPositionY = ApplyPositionEditorAxisSnap(y, y + deltaY, height, displaySize.y, snappedY, guideY);
    SetPositionEditorTargetPosition(target, snappedPositionX, snappedPositionY, displaySize);

    g_positionEditorSnapState.vertical = snappedX;
    g_positionEditorSnapState.horizontal = snappedY;
    g_positionEditorSnapState.x = guideX;
    g_positionEditorSnapState.y = guideY;
}

void DrawPositionEditorSnapGuides(ImDrawList* drawList, const ImVec2& displaySize, float alphaMultiplier) {
    if (drawList == nullptr) {
        return;
    }

    const ImU32 lineColor = ApplyStyleAlpha(IM_COL32(255, 255, 255, 190), alphaMultiplier);
    const ImU32 glowColor = ApplyStyleAlpha(IM_COL32(255, 255, 255, 42), alphaMultiplier);
    if (g_positionEditorSnapState.vertical) {
        const float x = std::clamp(g_positionEditorSnapState.x, 0.0f, std::max(0.0f, displaySize.x - 1.0f));
        drawList->AddLine(ImVec2(x, 0.0f), ImVec2(x, displaySize.y), glowColor, 4.0f);
        drawList->AddLine(ImVec2(x, 0.0f), ImVec2(x, displaySize.y), lineColor, 1.35f);
    }
    if (g_positionEditorSnapState.horizontal) {
        const float y = std::clamp(g_positionEditorSnapState.y, 0.0f, std::max(0.0f, displaySize.y - 1.0f));
        drawList->AddLine(ImVec2(0.0f, y), ImVec2(displaySize.x, y), glowColor, 4.0f);
        drawList->AddLine(ImVec2(0.0f, y), ImVec2(displaySize.x, y), lineColor, 1.35f);
    }
}

void ResizePositionEditorTarget(PositionEditorTarget target, float deltaScale) {
    if (target == PositionEditorTarget::Fps) {
        tane::gui::SetFpsEditorScale(tane::gui::GetFpsEditorScale() + deltaScale);
    } else if (target == PositionEditorTarget::Cps) {
        tane::gui::SetCpsEditorScale(tane::gui::GetCpsEditorScale() + deltaScale);
    } else if (target == PositionEditorTarget::Ping) {
        tane::gui::SetPingEditorScale(tane::gui::GetPingEditorScale() + deltaScale);
    } else if (target == PositionEditorTarget::ArrowCounter) {
        tane::gui::SetArrowCounterEditorScale(tane::gui::GetArrowCounterEditorScale() + deltaScale);
    } else if (target == PositionEditorTarget::PotCounter) {
        tane::gui::SetPotCounterEditorScale(tane::gui::GetPotCounterEditorScale() + deltaScale);
    } else if (target == PositionEditorTarget::EffectHud) {
        tane::gui::SetEffectHudEditorScale(tane::gui::GetEffectHudEditorScale() + deltaScale);
    } else if (target == PositionEditorTarget::KeyStroke) {
        tane::gui::SetKeyStrokeEditorScale(tane::gui::GetKeyStrokeEditorScale() + deltaScale);
    } else if (target == PositionEditorTarget::ControllerOverlay) {
        tane::gui::SetControllerOverlayEditorScale(tane::gui::GetControllerOverlayEditorScale() + deltaScale);
    } else {
        tane::gui::SetItemHudEditorScale(tane::gui::GetItemHudEditorScale() + deltaScale);
    }
}

const char* GetPositionEditorTargetName() {
    if (g_positionEditorTarget == PositionEditorTarget::ItemHudDurability) {
        return "Durability";
    }
    if (g_positionEditorTarget == PositionEditorTarget::Fps) {
        return "FPS";
    }
    if (g_positionEditorTarget == PositionEditorTarget::Cps) {
        return "CPS";
    }
    if (g_positionEditorTarget == PositionEditorTarget::Ping) {
        return "Ping";
    }
    if (g_positionEditorTarget == PositionEditorTarget::ArrowCounter) {
        return "Arrow Counter";
    }
    if (g_positionEditorTarget == PositionEditorTarget::PotCounter) {
        return "Pot Counter";
    }
    if (g_positionEditorTarget == PositionEditorTarget::EffectHud) {
        return "Effect HUD";
    }
    if (g_positionEditorTarget == PositionEditorTarget::KeyStroke) {
        return "KeyStroke";
    }
    if (g_positionEditorTarget == PositionEditorTarget::ControllerOverlay) {
        return "Controller Overlay";
    }
    return "Item HUD";
}
