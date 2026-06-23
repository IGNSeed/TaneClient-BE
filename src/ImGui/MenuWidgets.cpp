#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace tane::imgui_menu {

namespace {

ImU32 ApplyStyleAlpha(ImU32 color) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return color;
    }

    ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
    value.w *= ImGui::GetStyle().Alpha;
    return ImGui::ColorConvertFloat4ToU32(value);
}

ImU32 LerpColor(ImU32 from, ImU32 to, float progress) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    const ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
    const ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(
        a.x + (b.x - a.x) * progress,
        a.y + (b.y - a.y) * progress,
        a.z + (b.z - a.z) * progress,
        a.w + (b.w - a.w) * progress));
}

float UpdateToggleVisualValue(ImGuiID id, bool enabled) {
    const float target = enabled ? 1.0f : 0.0f;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* value = storage != nullptr ? storage->GetFloatRef(id, target) : nullptr;
    if (value == nullptr) {
        return target;
    }

    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    const float blend = 1.0f - std::pow(0.0018f, deltaTime);
    *value += (target - *value) * std::clamp(blend, 0.0f, 1.0f);
    if (std::fabs(*value - target) < 0.001f) {
        *value = target;
    }
    return *value;
}

}  // namespace

void DrawPanel(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 fillColor, float rounding, ImU32 borderColor) {
    const ImVec2 panelMin(std::floor(min.x) + 1.5f, std::floor(min.y) + 1.5f);
    const ImVec2 panelMax(std::floor(max.x) - 1.5f, std::floor(max.y) - 1.5f);
    const float innerRounding = std::max(0.0f, rounding - 1.5f);

    drawList->AddRectFilled(panelMin, panelMax, ApplyStyleAlpha(fillColor), innerRounding);
    drawList->AddRect(panelMin, panelMax, ApplyStyleAlpha(borderColor), innerRounding, 0, 1.35f);
}

void DrawModuleStatusStrip(ImDrawList* drawList, const ImVec2& rowMin, const ImVec2& rowMax, bool enabled) {
    const float value = UpdateToggleVisualValue(ImGui::GetID("##moduleStatusStrip"), enabled);
    const ImVec2 clipMin(std::floor(rowMin.x) + 2.5f, std::floor(rowMin.y) + 2.5f);
    const ImVec2 clipMax(std::floor(rowMax.x) - 2.5f, std::floor(rowMax.y) - 2.5f);
    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
        return;
    }

    constexpr float kStripWidth = 9.0f;
    constexpr float kStripRounding = 8.5f;
    const ImVec2 stripMax(std::min(clipMin.x + kStripWidth, clipMax.x), clipMax.y);
    const ImU32 stripColor = LerpColor(IM_COL32(96, 96, 96, 218), IM_COL32(248, 248, 248, 232), value);
    drawList->PushClipRect(clipMin, clipMax, true);
    drawList->AddRectFilled(
        clipMin,
        stripMax,
        ApplyStyleAlpha(stripColor),
        kStripRounding,
        ImDrawFlags_RoundCornersLeft);
    drawList->PopClipRect();
}

bool DrawToggleButton(const char* id, bool enabled, const ImVec2& size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID widgetId = ImGui::GetID(id);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const float value = UpdateToggleVisualValue(widgetId, enabled);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    const float rounding = size.y * 0.5f;
    const ImU32 offBg = hovered ? IM_COL32(72, 72, 72, 230) : IM_COL32(42, 42, 42, 222);
    const ImU32 onBg = hovered ? IM_COL32(245, 245, 245, 236) : IM_COL32(226, 226, 226, 228);
    const ImU32 bgColor = LerpColor(offBg, onBg, value);
    drawList->AddRectFilled(pos, max, ApplyStyleAlpha(bgColor), rounding);
    drawList->AddRect(
        ImVec2(pos.x + 0.75f, pos.y + 0.75f),
        ImVec2(max.x - 0.75f, max.y - 0.75f),
        ApplyStyleAlpha(LerpColor(IM_COL32(255, 255, 255, 76), IM_COL32(255, 255, 255, 122), value)),
        rounding - 0.75f,
        0,
        1.1f);

    const float knobRadius = (size.y - 8.0f) * 0.5f + (hovered ? 0.6f : 0.0f) + (active ? 0.8f : 0.0f);
    const float leftX = pos.x + size.y * 0.5f;
    const float rightX = pos.x + size.x - size.y * 0.5f;
    const float knobX = leftX + (rightX - leftX) * value;
    const ImVec2 knobCenter(knobX, pos.y + size.y * 0.5f);
    drawList->AddCircleFilled(
        knobCenter,
        knobRadius,
        ApplyStyleAlpha(LerpColor(IM_COL32(236, 236, 236, 248), IM_COL32(16, 16, 16, 248), value)),
        24);
    return clicked;
}

bool DrawGearButton(const char* id, const ImVec2& size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    drawList->AddRectFilled(
        pos,
        max,
        ApplyStyleAlpha(hovered ? IM_COL32(66, 66, 66, 222) : IM_COL32(30, 30, 30, 216)),
        9.0f);
    drawList->AddRect(ImVec2(pos.x + 0.75f, pos.y + 0.75f), ImVec2(max.x - 0.75f, max.y - 0.75f), ApplyStyleAlpha(IM_COL32(255, 255, 255, 82)), 8.25f, 0, 1.1f);

    const ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
    constexpr float kPi = 3.14159265358979323846f;
    const float outerRadius = std::min(size.x, size.y) * 0.24f;
    const float innerRadius = outerRadius * 0.42f;
    const ImU32 iconColor = ApplyStyleAlpha(IM_COL32(246, 246, 246, 235));
    for (int i = 0; i < 8; ++i) {
        const float angle = (static_cast<float>(i) / 8.0f) * kPi * 2.0f;
        const ImVec2 a(center.x + std::cos(angle) * (outerRadius + 1.0f), center.y + std::sin(angle) * (outerRadius + 1.0f));
        const ImVec2 b(center.x + std::cos(angle) * (outerRadius + 4.5f), center.y + std::sin(angle) * (outerRadius + 4.5f));
        drawList->AddLine(a, b, iconColor, 1.8f);
    }
    drawList->AddCircle(center, outerRadius, iconColor, 24, 1.9f);
    drawList->AddCircle(center, innerRadius, iconColor, 16, 1.8f);
    return clicked;
}

}  // namespace tane::imgui_menu
