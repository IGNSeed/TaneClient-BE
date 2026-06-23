bool BuildTabConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\Tab.json", guiPath) >= 0;
}

bool IsReadableAddress(const void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr || size == 0 || VirtualQuery(address, &info, sizeof(info)) == 0) {
        return false;
    }
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto end = begin + size;
    const auto regionBegin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto regionEnd = regionBegin + info.RegionSize;
    return end >= begin && begin >= regionBegin && end <= regionEnd;
}

bool IsExecutableAddress(const void* address) {
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr || VirtualQuery(address, &info, sizeof(info)) == 0) {
        return false;
    }
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protect = info.Protect & 0xFF;
    return protect == PAGE_EXECUTE ||
        protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE ||
        protect == PAGE_EXECUTE_WRITECOPY;
}

template<typename T>
bool ReadOffset(const void* base, std::size_t offset, T& value) {
    const auto* address = reinterpret_cast<const std::uint8_t*>(base) + offset;
    if (!IsReadableAddress(address, sizeof(T))) {
        return false;
    }
    __try {
        value = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
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
    case VK_CONTROL: return "Ctrl";
    case VK_LCONTROL: return "LCtrl";
    case VK_RCONTROL: return "RCtrl";
    case VK_SHIFT: return "Shift";
    case VK_LSHIFT: return "LShift";
    case VK_RSHIFT: return "RShift";
    case VK_MENU: return "Alt";
    case VK_LMENU: return "LAlt";
    case VK_RMENU: return "RAlt";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Escape";
    case VK_BACK: return "Backspace";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_XBUTTON1: return "Mouse Side 1";
    case VK_XBUTTON2: return "Mouse Side 2";
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
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9) {
        std::snprintf(name, sizeof(name), "Num%d", virtualKey - VK_NUMPAD0);
        return name;
    }

    const UINT scanCode = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC_EX);
    if (scanCode != 0) {
        const LONG lParam = static_cast<LONG>(scanCode << 16);
        if (GetKeyNameTextA(lParam, name, static_cast<int>(sizeof(name))) > 0) {
            return name;
        }
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

int ResolveMouseButtonVirtualKey(UINT message, WPARAM wParam) {
    switch (message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
        return GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 :
            (GET_XBUTTON_WPARAM(wParam) == XBUTTON2 ? VK_XBUTTON2 : 0);
    default:
        return 0;
    }
}

bool IsVirtualKeyDown(int virtualKey) {
    return virtualKey > 0 && virtualKey < kVirtualKeyCount && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
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

void UpdateKeyboardComboLabel();
void UpdateControllerComboLabel();
void SaveTabConfig();

void StoreCapturedKeyboardCombo() {
    if (!HasKeyboardKeySet(g_keyboardCaptureCandidateKeys)) {
        return;
    }

    int firstKey = 0;
    int secondKey = 0;
    constexpr int kPriorityKeys[] = {
        VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
        VK_SHIFT, VK_LSHIFT, VK_RSHIFT,
        VK_MENU, VK_LMENU, VK_RMENU,
    };
    auto appendKey = [&](int virtualKey) {
        if (virtualKey <= 0 || virtualKey >= kVirtualKeyCount || !g_keyboardCaptureCandidateKeys[virtualKey]) {
            return;
        }
        if (firstKey == 0) {
            firstKey = virtualKey;
        } else if (secondKey == 0 && firstKey != virtualKey) {
            secondKey = virtualKey;
        }
    };

    for (int virtualKey : kPriorityKeys) {
        appendKey(virtualKey);
    }
    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (!IsPriorityKeyboardLabelKey(virtualKey)) {
            appendKey(virtualKey);
        }
    }
    if (firstKey != 0) {
        g_keyboardKeyA.store(firstKey, std::memory_order_relaxed);
        g_keyboardKeyB.store(secondKey, std::memory_order_relaxed);
        UpdateKeyboardComboLabel();
        SaveTabConfig();
    }
}

void UpdateKeyboardComboLabel() {
    g_keyboardComboLabel[0] = '\0';
    const int keyA = g_keyboardKeyA.load(std::memory_order_relaxed);
    const int keyB = g_keyboardKeyB.load(std::memory_order_relaxed);
    AppendLabel(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), GetVirtualKeyName(keyA));
    if (keyB != 0 && keyB != keyA) {
        AppendLabel(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), GetVirtualKeyName(keyB));
    }
    if (g_keyboardComboLabel[0] == '\0') {
        std::snprintf(g_keyboardComboLabel, sizeof(g_keyboardComboLabel), "Not set");
    }
}

