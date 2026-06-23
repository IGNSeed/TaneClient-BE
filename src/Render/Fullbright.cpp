#include "Offsets.h"

#include <Windows.h>
#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tane::render {
namespace {

using OptionsGetGammaFn = float(__fastcall*)(void* options, void* context);

std::atomic_bool g_enabled = false;
std::atomic_bool g_configLoaded = false;
std::atomic_bool g_hookInstalled = false;
OptionsGetGammaFn g_originalOptionsGetGamma = nullptr;

bool BuildFullbrightConfigPath(wchar_t* path, DWORD pathCount) {
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

    wchar_t renderPath[MAX_PATH]{};
    if (swprintf_s(renderPath, L"%s\\Render", configPath) < 0) {
        return false;
    }
    CreateDirectoryW(renderPath, nullptr);

    return swprintf_s(path, pathCount, L"%s\\Fullbright.json", renderPath) >= 0;
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

std::uint8_t* GetModuleAddress(std::uintptr_t rva) {
    if (rva == 0) {
        return nullptr;
    }
    HMODULE module = GetModuleHandleW(L"Minecraft.Windows.exe");
    if (module == nullptr) {
        module = GetModuleHandleW(nullptr);
    }

    if (module == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<std::uint8_t*>(module) + rva;
}

bool IsExecutableAddress(const void* address) {
    if (address == nullptr) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }

    const bool executable =
        (info.Protect & PAGE_EXECUTE) != 0 ||
        (info.Protect & PAGE_EXECUTE_READ) != 0 ||
        (info.Protect & PAGE_EXECUTE_READWRITE) != 0 ||
        (info.Protect & PAGE_EXECUTE_WRITECOPY) != 0;

    return info.State == MEM_COMMIT &&
        executable &&
        (info.Protect & PAGE_GUARD) == 0;
}

float __fastcall HookOptionsGetGamma(void* options, void* context) {
    float gamma = 1.0f;
    if (g_originalOptionsGetGamma != nullptr) {
        __try {
            gamma = g_originalOptionsGetGamma(options, context);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            gamma = 1.0f;
        }
    }

    if (g_enabled.load(std::memory_order_acquire)) {
        return tane::offsets::render::kFullbrightGamma;
    }

    return gamma;
}

bool EnsureFullbrightHookInstalled() {
    if (g_hookInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    void* target = GetModuleAddress(tane::offsets::render::kOptionsGetGammaRva);
    if (!IsExecutableAddress(target)) {
        return false;
    }

    MH_STATUS createStatus = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookOptionsGetGamma),
        reinterpret_cast<void**>(&g_originalOptionsGetGamma));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    MH_STATUS enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        return false;
    }

    g_hookInstalled.store(true, std::memory_order_release);
    return true;
}

void SaveFullbrightConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildFullbrightConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[128]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 1,\n"
        "  \"enabled\": %s\n"
        "}\n",
        g_enabled.load(std::memory_order_relaxed) ? "true" : "false");

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
}

void EnsureFullbrightConfigLoaded() {
    bool expected = false;
    if (!g_configLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildFullbrightConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char json[512]{};
    DWORD read = 0;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';

        bool enabled = false;
        if (ParseBoolAfter(json, "\"enabled\"", enabled)) {
            g_enabled.store(enabled, std::memory_order_relaxed);
        }
    }

    CloseHandle(file);
}

}  // namespace

bool IsFullbrightEnabled() {
    EnsureFullbrightConfigLoaded();
    if (g_enabled.load(std::memory_order_acquire)) {
        EnsureFullbrightHookInstalled();
    }
    return g_enabled.load(std::memory_order_acquire);
}

void SetFullbrightEnabled(bool enabled) {
    EnsureFullbrightConfigLoaded();
    if (enabled) {
        EnsureFullbrightHookInstalled();
    }
    g_enabled.store(enabled, std::memory_order_release);
    SaveFullbrightConfig();
}

void TickFullbright(void* clientInstance) {
    (void)clientInstance;
    EnsureFullbrightConfigLoaded();
    if (g_enabled.load(std::memory_order_acquire)) {
        EnsureFullbrightHookInstalled();
    }
}

}  // namespace tane::render
