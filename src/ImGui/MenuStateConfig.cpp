void ToggleMenu() {
    g_menuOpen.store(!g_menuOpen.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void AppendLabel(char* buffer, std::size_t bufferSize, const char* text) {
    if (buffer == nullptr || bufferSize == 0 || text == nullptr || text[0] == '\0') {
        return;
    }

    const std::size_t currentLength = std::strlen(buffer);
    if (currentLength + 1 >= bufferSize) {
        return;
    }

    if (currentLength > 0) {
        std::strncat(buffer, " + ", bufferSize - currentLength - 1);
    }

    const std::size_t updatedLength = std::strlen(buffer);
    std::strncat(buffer, text, bufferSize - updatedLength - 1);
}

const char* GetVirtualKeyName(int virtualKey) {
    switch (virtualKey) {
    case VK_CONTROL:
        return "Ctrl";
    case VK_LCONTROL:
        return "LCtrl";
    case VK_RCONTROL:
        return "RCtrl";
    case VK_SHIFT:
        return "Shift";
    case VK_LSHIFT:
        return "LShift";
    case VK_RSHIFT:
        return "RShift";
    case VK_MENU:
        return "Alt";
    case VK_LMENU:
        return "LAlt";
    case VK_RMENU:
        return "RAlt";
    case VK_INSERT:
        return "Insert";
    case VK_DELETE:
        return "Delete";
    case VK_HOME:
        return "Home";
    case VK_END:
        return "End";
    case VK_PRIOR:
        return "PageUp";
    case VK_NEXT:
        return "PageDown";
    case VK_SPACE:
        return "Space";
    case VK_TAB:
        return "Tab";
    case VK_RETURN:
        return "Enter";
    case VK_ESCAPE:
        return "Escape";
    case VK_BACK:
        return "Backspace";
    case VK_UP:
        return "Up";
    case VK_DOWN:
        return "Down";
    case VK_LEFT:
        return "Left";
    case VK_RIGHT:
        return "Right";
    default:
        break;
    }

    static char name[32] = {};
    if ((virtualKey >= 'A' && virtualKey <= 'Z') || (virtualKey >= '0' && virtualKey <= '9')) {
        name[0] = static_cast<char>(virtualKey);
        name[1] = '\0';
        return name;
    }

    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        std::snprintf(name, sizeof(name), "F%d", virtualKey - VK_F1 + 1);
        return name;
    }

    std::snprintf(name, sizeof(name), "VK 0x%02X", virtualKey & 0xFF);
    return name;
}

int NormalizeVirtualKey(WPARAM virtualKey) {
    const int normalized = static_cast<int>(virtualKey);
    return normalized > 0 && normalized < kVirtualKeyCount ? normalized : 0;
}

int ResolveMessageVirtualKey(WPARAM virtualKey, LPARAM lParam) {
    const UINT scanCode = static_cast<UINT>((lParam >> 16) & 0xFF);
    if (scanCode != 0) {
        UINT extendedScanCode = scanCode;
        if ((lParam & (1u << 24)) != 0) {
            extendedScanCode |= 0xE000;
        }

        const UINT mapped = MapVirtualKeyW(extendedScanCode, MAPVK_VSC_TO_VK_EX);
        switch (mapped) {
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LMENU:
        case VK_RMENU:
            return NormalizeVirtualKey(mapped);
        default:
            break;
        }
    }

    return NormalizeVirtualKey(virtualKey);
}

int GetGenericModifierVirtualKey(int virtualKey) {
    switch (virtualKey) {
    case VK_LCONTROL:
    case VK_RCONTROL:
        return VK_CONTROL;
    case VK_LSHIFT:
    case VK_RSHIFT:
        return VK_SHIFT;
    case VK_LMENU:
    case VK_RMENU:
        return VK_MENU;
    default:
        break;
    }

    return virtualKey;
}

bool IsPriorityKeyboardLabelKey(int virtualKey) {
    switch (virtualKey) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return true;
    default:
        return false;
    }
}

void ClearKeyboardKeySet(bool* keys) {
    if (keys != nullptr) {
        std::memset(keys, 0, sizeof(bool) * kVirtualKeyCount);
    }
}

bool HasKeyboardKeySet(const bool* keys) {
    if (keys == nullptr) {
        return false;
    }

    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (keys[virtualKey]) {
            return true;
        }
    }
    return false;
}

void SetDefaultKeyboardCombo() {
    ClearKeyboardKeySet(g_keyboardComboKeys);
    g_keyboardComboKeys[VK_INSERT] = true;
}

void CopyKeyboardKeySet(bool* destination, const bool* source) {
    if (destination != nullptr && source != nullptr) {
        std::memcpy(destination, source, sizeof(bool) * kVirtualKeyCount);
    }
}