void UpdateControllerComboLabel() {
    const std::uint32_t mask = g_controllerMask.load(std::memory_order_relaxed);
    g_controllerComboLabel[0] = '\0';
    if ((mask & kControllerDpadUp) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Up");
    if ((mask & kControllerDpadDown) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Down");
    if ((mask & kControllerDpadLeft) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Left");
    if ((mask & kControllerDpadRight) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "DPad Right");
    if ((mask & kControllerStart) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Start");
    if ((mask & kControllerBack) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Back");
    if ((mask & kControllerLeftThumb) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LStick");
    if ((mask & kControllerRightThumb) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RStick");
    if ((mask & kControllerLeftShoulder) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LB");
    if ((mask & kControllerRightShoulder) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RB");
    if ((mask & kControllerA) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "A");
    if ((mask & kControllerB) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "B");
    if ((mask & kControllerX) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "X");
    if ((mask & kControllerY) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Y");
    if ((mask & kControllerLeftTrigger) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "LT");
    if ((mask & kControllerRightTrigger) != 0) AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), "RT");
    for (std::uint32_t index = 0; index < kExtendedControllerButtonCount; ++index) {
        if ((mask & (1u << (kExtendedControllerButtonShift + index))) != 0) {
            char label[16]{};
            std::snprintf(label, sizeof(label), "Rear %u", index + 1);
            AppendLabel(g_controllerComboLabel, sizeof(g_controllerComboLabel), label);
        }
    }
    if (g_controllerComboLabel[0] == '\0') {
        std::snprintf(g_controllerComboLabel, sizeof(g_controllerComboLabel), "Not set");
    }
}

void SaveTabConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildTabConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[384]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 1,\n"
        "  \"enabled\": %s,\n"
        "  \"keyboardKeyA\": %d,\n"
        "  \"keyboardKeyB\": %d,\n"
        "  \"controllerMask\": %u\n"
        "}\n",
        g_tabEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_keyboardKeyA.load(std::memory_order_relaxed),
        g_keyboardKeyB.load(std::memory_order_relaxed),
        g_controllerMask.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
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

bool ParseIntAfter(const char* section, const char* key, int& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }
    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(found + 1, &end, 10);
    if (end == found + 1) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

void EnsureTabConfigLoaded() {
    bool expected = false;
    if (!g_tabConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildTabConfigPath(path, MAX_PATH)) {
        UpdateKeyboardComboLabel();
        UpdateControllerComboLabel();
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        UpdateKeyboardComboLabel();
        UpdateControllerComboLabel();
        SaveTabConfig();
        return;
    }

    char json[1024]{};
    DWORD read = 0;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';
        bool enabled = false;
        int keyA = kDefaultKeyboardKeyA;
        int keyB = kDefaultKeyboardKeyB;
        int controllerMask = static_cast<int>(kDefaultControllerMask);
        ParseBoolAfter(json, "\"enabled\"", enabled);
        ParseIntAfter(json, "\"keyboardKeyA\"", keyA);
        ParseIntAfter(json, "\"keyboardKeyB\"", keyB);
        ParseIntAfter(json, "\"controllerMask\"", controllerMask);
        g_tabEnabled.store(enabled, std::memory_order_relaxed);
        g_keyboardKeyA.store(keyA > 0 && keyA < kVirtualKeyCount ? keyA : kDefaultKeyboardKeyA, std::memory_order_relaxed);
        g_keyboardKeyB.store(keyB > 0 && keyB < kVirtualKeyCount ? keyB : 0, std::memory_order_relaxed);
        g_controllerMask.store(controllerMask >= 0 ? static_cast<std::uint32_t>(controllerMask) : kDefaultControllerMask, std::memory_order_relaxed);
    }
    CloseHandle(file);
    UpdateKeyboardComboLabel();
    UpdateControllerComboLabel();
}

XInputGetStateFn ResolveXInputGetState() {
    if (g_triedResolveXInput) {
        return g_xInputGetState;
    }
    g_triedResolveXInput = true;
    constexpr const wchar_t* kDllNames[] = {
        L"xinput1_4.dll",
        L"xinput1_3.dll",
        L"xinput9_1_0.dll",
    };
    for (const wchar_t* dllName : kDllNames) {
        HMODULE module = LoadLibraryW(dllName);
        if (module == nullptr) {
            continue;
        }
        g_xInputGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(module, "XInputGetState"));
        if (g_xInputGetState != nullptr) {
            return g_xInputGetState;
        }
    }
    return nullptr;
}

bool ReadXInputControllerFlags(std::uint32_t& flags, ULONGLONG now) {
    XInputGetStateFn getState = ResolveXInputGetState();
    if (getState == nullptr) {
        return false;
    }
    if (!g_xInputControllerPresent.load(std::memory_order_relaxed)) {
        const ULONGLONG lastPoll = g_lastXInputPollTick.load(std::memory_order_relaxed);
        if (lastPoll != 0 && now - lastPoll < kDisconnectedXInputPollIntervalMs) {
            flags = g_cachedXInputControllerFlags.load(std::memory_order_relaxed);
            return flags != 0;
        }
        g_lastXInputPollTick.store(now, std::memory_order_relaxed);
    }

    bool found = false;
    std::uint32_t combined = 0;
    for (DWORD userIndex = 0; userIndex < 4; ++userIndex) {
        XInputState state{};
        if (getState(userIndex, &state) != ERROR_SUCCESS) {
            continue;
        }
        found = true;
        const WORD buttons = state.gamepad.buttons;
        if ((buttons & kXInputGamepadDpadUp) != 0) combined |= kControllerDpadUp;
        if ((buttons & kXInputGamepadDpadDown) != 0) combined |= kControllerDpadDown;
        if ((buttons & kXInputGamepadDpadLeft) != 0) combined |= kControllerDpadLeft;
        if ((buttons & kXInputGamepadDpadRight) != 0) combined |= kControllerDpadRight;
        if ((buttons & kXInputGamepadStart) != 0) combined |= kControllerStart;
        if ((buttons & kXInputGamepadBack) != 0) combined |= kControllerBack;
        if ((buttons & kXInputGamepadLeftThumb) != 0) combined |= kControllerLeftThumb;
        if ((buttons & kXInputGamepadRightThumb) != 0) combined |= kControllerRightThumb;
        if ((buttons & kXInputGamepadLeftShoulder) != 0) combined |= kControllerLeftShoulder;
        if ((buttons & kXInputGamepadRightShoulder) != 0) combined |= kControllerRightShoulder;
        if ((buttons & kXInputGamepadA) != 0) combined |= kControllerA;
        if ((buttons & kXInputGamepadB) != 0) combined |= kControllerB;
        if ((buttons & kXInputGamepadX) != 0) combined |= kControllerX;
        if ((buttons & kXInputGamepadY) != 0) combined |= kControllerY;
        if (state.gamepad.leftTrigger >= kXInputTriggerThreshold) combined |= kControllerLeftTrigger;
        if (state.gamepad.rightTrigger >= kXInputTriggerThreshold) combined |= kControllerRightTrigger;
    }
    g_xInputControllerPresent.store(found, std::memory_order_relaxed);
    g_cachedXInputControllerFlags.store(combined, std::memory_order_relaxed);
    flags = combined;
    return found;
}

void ReadVirtualGamepadFlags(std::uint32_t& flags) {
    flags = 0;
#ifdef VK_GAMEPAD_DPAD_UP
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_UP) & 0x8000) != 0) flags |= kControllerDpadUp;
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_DOWN) & 0x8000) != 0) flags |= kControllerDpadDown;
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_LEFT) & 0x8000) != 0) flags |= kControllerDpadLeft;
    if ((GetAsyncKeyState(VK_GAMEPAD_DPAD_RIGHT) & 0x8000) != 0) flags |= kControllerDpadRight;
    if ((GetAsyncKeyState(VK_GAMEPAD_MENU) & 0x8000) != 0) flags |= kControllerStart;
    if ((GetAsyncKeyState(VK_GAMEPAD_VIEW) & 0x8000) != 0) flags |= kControllerBack;
    if ((GetAsyncKeyState(VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON) & 0x8000) != 0) flags |= kControllerLeftThumb;
    if ((GetAsyncKeyState(VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON) & 0x8000) != 0) flags |= kControllerRightThumb;
    if ((GetAsyncKeyState(VK_GAMEPAD_LEFT_SHOULDER) & 0x8000) != 0) flags |= kControllerLeftShoulder;
    if ((GetAsyncKeyState(VK_GAMEPAD_RIGHT_SHOULDER) & 0x8000) != 0) flags |= kControllerRightShoulder;
    if ((GetAsyncKeyState(VK_GAMEPAD_A) & 0x8000) != 0) flags |= kControllerA;
    if ((GetAsyncKeyState(VK_GAMEPAD_B) & 0x8000) != 0) flags |= kControllerB;
    if ((GetAsyncKeyState(VK_GAMEPAD_X) & 0x8000) != 0) flags |= kControllerX;
    if ((GetAsyncKeyState(VK_GAMEPAD_Y) & 0x8000) != 0) flags |= kControllerY;
    if ((GetAsyncKeyState(VK_GAMEPAD_LEFT_TRIGGER) & 0x8000) != 0) flags |= kControllerLeftTrigger;
    if ((GetAsyncKeyState(VK_GAMEPAD_RIGHT_TRIGGER) & 0x8000) != 0) flags |= kControllerRightTrigger;
