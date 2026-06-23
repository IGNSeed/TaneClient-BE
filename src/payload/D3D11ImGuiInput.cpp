bool IsMouseMessage(UINT message) {
    return message == WM_MOUSEMOVE ||
        message == WM_MOUSELEAVE ||
        message == WM_LBUTTONDOWN ||
        message == WM_LBUTTONUP ||
        message == WM_LBUTTONDBLCLK ||
        message == WM_RBUTTONDOWN ||
        message == WM_RBUTTONUP ||
        message == WM_RBUTTONDBLCLK ||
        message == WM_MBUTTONDOWN ||
        message == WM_MBUTTONUP ||
        message == WM_MBUTTONDBLCLK ||
        message == WM_XBUTTONDOWN ||
        message == WM_XBUTTONUP ||
        message == WM_XBUTTONDBLCLK ||
        message == WM_MOUSEWHEEL ||
        message == WM_MOUSEHWHEEL;
}

bool IsKeyboardMessage(UINT message) {
    return message == WM_KEYDOWN ||
        message == WM_KEYUP ||
        message == WM_SYSKEYDOWN ||
        message == WM_SYSKEYUP ||
        message == WM_CHAR ||
        message == WM_SYSCHAR;
}

bool IsPointerMessage(UINT message) {
#ifdef WM_POINTERUPDATE
    switch (message) {
    case WM_POINTERUPDATE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERENTER:
    case WM_POINTERLEAVE:
    case WM_POINTERACTIVATE:
    case WM_POINTERCAPTURECHANGED:
#ifdef WM_POINTERWHEEL
    case WM_POINTERWHEEL:
#endif
#ifdef WM_POINTERHWHEEL
    case WM_POINTERHWHEEL:
#endif
        return true;
    default:
        break;
    }
#endif
    return message == WM_TOUCH || message == WM_GESTURE;
}

bool IsRawInputMessage(UINT message) {
    return message == WM_INPUT || message == WM_INPUT_DEVICE_CHANGE;
}

bool IsMouseVirtualKey(int virtualKey) {
    return virtualKey == VK_LBUTTON ||
        virtualKey == VK_RBUTTON ||
        virtualKey == VK_MBUTTON ||
        virtualKey == VK_XBUTTON1 ||
        virtualKey == VK_XBUTTON2;
}

bool ShouldAllowMenuVirtualKey(int virtualKey) {
    const bool controllerTogglePolling =
        tane::gui::IsControllerTogglePolling() ||
        tane::gui::IsTabControllerComboPolling() ||
        tane::camera::IsZoomControllerComboPolling() ||
        tane::camera::IsFreeLookControllerComboPolling();

    if (virtualKey == VK_INSERT ||
        virtualKey == VK_CONTROL ||
        virtualKey == VK_LCONTROL ||
        virtualKey == VK_RCONTROL ||
        virtualKey == VK_SHIFT ||
        virtualKey == VK_LSHIFT ||
        virtualKey == VK_RSHIFT ||
        virtualKey == VK_MENU ||
        virtualKey == VK_LMENU ||
        virtualKey == VK_RMENU) {
        return true;
    }

#ifdef VK_GAMEPAD_LEFT_SHOULDER
    if (!controllerTogglePolling) {
        return virtualKey == VK_GAMEPAD_LEFT_SHOULDER ||
            virtualKey == VK_GAMEPAD_LEFT_TRIGGER ||
            virtualKey == VK_GAMEPAD_RIGHT_SHOULDER ||
            virtualKey == VK_GAMEPAD_RIGHT_TRIGGER ||
            virtualKey == VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON;
    }

    return virtualKey == VK_GAMEPAD_A ||
        virtualKey == VK_GAMEPAD_B ||
        virtualKey == VK_GAMEPAD_X ||
        virtualKey == VK_GAMEPAD_Y ||
        virtualKey == VK_GAMEPAD_RIGHT_SHOULDER ||
        virtualKey == VK_GAMEPAD_LEFT_SHOULDER ||
        virtualKey == VK_GAMEPAD_LEFT_TRIGGER ||
        virtualKey == VK_GAMEPAD_RIGHT_TRIGGER ||
        virtualKey == VK_GAMEPAD_DPAD_UP ||
        virtualKey == VK_GAMEPAD_DPAD_DOWN ||
        virtualKey == VK_GAMEPAD_DPAD_LEFT ||
        virtualKey == VK_GAMEPAD_DPAD_RIGHT ||
        virtualKey == VK_GAMEPAD_MENU ||
        virtualKey == VK_GAMEPAD_VIEW ||
        virtualKey == VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON ||
        virtualKey == VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON;
#else
    return false;
#endif
}