void UpdateKeyboardComboLabel() {
    g_keyboardComboLabel[0] = '\0';

    constexpr int kPriorityKeys[] = {
        VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
        VK_SHIFT, VK_LSHIFT, VK_RSHIFT,
        VK_MENU, VK_LMENU, VK_RMENU,
    };
    for (int virtualKey : kPriorityKeys) {
        if (g_keyboardComboKeys[virtualKey]) {
            AppendLabel(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), GetVirtualKeyName(virtualKey));
        }
    }

    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (IsPriorityKeyboardLabelKey(virtualKey)) {
            continue;
        }
        if (g_keyboardComboKeys[virtualKey]) {
            AppendLabel(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), GetVirtualKeyName(virtualKey));
        }
    }

    if (g_keyboardComboLabel[0] == '\0') {
        std::snprintf(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), "Not set");
    }
}

void UpdateControllerComboLabel() {
    g_controllerComboLabel[0] = '\0';
    if ((g_controllerComboMask & kControllerDpadUp) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Up");
    if ((g_controllerComboMask & kControllerDpadDown) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Down");
    if ((g_controllerComboMask & kControllerDpadLeft) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Left");
    if ((g_controllerComboMask & kControllerDpadRight) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Right");
    if ((g_controllerComboMask & kControllerStart) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Start");
    if ((g_controllerComboMask & kControllerBack) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Back");
    if ((g_controllerComboMask & kControllerLeftThumb) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LStick");
    if ((g_controllerComboMask & kControllerRightThumb) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RStick");
    if ((g_controllerComboMask & kControllerLeftShoulder) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LB");
    if ((g_controllerComboMask & kControllerRightShoulder) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RB");
    if ((g_controllerComboMask & kControllerA) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "A");
    if ((g_controllerComboMask & kControllerB) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "B");
    if ((g_controllerComboMask & kControllerX) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "X");
    if ((g_controllerComboMask & kControllerY) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Y");
    if ((g_controllerComboMask & kControllerLeftTrigger) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LT");
    if ((g_controllerComboMask & kControllerRightTrigger) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RT");
    if (g_controllerComboLabel[0] == '\0') {
        std::snprintf(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Not set");
    }
}

bool BuildKeyComboConfigPath(wchar_t* path, DWORD pathCount) {
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

    wchar_t keyComboPath[MAX_PATH]{};
    if (swprintf_s(keyComboPath, L"%s\\KeyCombo", configPath) < 0) {
        return false;
    }
    CreateDirectoryW(keyComboPath, nullptr);

    return swprintf_s(path, pathCount, L"%s\\KeyCombo.json", keyComboPath) >= 0;
}

bool ParseIntAfter(const char* section, const char* key, int& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    return std::sscanf(found + 1, " %d", &value) == 1;
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

bool ParseKeyboardKeysArray(const char* section, bool* outputKeys) {
    if (section == nullptr || outputKeys == nullptr) {
        return false;
    }

    const char* found = std::strstr(section, "\"keys\"");
    if (found == nullptr) {
        return false;
    }

    const char* cursor = std::strchr(found, '[');
    const char* end = cursor != nullptr ? std::strchr(cursor, ']') : nullptr;
    if (cursor == nullptr || end == nullptr || cursor >= end) {
        return false;
    }

    ClearKeyboardKeySet(outputKeys);
    bool parsedAny = false;
    while (cursor < end) {
        int parsed = 0;
        int consumed = 0;
        if (std::sscanf(cursor, " %d%n", &parsed, &consumed) == 1 && consumed > 0) {
            const int normalized = NormalizeVirtualKey(static_cast<WPARAM>(parsed));
            if (normalized > 0) {
                outputKeys[normalized] = true;
                parsedAny = true;
            }
            cursor += consumed;
        } else {
            ++cursor;
        }
    }

    return parsedAny;
}

void ApplyLegacyKeyboardCombo(int virtualKey, bool ctrl, bool shift, bool alt) {
    ClearKeyboardKeySet(g_keyboardComboKeys);
    if (ctrl) {
        g_keyboardComboKeys[VK_CONTROL] = true;
    }
    if (shift) {
        g_keyboardComboKeys[VK_SHIFT] = true;
    }
    if (alt) {
        g_keyboardComboKeys[VK_MENU] = true;
    }

    const int normalized = NormalizeVirtualKey(static_cast<WPARAM>(virtualKey));
    if (normalized > 0) {
        g_keyboardComboKeys[normalized] = true;
    }

    if (!HasKeyboardKeySet(g_keyboardComboKeys)) {
        SetDefaultKeyboardCombo();
    }
}

void AppendKeyboardKeysJson(char* buffer, std::size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    bool first = true;
    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (!g_keyboardComboKeys[virtualKey]) {
            continue;
        }

        char item[16]{};
        std::snprintf(item, sizeof(item), first ? "%d" : ", %d", virtualKey);
        const std::size_t currentLength = std::strlen(buffer);
        if (currentLength + std::strlen(item) + 1 >= bufferSize) {
            break;
        }
        std::strncat(buffer, item, bufferSize - currentLength - 1);
        first = false;
    }
}

void SaveKeyComboConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildKeyComboConfigPath(path, MAX_PATH)) {
        return;
    }

    char keyboardKeysJson[1024]{};
    AppendKeyboardKeysJson(keyboardKeysJson, sizeof(keyboardKeysJson));
    int legacyPrimaryKey = VK_INSERT;
    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (g_keyboardComboKeys[virtualKey] &&
            virtualKey != VK_CONTROL &&
            virtualKey != VK_SHIFT &&
            virtualKey != VK_MENU) {
            legacyPrimaryKey = virtualKey;
            break;
        }
        if (g_keyboardComboKeys[virtualKey]) {
            legacyPrimaryKey = virtualKey;
        }
    }

    char json[1536]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 2,\n"
        "  \"keyboard\": {\n"
        "    \"keys\": [%s],\n"
        "    \"key\": %d,\n"
        "    \"ctrl\": %s,\n"
        "    \"shift\": %s,\n"
        "    \"alt\": %s\n"
        "  },\n"
        "  \"controller\": {\n"
        "    \"mask\": %u\n"
        "  }\n"
        "}\n",
        keyboardKeysJson,
        legacyPrimaryKey,
        (g_keyboardComboKeys[VK_CONTROL] || g_keyboardComboKeys[VK_LCONTROL] || g_keyboardComboKeys[VK_RCONTROL]) ? "true" : "false",
        (g_keyboardComboKeys[VK_SHIFT] || g_keyboardComboKeys[VK_LSHIFT] || g_keyboardComboKeys[VK_RSHIFT]) ? "true" : "false",
        (g_keyboardComboKeys[VK_MENU] || g_keyboardComboKeys[VK_LMENU] || g_keyboardComboKeys[VK_RMENU]) ? "true" : "false",
        g_controllerComboMask);

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
}

void EnsureKeyComboConfigLoaded() {
    if (g_keyComboConfigLoaded) {
        return;
    }

    g_keyComboConfigLoaded = true;
    SetDefaultKeyboardCombo();
    wchar_t path[MAX_PATH]{};
    if (BuildKeyComboConfigPath(path, MAX_PATH)) {
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            char json[4096]{};
            DWORD read = 0;
            if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
                json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';
                const char* keyboard = std::strstr(json, "\"keyboard\"");
                const char* controller = std::strstr(json, "\"controller\"");
                int parsed = 0;
                bool parsedKeyboardKeys[kVirtualKeyCount]{};
                if (ParseKeyboardKeysArray(keyboard, parsedKeyboardKeys)) {
                    CopyKeyboardKeySet(g_keyboardComboKeys, parsedKeyboardKeys);
                } else {
                    bool legacyCtrl = false;
                    bool legacyShift = false;
                    bool legacyAlt = false;
                    int legacyKey = VK_INSERT;
                    if (ParseIntAfter(keyboard, "\"key\"", parsed) && parsed > 0 && parsed <= 0xFF) {
                        legacyKey = parsed;
                    }
                    ParseBoolAfter(keyboard, "\"ctrl\"", legacyCtrl);
                    ParseBoolAfter(keyboard, "\"shift\"", legacyShift);
                    ParseBoolAfter(keyboard, "\"alt\"", legacyAlt);
                    ApplyLegacyKeyboardCombo(legacyKey, legacyCtrl, legacyShift, legacyAlt);
                }
                if (ParseIntAfter(controller, "\"mask\"", parsed) && parsed > 0) {
                    g_controllerComboMask = static_cast<std::uint32_t>(parsed);
                }
            }
            CloseHandle(file);
        } else {
            SaveKeyComboConfig();
        }
    }

    UpdateKeyboardComboLabel();
    UpdateControllerComboLabel();
}

std::uint32_t GetControllerFlagsFromState(const XInputGamepadState& gamepad) {
    std::uint32_t flags = 0;
    if ((gamepad.buttons & 0x0001) != 0) flags |= kControllerDpadUp;
    if ((gamepad.buttons & 0x0002) != 0) flags |= kControllerDpadDown;
    if ((gamepad.buttons & 0x0004) != 0) flags |= kControllerDpadLeft;
    if ((gamepad.buttons & 0x0008) != 0) flags |= kControllerDpadRight;
    if ((gamepad.buttons & 0x0010) != 0) flags |= kControllerStart;
    if ((gamepad.buttons & 0x0020) != 0) flags |= kControllerBack;
    if ((gamepad.buttons & 0x0040) != 0) flags |= kControllerLeftThumb;
    if ((gamepad.buttons & kXInputGamepadRightThumb) != 0) flags |= kControllerRightThumb;
    if ((gamepad.buttons & kXInputGamepadLeftShoulder) != 0) flags |= kControllerLeftShoulder;
    if ((gamepad.buttons & kXInputGamepadRightShoulder) != 0) flags |= kControllerRightShoulder;
    if ((gamepad.buttons & 0x1000) != 0) flags |= kControllerA;
    if ((gamepad.buttons & 0x2000) != 0) flags |= kControllerB;
    if ((gamepad.buttons & 0x4000) != 0) flags |= kControllerX;
    if ((gamepad.buttons & 0x8000) != 0) flags |= kControllerY;
    if (gamepad.leftTrigger > kXInputTriggerThreshold) flags |= kControllerLeftTrigger;
    if (gamepad.rightTrigger > kXInputTriggerThreshold) flags |= kControllerRightTrigger;
    return flags;
}