#endif
}

std::uint32_t ReadControllerFlags() {
    const ULONGLONG now = GetTickCount64();
    ULONGLONG lastReadTick = g_lastControllerReadTick.load(std::memory_order_relaxed);
    if (lastReadTick != 0 && now - lastReadTick < kControllerPollIntervalMs) {
        return g_cachedControllerFlags.load(std::memory_order_relaxed);
    }
    if (!g_lastControllerReadTick.compare_exchange_strong(lastReadTick, now, std::memory_order_relaxed)) {
        return g_cachedControllerFlags.load(std::memory_order_relaxed);
    }

    std::uint32_t xInputFlags = g_cachedXInputControllerFlags.load(std::memory_order_relaxed);
    std::uint32_t virtualFlags = 0;
    ReadXInputControllerFlags(xInputFlags, now);
    ReadVirtualGamepadFlags(virtualFlags);
    const std::uint32_t flags = xInputFlags | virtualFlags | tane::payload::GetExtendedControllerFlags();
    g_cachedControllerFlags.store(flags, std::memory_order_relaxed);
    return flags;
}

bool IsKeyboardComboHeld() {
    const int keyA = g_keyboardKeyA.load(std::memory_order_relaxed);
    const int keyB = g_keyboardKeyB.load(std::memory_order_relaxed);
    if (keyA <= 0) {
        return false;
    }
    return IsVirtualKeyDown(keyA) && (keyB <= 0 || IsVirtualKeyDown(keyB));
}

