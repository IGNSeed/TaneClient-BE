#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace tane::payload {
ImFont* GetItemHudFont();
}

namespace tane::gui {
bool IsGuiPositionEditorActive();
bool ShouldShowGuiOverlay();
void DrawGuiTextWithShadow(
    ImDrawList* drawList,
    ImFont* font,
    float fontSize,
    const ImVec2& pos,
    ImU32 color,
    const char* text,
    float shadowOffset,
    ImU32 shadowColor);
}

namespace tane::gui {
namespace {

constexpr float kMinKeyStrokeScale = 0.55f;
constexpr float kMaxKeyStrokeScale = 3.0f;
constexpr float kDefaultKeyStrokeX = 8.0f;
constexpr float kDefaultKeyStrokeY = 180.0f;
constexpr float kDefaultKeyStrokeScale = 1.35f;

constexpr float kKeySize = 42.0f;
constexpr float kKeyGap = 7.0f;
constexpr float kMouseButtonHeight = 36.0f;
constexpr float kJumpBarHeight = 20.0f;

std::atomic_bool g_keyStrokeEnabled = false;
std::atomic_bool g_keyStrokeConfigLoaded = false;
std::atomic_bool g_keyStrokeCustomPosition = false;
std::atomic_bool g_keyStrokePositionDirty = false;
std::atomic<float> g_keyStrokePositionX = kDefaultKeyStrokeX;
std::atomic<float> g_keyStrokePositionY = kDefaultKeyStrokeY;
std::atomic<float> g_keyStrokeScale = kDefaultKeyStrokeScale;

bool BuildKeyStrokeConfigPath(wchar_t* path, DWORD pathCount) {
    if (path == nullptr || pathCount == 0) {
        return false;
    }

    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    if (tempLength == 0 || tempLength >= MAX_PATH) {
        return false;
    }

    wchar_t basePath[MAX_PATH]{};
    if (swprintf_s(basePath, L"%sTaneClient", tempPath) < 0) {
        return false;
    }
    CreateDirectoryW(basePath, nullptr);

    wchar_t configPath[MAX_PATH]{};
    if (swprintf_s(configPath, L"%s\\Config", basePath) < 0) {
        return false;
    }
    CreateDirectoryW(configPath, nullptr);

    wchar_t guiPath[MAX_PATH]{};
    if (swprintf_s(guiPath, L"%s\\GUI", configPath) < 0) {
        return false;
    }
    CreateDirectoryW(guiPath, nullptr);

    return swprintf_s(path, pathCount, L"%s\\KeyStroke.json", guiPath) >= 0;
}

bool ParseFloatAfter(const char* section, const char* key, float& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    return std::sscanf(found + 1, " %f", &value) == 1;
}

bool ParseBoolAfter(const char* section, const char* key, bool& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    while (*++found == ' ' || *found == '\t') {
    }

    if (std::strncmp(found, "true", 4) == 0) {
        value = true;
        return true;
    }
    if (std::strncmp(found, "false", 5) == 0) {
        value = false;
        return true;
    }

    return false;
}

void SaveKeyStrokeConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildKeyStrokeConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[384]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 1,\n"
        "  \"enabled\": %s,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        g_keyStrokeEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_keyStrokeCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_keyStrokePositionX.load(std::memory_order_relaxed),
        g_keyStrokePositionY.load(std::memory_order_relaxed),
        g_keyStrokeScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_keyStrokePositionDirty.store(false, std::memory_order_relaxed);
}

void EnsureKeyStrokeConfigLoaded() {
    bool expected = false;
    if (!g_keyStrokeConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildKeyStrokeConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SaveKeyStrokeConfig();
        return;
    }

    bool shouldRewriteConfig = false;
    char json[1024]{};
    DWORD read = 0;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';
        bool enabled = false;
        bool custom = false;
        float x = kDefaultKeyStrokeX;
        float y = kDefaultKeyStrokeY;
        float scale = kDefaultKeyStrokeScale;

        if (!ParseBoolAfter(json, "\"enabled\"", enabled)) {
            shouldRewriteConfig = true;
        }
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);

        g_keyStrokeEnabled.store(enabled, std::memory_order_relaxed);
        g_keyStrokeCustomPosition.store(custom, std::memory_order_relaxed);
        g_keyStrokePositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_keyStrokePositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_keyStrokeScale.store(std::clamp(scale, kMinKeyStrokeScale, kMaxKeyStrokeScale), std::memory_order_relaxed);
    }
    CloseHandle(file);
    if (shouldRewriteConfig) {
        SaveKeyStrokeConfig();
    }
}

