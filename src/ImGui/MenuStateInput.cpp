bool ReadVirtualGamepadFlags(std::uint32_t& flags) {
    flags = 0;
#ifdef VK_GAMEPAD_LEFT_SHOULDER
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
    return flags != 0;
#else
    return false;
#endif
}

bool IsVirtualGamepadComboHeld(std::uint32_t requiredFlags) {
    std::uint32_t flags = 0;
    return ReadVirtualGamepadFlags(flags) && requiredFlags != 0 && (flags & requiredFlags) == requiredFlags;
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

bool ReadXInputControllerFlags(std::uint32_t& flags) {
    XInputGetStateFn getState = ResolveXInputGetState();
    if (getState == nullptr) {
        return false;
    }

    flags = 0;
    for (DWORD userIndex = 0; userIndex < 4; ++userIndex) {
        XInputState state{};
        if (getState(userIndex, &state) != ERROR_SUCCESS) {
            continue;
        }

        flags = GetControllerFlagsFromState(state.gamepad);
        if (flags != 0) {
            return true;
        }
    }

    return false;
}

bool ReadXInputPositionEditorState(float& moveX, float& moveY, float& resize, std::uint32_t& flags) {
    XInputGetStateFn getState = ResolveXInputGetState();
    if (getState == nullptr) {
        return false;
    }

    for (DWORD userIndex = 0; userIndex < 4; ++userIndex) {
        XInputState state{};
        if (getState(userIndex, &state) != ERROR_SUCCESS) {
            continue;
        }

        flags = GetControllerFlagsFromState(state.gamepad);
        constexpr float kThumbDeadZone = 0.22f;
        const float thumbX = static_cast<float>(state.gamepad.leftThumbX) / 32767.0f;
        const float thumbY = static_cast<float>(state.gamepad.leftThumbY) / 32767.0f;
        if (std::fabs(thumbX) > kThumbDeadZone) {
            moveX += thumbX;
        }
        if (std::fabs(thumbY) > kThumbDeadZone) {
            moveY -= thumbY;
        }
        const float leftTrigger = static_cast<float>(state.gamepad.leftTrigger) / 255.0f;
        const float rightTrigger = static_cast<float>(state.gamepad.rightTrigger) / 255.0f;
        if (leftTrigger > 0.12f) {
            resize -= leftTrigger;
        }
        if (rightTrigger > 0.12f) {
            resize += rightTrigger;
        }
        return flags != 0 || moveX != 0.0f || moveY != 0.0f || resize != 0.0f;
    }

    return false;
}

bool IsXInputControllerComboHeld(std::uint32_t requiredFlags) {
    std::uint32_t flags = 0;
    return ReadXInputControllerFlags(flags) && requiredFlags != 0 && (flags & requiredFlags) == requiredFlags;
}

void CaptureKeyboardCombo(WPARAM virtualKey) {
    (void)virtualKey;
    if (!HasKeyboardKeySet(g_keyboardCaptureCandidateKeys)) {
        return;
    }

    CopyKeyboardKeySet(g_keyboardComboKeys, g_keyboardCaptureCandidateKeys);
    g_keyboardCaptureActive = false;
    ClearKeyboardKeySet(g_keyboardCaptureCandidateKeys);
    ClearKeyboardKeySet(g_keyboardCaptureDownKeys);
    UpdateKeyboardComboLabel();
    SaveKeyComboConfig();
}

bool HasKeyboardCaptureDownKeys() {
    return HasKeyboardKeySet(g_keyboardCaptureDownKeys);
}

void UpdateRuntimeKeyboardKey(int virtualKey, bool down) {
    if (virtualKey > 0 && virtualKey < kVirtualKeyCount) {
        g_keyboardRuntimeDownKeys[virtualKey] = down;
    }
}

bool IsKeyboardRuntimeKeyDown(int virtualKey) {
    switch (virtualKey) {
    case VK_CONTROL:
        return g_keyboardRuntimeDownKeys[VK_CONTROL] ||
            g_keyboardRuntimeDownKeys[VK_LCONTROL] ||
            g_keyboardRuntimeDownKeys[VK_RCONTROL];
    case VK_SHIFT:
        return g_keyboardRuntimeDownKeys[VK_SHIFT] ||
            g_keyboardRuntimeDownKeys[VK_LSHIFT] ||
            g_keyboardRuntimeDownKeys[VK_RSHIFT];
    case VK_MENU:
        return g_keyboardRuntimeDownKeys[VK_MENU] ||
            g_keyboardRuntimeDownKeys[VK_LMENU] ||
            g_keyboardRuntimeDownKeys[VK_RMENU];
    default:
        break;
    }

    return virtualKey > 0 && virtualKey < kVirtualKeyCount && g_keyboardRuntimeDownKeys[virtualKey];
}

bool IsKeyboardComboTriggerKey(int triggerVirtualKey) {
    if (triggerVirtualKey <= 0 || triggerVirtualKey >= kVirtualKeyCount) {
        return false;
    }
    if (g_keyboardComboKeys[triggerVirtualKey]) {
        return true;
    }

    const int genericModifier = GetGenericModifierVirtualKey(triggerVirtualKey);
    return genericModifier != triggerVirtualKey && g_keyboardComboKeys[genericModifier];
}

bool IsKeyboardComboHeldByRuntimeState(int triggerVirtualKey) {
    if (!IsKeyboardComboTriggerKey(triggerVirtualKey)) {
        return false;
    }

    bool hasComboKey = false;
    for (int virtualKey = 1; virtualKey < kVirtualKeyCount; ++virtualKey) {
        if (!g_keyboardComboKeys[virtualKey]) {
            continue;
        }

        hasComboKey = true;
        if (!IsKeyboardRuntimeKeyDown(virtualKey)) {
            return false;
        }
    }

    return hasComboKey;
}

}  // namespace