bool IsConfiguredKeyboardKey(int virtualKey) {
    const int keyA = g_keyboardKeyA.load(std::memory_order_relaxed);
    const int keyB = g_keyboardKeyB.load(std::memory_order_relaxed);
    return virtualKey > 0 && (virtualKey == keyA || virtualKey == keyB);
}

bool IsControllerComboHeld(std::uint32_t flags) {
    const std::uint32_t mask = g_controllerMask.load(std::memory_order_relaxed);
    return mask != 0 && (flags & mask) == mask;
}

void TickControllerComboCapture() {
    if (!g_controllerCaptureActive) {
        return;
    }
    const std::uint32_t flags = ReadControllerFlags();
    if (flags != 0) {
        if (g_controllerCaptureWaitingForRelease) {
            g_controllerCaptureWaitingForRelease = false;
        }
        g_controllerCaptureCandidateMask |= flags;
    } else if (!g_controllerCaptureWaitingForRelease && g_controllerCaptureCandidateMask != 0) {
        g_controllerMask.store(g_controllerCaptureCandidateMask, std::memory_order_relaxed);
        g_controllerCaptureActive = false;
        g_controllerCaptureWaitingForRelease = false;
        g_controllerCaptureCandidateMask = 0;
        UpdateControllerComboLabel();
        SaveTabConfig();
    }
}

void* GetLocalPlayer(void* clientInstance) {
    if (clientInstance == nullptr) {
        return nullptr;
    }
    void* vtable = nullptr;
    void* function = nullptr;
    if (!ReadOffset(clientInstance, 0, vtable) ||
        !ReadOffset(vtable, kClientInstanceLocalPlayerVtableOffset, function) ||
        !IsExecutableAddress(function)) {
        return nullptr;
    }
    __try {
        return reinterpret_cast<GetLocalPlayerFn>(function)(clientInstance);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void* GetLevelFromLocalPlayer(void* localPlayer) {
    void* level = nullptr;
    return ReadOffset(localPlayer, kActorLevelOffset, level) ? level : nullptr;
}

PlayerMap* GetPlayerMap(void* level) {
    if (level == nullptr) {
        return nullptr;
    }

    void* vtable = nullptr;
    void* function = nullptr;
    if (ReadOffset(level, 0, vtable) &&
        ReadOffset(vtable, kLevelGetPlayerListVtableOffset, function) &&
        IsExecutableAddress(function)) {
        __try {
            if (PlayerMap* playerMap = reinterpret_cast<GetPlayerListFn>(function)(level); playerMap != nullptr) {
                return playerMap;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    PlayerMap* playerMap = nullptr;
    return ReadOffset(level, kLevelPlayerListMapOffset, playerMap) ? playerMap : nullptr;
}