float GetClampedKeyStrokeScale() {
    return std::clamp(g_keyStrokeScale.load(std::memory_order_relaxed), kMinKeyStrokeScale, kMaxKeyStrokeScale);
}

ImVec2 GetKeyStrokeRectSize(float scale) {
    const float width = kKeySize * 3.0f + kKeyGap * 2.0f;
    const float height = kKeySize * 2.0f + kMouseButtonHeight + kJumpBarHeight + kKeyGap * 3.0f;
    return ImVec2(width * scale, height * scale);
}

ImVec2 ClampKeyStrokePosition(const ImVec2& position, const ImVec2& displaySize) {
    const ImVec2 rectSize = GetKeyStrokeRectSize(GetClampedKeyStrokeScale());
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, displaySize.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, displaySize.y - rectSize.y)));
}

ImVec2 GetEffectiveKeyStrokePosition(const ImVec2& displaySize) {
    EnsureKeyStrokeConfigLoaded();
    if (!g_keyStrokeCustomPosition.load(std::memory_order_relaxed)) {
        return ClampKeyStrokePosition(ImVec2(kDefaultKeyStrokeX, kDefaultKeyStrokeY), displaySize);
    }

    return ClampKeyStrokePosition(
        ImVec2(
            g_keyStrokePositionX.load(std::memory_order_relaxed),
            g_keyStrokePositionY.load(std::memory_order_relaxed)),
        displaySize);
}

bool IsVirtualKeyDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool IsDisplayKeyDown(int virtualKey, ImGuiKey imguiKey) {
    return IsVirtualKeyDown(virtualKey) || ImGui::IsKeyDown(imguiKey);
}

bool IsDisplayMouseDown(int virtualKey, ImGuiMouseButton imguiButton) {
    return IsVirtualKeyDown(virtualKey) || ImGui::IsMouseDown(imguiButton);
}