bool IsNamedProc(LPCSTR procName, const char* expected) {
    if (procName == nullptr || reinterpret_cast<ULONG_PTR>(procName) <= 0xFFFF) {
        return false;
    }

    return std::strcmp(procName, expected) == 0;
}

void* ResolveExport(HMODULE module, LPCSTR procName) {
    if (module == nullptr || procName == nullptr) {
        return nullptr;
    }

    FARPROC proc = g_originalGetProcAddress != nullptr
        ? g_originalGetProcAddress(module, procName)
        : GetProcAddress(module, procName);
    return reinterpret_cast<void*>(proc);
}

bool CreateAndEnableHook(void* target, void* detour, void** original) {
    if (target == nullptr || detour == nullptr || original == nullptr) {
        return false;
    }

    MH_STATUS createStatus = MH_CreateHook(target, detour, original);
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    MH_STATUS enableStatus = MH_EnableHook(target);
    return enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED;
}

template <typename T>
bool ClearGameInputState(T* state) {
    if (state == nullptr) {
        return false;
    }

    SecureZeroMemory(state, sizeof(T));
    return false;
}

HRESULT STDMETHODCALLTYPE HookGameInputGetCurrentReading(
    IGameInput* self,
    GameInputKind inputKind,
    IGameInputDevice* device,
    IGameInputReading** reading) {
    HRESULT hr = g_originalGameInputGetCurrentReading != nullptr
        ? g_originalGameInputGetCurrentReading(self, inputKind, device, reading)
        : GAMEINPUT_E_READING_NOT_FOUND;
    if (SUCCEEDED(hr) && reading != nullptr && *reading != nullptr) {
        InstallGameInputReadingHooks(*reading);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE HookGameInputGetNextReading(
    IGameInput* self,
    IGameInputReading* referenceReading,
    GameInputKind inputKind,
    IGameInputDevice* device,
    IGameInputReading** reading) {
    HRESULT hr = g_originalGameInputGetNextReading != nullptr
        ? g_originalGameInputGetNextReading(self, referenceReading, inputKind, device, reading)
        : GAMEINPUT_E_READING_NOT_FOUND;
    if (SUCCEEDED(hr) && reading != nullptr && *reading != nullptr) {
        InstallGameInputReadingHooks(*reading);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE HookGameInputGetPreviousReading(
    IGameInput* self,
    IGameInputReading* referenceReading,
    GameInputKind inputKind,
    IGameInputDevice* device,
    IGameInputReading** reading) {
    HRESULT hr = g_originalGameInputGetPreviousReading != nullptr
        ? g_originalGameInputGetPreviousReading(self, referenceReading, inputKind, device, reading)
        : GAMEINPUT_E_READING_NOT_FOUND;
    if (SUCCEEDED(hr) && reading != nullptr && *reading != nullptr) {
        InstallGameInputReadingHooks(*reading);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE HookGameInputGetTemporalReading(
    IGameInput* self,
    std::uint64_t timestamp,
    IGameInputDevice* device,
    IGameInputReading** reading) {
    HRESULT hr = g_originalGameInputGetTemporalReading != nullptr
        ? g_originalGameInputGetTemporalReading(self, timestamp, device, reading)
        : GAMEINPUT_E_READING_NOT_FOUND;
    if (SUCCEEDED(hr) && reading != nullptr && *reading != nullptr) {
        InstallGameInputReadingHooks(*reading);
    }
    return hr;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetControllerAxisCount(IGameInputReading* self) {
    return g_originalReadingGetControllerAxisCount != nullptr ? g_originalReadingGetControllerAxisCount(self) : 0;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetControllerAxisState(IGameInputReading* self, std::uint32_t count, float* stateArray) {
    if (tane::gui::IsMenuOpen()) {
        if (stateArray != nullptr && count > 0) {
            SecureZeroMemory(stateArray, sizeof(float) * count);
        }
        return count;
    }
    return g_originalReadingGetControllerAxisState != nullptr ? g_originalReadingGetControllerAxisState(self, count, stateArray) : 0;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetControllerButtonCount(IGameInputReading* self) {
    return g_originalReadingGetControllerButtonCount != nullptr ? g_originalReadingGetControllerButtonCount(self) : 0;
}

void UpdateExtendedControllerButtonFlags(std::uint32_t count, const bool* stateArray) {
    std::uint32_t flags = 0;
    if (stateArray != nullptr) {
        const std::uint32_t buttonCount = count < kMaxExtendedControllerButtons ? count : kMaxExtendedControllerButtons;
        for (std::uint32_t index = 0; index < buttonCount; ++index) {
            if (stateArray[index]) {
                flags |= 1u << (kExtendedControllerButtonShift + index);
            }
        }
    }

    g_cachedExtendedControllerFlags.store(flags, std::memory_order_relaxed);
    g_lastExtendedControllerTick.store(GetTickCount64(), std::memory_order_relaxed);
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetControllerButtonState(IGameInputReading* self, std::uint32_t count, bool* stateArray) {
    const std::uint32_t result = g_originalReadingGetControllerButtonState != nullptr
        ? g_originalReadingGetControllerButtonState(self, count, stateArray)
        : 0;
    if (stateArray != nullptr) {
        UpdateExtendedControllerButtonFlags(result < count ? result : count, stateArray);
    }

    if (tane::gui::IsMenuOpen()) {
        if (stateArray != nullptr && count > 0) {
            SecureZeroMemory(stateArray, sizeof(bool) * count);
        }
        return count;
    }
    return result;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetControllerSwitchCount(IGameInputReading* self) {
    return g_originalReadingGetControllerSwitchCount != nullptr ? g_originalReadingGetControllerSwitchCount(self) : 0;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetControllerSwitchState(IGameInputReading* self, std::uint32_t count, GameInputSwitchPosition* stateArray) {
    if (tane::gui::IsMenuOpen()) {
        if (stateArray != nullptr && count > 0) {
            SecureZeroMemory(stateArray, sizeof(GameInputSwitchPosition) * count);
        }
        return count;
    }
    return g_originalReadingGetControllerSwitchState != nullptr ? g_originalReadingGetControllerSwitchState(self, count, stateArray) : 0;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetKeyCount(IGameInputReading* self) {
    return g_originalReadingGetKeyCount != nullptr ? g_originalReadingGetKeyCount(self) : 0;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetKeyState(IGameInputReading* self, std::uint32_t count, GameInputKeyState* stateArray) {
    if (tane::gui::IsMenuOpen()) {
        if (stateArray != nullptr && count > 0) {
            SecureZeroMemory(stateArray, sizeof(GameInputKeyState) * count);
        }
        return count;
    }
    return g_originalReadingGetKeyState != nullptr ? g_originalReadingGetKeyState(self, count, stateArray) : 0;
}

bool STDMETHODCALLTYPE HookReadingGetMouseState(IGameInputReading* self, GameInputMouseState* state) {
    const bool hasState = g_originalReadingGetMouseState != nullptr && g_originalReadingGetMouseState(self, state);
    if (tane::gui::IsMenuOpen() && hasState) {
        ClearGameInputState(state);
    }
    return hasState;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetTouchCount(IGameInputReading* self) {
    return g_originalReadingGetTouchCount != nullptr ? g_originalReadingGetTouchCount(self) : 0;
}

std::uint32_t STDMETHODCALLTYPE HookReadingGetTouchState(IGameInputReading* self, std::uint32_t count, GameInputTouchState* stateArray) {
    if (tane::gui::IsMenuOpen()) {
        if (stateArray != nullptr && count > 0) {
            SecureZeroMemory(stateArray, sizeof(GameInputTouchState) * count);
        }
        return count;
    }
    return g_originalReadingGetTouchState != nullptr ? g_originalReadingGetTouchState(self, count, stateArray) : 0;
}

bool STDMETHODCALLTYPE HookReadingGetMotionState(IGameInputReading* self, GameInputMotionState* state) {
    const bool hasState = g_originalReadingGetMotionState != nullptr && g_originalReadingGetMotionState(self, state);
    if (tane::gui::IsMenuOpen() && hasState) {
        ClearGameInputState(state);
    }
    return hasState;
}

bool STDMETHODCALLTYPE HookReadingGetArcadeStickState(IGameInputReading* self, GameInputArcadeStickState* state) {
    const bool hasState = g_originalReadingGetArcadeStickState != nullptr && g_originalReadingGetArcadeStickState(self, state);
    if (tane::gui::IsMenuOpen() && hasState) {
        ClearGameInputState(state);
    }
    return hasState;
}

bool STDMETHODCALLTYPE HookReadingGetFlightStickState(IGameInputReading* self, GameInputFlightStickState* state) {
    const bool hasState = g_originalReadingGetFlightStickState != nullptr && g_originalReadingGetFlightStickState(self, state);
    if (tane::gui::IsMenuOpen() && hasState) {
        ClearGameInputState(state);
    }
    return hasState;
}

bool STDMETHODCALLTYPE HookReadingGetGamepadState(IGameInputReading* self, GameInputGamepadState* state) {
    const bool hasState = g_originalReadingGetGamepadState != nullptr && g_originalReadingGetGamepadState(self, state);
    if (tane::gui::IsMenuOpen() && hasState) {
        ClearGameInputState(state);
    }
    return hasState;
}

bool STDMETHODCALLTYPE HookReadingGetRacingWheelState(IGameInputReading* self, GameInputRacingWheelState* state) {
    const bool hasState = g_originalReadingGetRacingWheelState != nullptr && g_originalReadingGetRacingWheelState(self, state);
    if (tane::gui::IsMenuOpen() && hasState) {
        ClearGameInputState(state);
    }
    return hasState;
}

bool STDMETHODCALLTYPE HookReadingGetUiNavigationState(IGameInputReading* self, GameInputUiNavigationState* state) {
    const bool hasState = g_originalReadingGetUiNavigationState != nullptr && g_originalReadingGetUiNavigationState(self, state);
    if (tane::gui::IsMenuOpen() && hasState) {
        ClearGameInputState(state);
    }
    return hasState;
}

void InstallGameInputMethodHooks(IGameInput* gameInput) {
    if (gameInput == nullptr || g_gameInputMethodsHookInstalled.load(std::memory_order_acquire)) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(gameInput);
    if (vtable == nullptr) {
        return;
    }

    const bool ok =
        CreateAndEnableHook(vtable[4], reinterpret_cast<void*>(&HookGameInputGetCurrentReading), reinterpret_cast<void**>(&g_originalGameInputGetCurrentReading)) &&
        CreateAndEnableHook(vtable[5], reinterpret_cast<void*>(&HookGameInputGetNextReading), reinterpret_cast<void**>(&g_originalGameInputGetNextReading)) &&
        CreateAndEnableHook(vtable[6], reinterpret_cast<void*>(&HookGameInputGetPreviousReading), reinterpret_cast<void**>(&g_originalGameInputGetPreviousReading)) &&
        CreateAndEnableHook(vtable[7], reinterpret_cast<void*>(&HookGameInputGetTemporalReading), reinterpret_cast<void**>(&g_originalGameInputGetTemporalReading));
    if (ok) {
        g_gameInputMethodsHookInstalled.store(true, std::memory_order_release);
    }
}

void InstallGameInputReadingHooks(IGameInputReading* reading) {
    if (reading == nullptr || g_gameInputReadingHooksInstalled.load(std::memory_order_acquire)) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(reading);
    if (vtable == nullptr) {
        return;
    }

    const bool ok =
        CreateAndEnableHook(vtable[8], reinterpret_cast<void*>(&HookReadingGetControllerAxisCount), reinterpret_cast<void**>(&g_originalReadingGetControllerAxisCount)) &&
        CreateAndEnableHook(vtable[9], reinterpret_cast<void*>(&HookReadingGetControllerAxisState), reinterpret_cast<void**>(&g_originalReadingGetControllerAxisState)) &&
        CreateAndEnableHook(vtable[10], reinterpret_cast<void*>(&HookReadingGetControllerButtonCount), reinterpret_cast<void**>(&g_originalReadingGetControllerButtonCount)) &&
        CreateAndEnableHook(vtable[11], reinterpret_cast<void*>(&HookReadingGetControllerButtonState), reinterpret_cast<void**>(&g_originalReadingGetControllerButtonState)) &&
        CreateAndEnableHook(vtable[12], reinterpret_cast<void*>(&HookReadingGetControllerSwitchCount), reinterpret_cast<void**>(&g_originalReadingGetControllerSwitchCount)) &&
        CreateAndEnableHook(vtable[13], reinterpret_cast<void*>(&HookReadingGetControllerSwitchState), reinterpret_cast<void**>(&g_originalReadingGetControllerSwitchState)) &&
        CreateAndEnableHook(vtable[14], reinterpret_cast<void*>(&HookReadingGetKeyCount), reinterpret_cast<void**>(&g_originalReadingGetKeyCount)) &&
        CreateAndEnableHook(vtable[15], reinterpret_cast<void*>(&HookReadingGetKeyState), reinterpret_cast<void**>(&g_originalReadingGetKeyState)) &&
        CreateAndEnableHook(vtable[16], reinterpret_cast<void*>(&HookReadingGetMouseState), reinterpret_cast<void**>(&g_originalReadingGetMouseState)) &&
        CreateAndEnableHook(vtable[17], reinterpret_cast<void*>(&HookReadingGetTouchCount), reinterpret_cast<void**>(&g_originalReadingGetTouchCount)) &&
        CreateAndEnableHook(vtable[18], reinterpret_cast<void*>(&HookReadingGetTouchState), reinterpret_cast<void**>(&g_originalReadingGetTouchState)) &&
        CreateAndEnableHook(vtable[19], reinterpret_cast<void*>(&HookReadingGetMotionState), reinterpret_cast<void**>(&g_originalReadingGetMotionState)) &&
        CreateAndEnableHook(vtable[20], reinterpret_cast<void*>(&HookReadingGetArcadeStickState), reinterpret_cast<void**>(&g_originalReadingGetArcadeStickState)) &&
        CreateAndEnableHook(vtable[21], reinterpret_cast<void*>(&HookReadingGetFlightStickState), reinterpret_cast<void**>(&g_originalReadingGetFlightStickState)) &&
        CreateAndEnableHook(vtable[22], reinterpret_cast<void*>(&HookReadingGetGamepadState), reinterpret_cast<void**>(&g_originalReadingGetGamepadState)) &&
        CreateAndEnableHook(vtable[23], reinterpret_cast<void*>(&HookReadingGetRacingWheelState), reinterpret_cast<void**>(&g_originalReadingGetRacingWheelState)) &&
        CreateAndEnableHook(vtable[24], reinterpret_cast<void*>(&HookReadingGetUiNavigationState), reinterpret_cast<void**>(&g_originalReadingGetUiNavigationState));
    if (ok) {
        g_gameInputReadingHooksInstalled.store(true, std::memory_order_release);
    }
}

HRESULT WINAPI HookGameInputCreate(IGameInput** gameInput) {
    HRESULT hr = g_originalGameInputCreate != nullptr ? g_originalGameInputCreate(gameInput) : E_FAIL;
    if (SUCCEEDED(hr) && gameInput != nullptr && *gameInput != nullptr) {
        InstallGameInputMethodHooks(*gameInput);
    }
    return hr;
}

void InstallGameInputCreateHook(void* gameInputCreate) {
    if (gameInputCreate == nullptr || g_gameInputCreateHookInstalled.load(std::memory_order_acquire)) {
        return;
    }

    if (CreateAndEnableHook(gameInputCreate, reinterpret_cast<void*>(&HookGameInputCreate), reinterpret_cast<void**>(&g_originalGameInputCreate))) {
        g_gameInputCreateHookInstalled.store(true, std::memory_order_release);
    }
}

FARPROC WINAPI HookGetProcAddress(HMODULE module, LPCSTR procName) {
    FARPROC proc = g_originalGetProcAddress != nullptr ? g_originalGetProcAddress(module, procName) : nullptr;
    if (IsNamedProc(procName, "GameInputCreate")) {
        InstallGameInputCreateHook(reinterpret_cast<void*>(proc));
    } else if (IsNamedProc(procName, "XInputGetState") && g_originalXInputGetState == nullptr) {
        CreateAndEnableHook(reinterpret_cast<void*>(proc), reinterpret_cast<void*>(&HookXInputGetState), reinterpret_cast<void**>(&g_originalXInputGetState));
    } else if (IsNamedProc(procName, "XInputGetCapabilities") && g_originalXInputGetCapabilities == nullptr) {
        CreateAndEnableHook(reinterpret_cast<void*>(proc), reinterpret_cast<void*>(&HookXInputGetCapabilities), reinterpret_cast<void**>(&g_originalXInputGetCapabilities));
    }

    return proc;
}

DWORD WINAPI HookXInputGetState(DWORD userIndex, void* state) {
    constexpr std::size_t kXInputGamepadStateSize =
        sizeof(WORD) + sizeof(BYTE) + sizeof(BYTE) + sizeof(SHORT) * 4;
    const DWORD result = g_originalXInputGetState != nullptr
        ? g_originalXInputGetState(userIndex, state)
        : ERROR_DEVICE_NOT_CONNECTED;
    if (result == ERROR_SUCCESS &&
        state != nullptr &&
        tane::gui::IsMenuOpen() &&
        !tane::gui::IsControllerTogglePolling() &&
        !tane::gui::IsTabControllerComboPolling() &&
        !tane::camera::IsZoomControllerComboPolling() &&
        !tane::camera::IsFreeLookControllerComboPolling()) {
        SecureZeroMemory(reinterpret_cast<std::uint8_t*>(state) + sizeof(DWORD), kXInputGamepadStateSize);
    }

    return result;
}

DWORD WINAPI HookXInputGetCapabilities(DWORD userIndex, DWORD flags, void* capabilities) {
    return g_originalXInputGetCapabilities != nullptr
        ? g_originalXInputGetCapabilities(userIndex, flags, capabilities)
        : ERROR_DEVICE_NOT_CONNECTED;
}

void RefreshWindowSubclass(HWND hwnd) {
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }

    LONG_PTR current = GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
    if (current == reinterpret_cast<LONG_PTR>(&HookWndProc)) {
        g_gameWindow = hwnd;
        return;
    }

    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HookWndProc));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        return;
    }

    g_gameWindow = hwnd;
    g_originalWndProc = reinterpret_cast<WNDPROC>(previous);
}

void UpdateMenuInputOwnership(HWND hwnd, bool menuOpen) {
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }

    if (menuOpen) {
        RefreshWindowSubclass(hwnd);
        if (!g_menuInputCaptured || GetCapture() != hwnd) {
            SetCapture(hwnd);
            g_menuInputCaptured = true;
        }
        ClipCursor(nullptr);
        return;
    }

    if (g_menuInputCaptured) {
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        g_menuInputCaptured = false;
    }
    ClearPendingInputMessages();
}

void ForwardMessageToImGui(const MSG& message, bool rawInputMessage) {
    if (rawInputMessage) {
        return;
    }

    ImGuiExclusiveLock imguiLock(false);
    if (imguiLock && g_platformReady.load() && ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplWin32_WndProcHandler(message.hwnd, message.message, message.wParam, message.lParam);
        return;
    }

    if (g_platformReady.load()) {
        AcquireSRWLockExclusive(&g_pendingInputMessagesLock);
        if (g_pendingInputMessageCount >= kMaxPendingInputMessages) {
            for (UINT index = 1; index < g_pendingInputMessageCount; ++index) {
                g_pendingInputMessages[index - 1] = g_pendingInputMessages[index];
            }
            g_pendingInputMessageCount = kMaxPendingInputMessages - 1;
        }
        g_pendingInputMessages[g_pendingInputMessageCount++] = message;
        ReleaseSRWLockExclusive(&g_pendingInputMessagesLock);
    }
}

void DrainPendingInputMessagesToImGui() {
    if (!g_platformReady.load() || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    MSG pendingMessages[kMaxPendingInputMessages]{};
    UINT pendingCount = 0;
    AcquireSRWLockExclusive(&g_pendingInputMessagesLock);
    pendingCount = g_pendingInputMessageCount;
    for (UINT index = 0; index < pendingCount; ++index) {
        pendingMessages[index] = g_pendingInputMessages[index];
    }
    g_pendingInputMessageCount = 0;
    ReleaseSRWLockExclusive(&g_pendingInputMessagesLock);

    for (UINT index = 0; index < pendingCount; ++index) {
        ImGui_ImplWin32_WndProcHandler(
            pendingMessages[index].hwnd,
            pendingMessages[index].message,
            pendingMessages[index].wParam,
            pendingMessages[index].lParam);
    }
}

void ClearPendingInputMessages() {
    AcquireSRWLockExclusive(&g_pendingInputMessagesLock);
    g_pendingInputMessageCount = 0;
    ReleaseSRWLockExclusive(&g_pendingInputMessagesLock);
}

bool ShouldConsumeInputMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool forwardToImGui) {
    const bool mouseMessage = IsMouseMessage(message);
    const bool keyboardMessage = IsKeyboardMessage(message);
    const bool rawInputMessage = IsRawInputMessage(message);
    const bool pointerMessage = IsPointerMessage(message);
    const bool inputMessage = mouseMessage || keyboardMessage || rawInputMessage || pointerMessage;
    if (!inputMessage) {
        return false;
    }

    if ((keyboardMessage || mouseMessage) && tane::camera::HandleZoomKeyMessage(message, wParam, lParam)) {
        UpdateMenuInputOwnership(hwnd, tane::gui::IsMenuOpen());
        return true;
    }

    if ((keyboardMessage || mouseMessage) && tane::camera::HandleFreeLookKeyMessage(message, wParam, lParam)) {
        UpdateMenuInputOwnership(hwnd, tane::gui::IsMenuOpen());
        return true;
    }

    if ((keyboardMessage || mouseMessage) && tane::gui::HandleTabKeyMessage(message, wParam, lParam)) {
        UpdateMenuInputOwnership(hwnd, tane::gui::IsMenuOpen());
        return true;
    }

    if (keyboardMessage && tane::gui::HandleMenuKeyMessage(message, wParam, lParam)) {
        UpdateMenuInputOwnership(hwnd, tane::gui::IsMenuOpen());
        return true;
    }

    if (!tane::gui::IsMenuOpen()) {
        UpdateMenuInputOwnership(hwnd, false);
        return false;
    }

    UpdateMenuInputOwnership(hwnd, true);
    if (forwardToImGui && (mouseMessage || keyboardMessage || pointerMessage)) {
        MSG msg{};
        msg.hwnd = hwnd;
        msg.message = message;
        msg.wParam = wParam;
        msg.lParam = lParam;
        ForwardMessageToImGui(msg, rawInputMessage);
    }

    return true;
}

bool ShouldTranslateConsumedKeyMessage(const MSG& message, bool forwardToImGui) {
    if (!forwardToImGui || !tane::gui::IsMenuOpen()) {
        return false;
    }

    return message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN;
}

bool ShouldConsumeRetrievedMessage(const MSG& message, bool forwardToImGui) {
    const bool consumed = ShouldConsumeInputMessage(message.hwnd, message.message, message.wParam, message.lParam, forwardToImGui);
    if (consumed && ShouldTranslateConsumedKeyMessage(message, forwardToImGui)) {
        TranslateMessage(&message);
    }

    return consumed;
}

BOOL WINAPI HookPeekMessageW(LPMSG message, HWND hwnd, UINT messageFilterMin, UINT messageFilterMax, UINT removeMsg) {
    if (g_originalPeekMessageW == nullptr) {
        return FALSE;
    }

    while (true) {
        const BOOL result = g_originalPeekMessageW(message, hwnd, messageFilterMin, messageFilterMax, removeMsg);
        if (!result || message == nullptr || message->message == WM_QUIT) {
            return result;
        }

        if (!ShouldConsumeRetrievedMessage(*message, (removeMsg & PM_REMOVE) != 0)) {
            return result;
        }

        if ((removeMsg & PM_REMOVE) == 0) {
            MSG removed{};
            while (g_originalPeekMessageW(&removed, hwnd, messageFilterMin, messageFilterMax, PM_REMOVE)) {
                if (removed.message == WM_QUIT) {
                    *message = removed;
                    return TRUE;
                }
                if (!ShouldConsumeRetrievedMessage(removed, true)) {
                    return FALSE;
                }
                break;
            }
            return FALSE;
        }
    }
}

BOOL WINAPI HookGetMessageW(LPMSG message, HWND hwnd, UINT messageFilterMin, UINT messageFilterMax) {
    if (g_originalGetMessageW == nullptr) {
        return FALSE;
    }

    while (true) {
        const BOOL result = g_originalGetMessageW(message, hwnd, messageFilterMin, messageFilterMax);
        if (result <= 0 || message == nullptr || message->message == WM_QUIT) {
            return result;
        }

        if (!ShouldConsumeRetrievedMessage(*message, true)) {
            return result;
        }
    }
}

UINT WINAPI HookGetRawInputData(HRAWINPUT rawInput, UINT command, LPVOID data, PUINT size, UINT headerSize) {
    if (tane::gui::IsMenuOpen()) {
        if (size != nullptr) {
            *size = 0;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return static_cast<UINT>(-1);
    }

    return g_originalGetRawInputData != nullptr
        ? g_originalGetRawInputData(rawInput, command, data, size, headerSize)
        : static_cast<UINT>(-1);
}

UINT WINAPI HookGetRawInputBuffer(PRAWINPUT data, PUINT size, UINT headerSize) {
    if (tane::gui::IsMenuOpen()) {
        if (size != nullptr) {
            *size = 0;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return static_cast<UINT>(-1);
    }

    return g_originalGetRawInputBuffer != nullptr
        ? g_originalGetRawInputBuffer(data, size, headerSize)
        : static_cast<UINT>(-1);
}

SHORT WINAPI HookGetAsyncKeyState(int virtualKey) {
    if (tane::gui::IsMenuOpen() && !ShouldAllowMenuVirtualKey(virtualKey)) {
        return 0;
    }

    return g_originalGetAsyncKeyState != nullptr ? g_originalGetAsyncKeyState(virtualKey) : 0;
}

SHORT WINAPI HookGetKeyState(int virtualKey) {
    if (tane::gui::IsMenuOpen() && !ShouldAllowMenuVirtualKey(virtualKey)) {
        return 0;
    }

    return g_originalGetKeyState != nullptr ? g_originalGetKeyState(virtualKey) : 0;
}

BOOL WINAPI HookSetCursorPos(int x, int y) {
    if (tane::gui::IsMenuOpen()) {
        return TRUE;
    }

    return g_originalSetCursorPos != nullptr ? g_originalSetCursorPos(x, y) : FALSE;
}

LRESULT CALLBACK HookWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ShouldConsumeInputMessage(hwnd, message, wParam, lParam, true)) {
        return 0;
    }

    if (g_originalWndProc != nullptr) {
        return CallWindowProcW(g_originalWndProc, hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