float SmoothStep(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float UpdatePressAnimation(float& animation, bool active, bool enabled) {
    if (!enabled) {
        animation = 0.0f;
        return 0.0f;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float deltaTime = std::clamp(io.DeltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
    const float target = active ? 1.0f : 0.0f;
    const float speed = active ? 18.0f : 9.0f;
    animation += (target - animation) * std::clamp(deltaTime * speed, 0.0f, 1.0f);
    if (animation < 0.002f) {
        animation = 0.0f;
    }
    return SmoothStep(animation);
}

ImFont* GetGuiTextFont() {
    ImFont* font = tane::payload::GetItemHudFont();
    return font != nullptr ? font : ImGui::GetFont();
}

ImVec2 CalcGuiTextSize(const char* text, float fontSize) {
    ImFont* font = GetGuiTextFont();
    if (font != nullptr) {
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    }

    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const float baseFontSize = ImGui::GetFontSize();
    const float scale = baseFontSize > 0.0f ? fontSize / baseFontSize : 1.0f;
    return ImVec2(textSize.x * scale, textSize.y * scale);
}

void DrawKeyBox(
    ImDrawList* drawList,
    const ImVec2& min,
    const ImVec2& size,
    const char* label,
    bool active,
    float activeAmount,
    bool centerHighlight,
    float scale) {
    if (drawList == nullptr || label == nullptr) {
        return;
    }

    const ImVec2 max(min.x + size.x, min.y + size.y);
    const float rounding = 7.0f * scale;
    const float borderAmount = active ? activeAmount : activeAmount * 0.35f;
    const float borderThickness = std::max(1.15f, (1.35f + borderAmount * 0.6f) * scale);
    const ImU32 fill = IM_COL32(0, 0, 0, active ? 204 : 184);
    const ImU32 border = IM_COL32(244, 247, 250, static_cast<int>(150.0f + borderAmount * 46.0f));
    drawList->AddRectFilled(min, max, fill, rounding);

    if (centerHighlight && active && activeAmount > 0.0f) {
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        const float expand = 0.10f + activeAmount * 0.90f;
        const ImVec2 half(size.x * 0.5f * expand, size.y * 0.5f * expand);
        const ImVec2 pulseMin(center.x - half.x, center.y - half.y);
        const ImVec2 pulseMax(center.x + half.x, center.y + half.y);
        const int pulseAlpha = static_cast<int>(20.0f + activeAmount * 42.0f);
        drawList->AddRectFilled(pulseMin, pulseMax, IM_COL32(210, 224, 238, pulseAlpha), rounding * expand);

        const float shineExpand = std::min(1.0f, expand + 0.14f);
        const ImVec2 shineHalf(size.x * 0.5f * shineExpand, size.y * 0.5f * shineExpand);
        const ImVec2 shineMin(center.x - shineHalf.x, center.y - shineHalf.y);
        const ImVec2 shineMax(center.x + shineHalf.x, center.y + shineHalf.y);
        drawList->AddRect(
            shineMin,
            shineMax,
            IM_COL32(226, 236, 246, static_cast<int>(48.0f + activeAmount * 42.0f)),
            rounding * shineExpand,
            0,
            std::max(1.0f, 0.85f * scale));
    } else if (active && activeAmount > 0.0f) {
        drawList->AddRectFilled(
            min,
            max,
            IM_COL32(255, 255, 255, static_cast<int>(34.0f + activeAmount * 44.0f)),
            rounding);
    }

    drawList->AddRect(min, max, border, rounding, 0, borderThickness);

    ImFont* font = GetGuiTextFont();
    const float fontSize = ImGui::GetFontSize() * scale * (1.04f + activeAmount * 0.05f);
    const ImVec2 textSize = CalcGuiTextSize(label, fontSize);
    const ImVec2 textPos(
        min.x + (size.x - textSize.x) * 0.5f,
        min.y + (size.y - textSize.y) * 0.5f);
    DrawGuiTextWithShadow(
        drawList,
        font,
        fontSize,
        textPos,
        active ? IM_COL32(250, 253, 255, 250) : IM_COL32(236, 240, 244, 230),
        label,
        scale,
        IM_COL32(0, 0, 0, 178));
}

void DrawKeyStrokeAt(ImDrawList* drawList, const ImVec2& position, float scale, bool useLiveInput) {
    if (drawList == nullptr) {
        return;
    }

    const float key = kKeySize * scale;
    const float gap = kKeyGap * scale;
    const float mouseHeight = kMouseButtonHeight * scale;
    const float jumpHeight = kJumpBarHeight * scale;
    const float width = key * 3.0f + gap * 2.0f;
    const float mouseWidth = (width - gap) * 0.5f;

    const bool wDown = useLiveInput && IsDisplayKeyDown('W', ImGuiKey_W);
    const bool aDown = useLiveInput && IsDisplayKeyDown('A', ImGuiKey_A);
    const bool sDown = useLiveInput && IsDisplayKeyDown('S', ImGuiKey_S);
    const bool dDown = useLiveInput && IsDisplayKeyDown('D', ImGuiKey_D);
    const bool lmbDown = useLiveInput && IsDisplayMouseDown(VK_LBUTTON, ImGuiMouseButton_Left);
    const bool rmbDown = useLiveInput && IsDisplayMouseDown(VK_RBUTTON, ImGuiMouseButton_Right);
    const bool jumpDown = useLiveInput && IsDisplayKeyDown(VK_SPACE, ImGuiKey_Space);

    static float wAnimation = 0.0f;
    static float aAnimation = 0.0f;
    static float sAnimation = 0.0f;
    static float dAnimation = 0.0f;
    static float lmbAnimation = 0.0f;
    static float rmbAnimation = 0.0f;
    static float jumpAnimation = 0.0f;

    const float wAmount = UpdatePressAnimation(wAnimation, wDown, useLiveInput);
    const float aAmount = UpdatePressAnimation(aAnimation, aDown, useLiveInput);
    const float sAmount = UpdatePressAnimation(sAnimation, sDown, useLiveInput);
    const float dAmount = UpdatePressAnimation(dAnimation, dDown, useLiveInput);
    const float lmbAmount = UpdatePressAnimation(lmbAnimation, lmbDown, useLiveInput);
    const float rmbAmount = UpdatePressAnimation(rmbAnimation, rmbDown, useLiveInput);
    const float jumpAmount = UpdatePressAnimation(jumpAnimation, jumpDown, useLiveInput);

    DrawKeyBox(drawList, ImVec2(position.x + key + gap, position.y), ImVec2(key, key), "W", wDown, wAmount, true, scale);
    DrawKeyBox(drawList, ImVec2(position.x, position.y + key + gap), ImVec2(key, key), "A", aDown, aAmount, true, scale);
    DrawKeyBox(drawList, ImVec2(position.x + key + gap, position.y + key + gap), ImVec2(key, key), "S", sDown, sAmount, true, scale);
    DrawKeyBox(drawList, ImVec2(position.x + (key + gap) * 2.0f, position.y + key + gap), ImVec2(key, key), "D", dDown, dAmount, true, scale);

    const float mouseY = position.y + (key + gap) * 2.0f;
    DrawKeyBox(drawList, ImVec2(position.x, mouseY), ImVec2(mouseWidth, mouseHeight), "LMB", lmbDown, lmbAmount, false, scale);
    DrawKeyBox(drawList, ImVec2(position.x + mouseWidth + gap, mouseY), ImVec2(mouseWidth, mouseHeight), "RMB", rmbDown, rmbAmount, false, scale);

    const float jumpY = mouseY + mouseHeight + gap;
    DrawKeyBox(drawList, ImVec2(position.x, jumpY), ImVec2(width, jumpHeight), "", jumpDown, jumpAmount, true, scale);
}

} // namespace

bool IsKeyStrokeEnabled() {
    EnsureKeyStrokeConfigLoaded();
    return g_keyStrokeEnabled.load(std::memory_order_relaxed);
}

void SetKeyStrokeEnabled(bool enabled) {
    EnsureKeyStrokeConfigLoaded();
    g_keyStrokeEnabled.store(enabled, std::memory_order_relaxed);
    SaveKeyStrokeConfig();
}

bool GetKeyStrokeEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsureKeyStrokeConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 displaySize(displayWidth, displayHeight);
    const float scale = GetClampedKeyStrokeScale();
    const ImVec2 position = GetEffectiveKeyStrokePosition(displaySize);
    const ImVec2 size = GetKeyStrokeRectSize(scale);

    x = position.x;
    y = position.y;
    width = size.x;
    height = size.y;
    return true;
}

void SetKeyStrokeEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsureKeyStrokeConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const ImVec2 clamped = ClampKeyStrokePosition(ImVec2(displayX, displayY), ImVec2(displayWidth, displayHeight));
    g_keyStrokeCustomPosition.store(true, std::memory_order_relaxed);
    g_keyStrokePositionX.store(clamped.x, std::memory_order_relaxed);
    g_keyStrokePositionY.store(clamped.y, std::memory_order_relaxed);
    g_keyStrokePositionDirty.store(true, std::memory_order_relaxed);
}

void MoveKeyStrokeEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetKeyStrokeEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetKeyStrokeEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetKeyStrokeEditorScale() {
    EnsureKeyStrokeConfigLoaded();
    return GetClampedKeyStrokeScale();
}

void SetKeyStrokeEditorScale(float scale) {
    EnsureKeyStrokeConfigLoaded();
    g_keyStrokeScale.store(std::clamp(scale, kMinKeyStrokeScale, kMaxKeyStrokeScale), std::memory_order_relaxed);
    g_keyStrokePositionDirty.store(true, std::memory_order_relaxed);
}

void ResetKeyStrokeEditorPosition() {
    EnsureKeyStrokeConfigLoaded();
    g_keyStrokeCustomPosition.store(false, std::memory_order_relaxed);
    g_keyStrokePositionX.store(kDefaultKeyStrokeX, std::memory_order_relaxed);
    g_keyStrokePositionY.store(kDefaultKeyStrokeY, std::memory_order_relaxed);
    g_keyStrokeScale.store(kDefaultKeyStrokeScale, std::memory_order_relaxed);
    g_keyStrokePositionDirty.store(true, std::memory_order_relaxed);
    SaveKeyStrokeConfig();
}

void CommitKeyStrokeEditorPosition() {
    EnsureKeyStrokeConfigLoaded();
    if (g_keyStrokePositionDirty.load(std::memory_order_relaxed)) {
        SaveKeyStrokeConfig();
    }
}

void RenderKeyStrokePreview(ImDrawList* drawList, const ImVec2& min, float scale) {
    DrawKeyStrokeAt(drawList, min, scale, false);
}

void RenderKeyStrokeOverlay() {
    EnsureKeyStrokeConfigLoaded();
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (positionEditorActive ||
        !g_keyStrokeEnabled.load(std::memory_order_relaxed) ||
        !ShouldShowGuiOverlay()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float scale = GetClampedKeyStrokeScale();
    const ImVec2 position = GetEffectiveKeyStrokePosition(io.DisplaySize);
    DrawKeyStrokeAt(ImGui::GetForegroundDrawList(), position, scale, true);
}

} // namespace tane::gui
