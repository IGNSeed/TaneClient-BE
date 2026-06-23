#include "Offsets.h"

#include <Windows.h>
#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tane::render {
namespace {

using VolumetricFogRenderFn = void(__fastcall*)(void* self, void* frameContext, void* frameData);
using LevelRendererPlayerFogRenderUpdateFn = void(__fastcall*)(void* self, void* frameData, float interpolation);

std::atomic_bool g_enabled = false;
std::atomic_bool g_configLoaded = false;
std::atomic_bool g_volumetricFogHookInstalled = false;
std::atomic_bool g_levelRendererPlayerFogRenderUpdateHookInstalled = false;

VolumetricFogRenderFn g_originalVolumetricFogRender = nullptr;
LevelRendererPlayerFogRenderUpdateFn g_originalLevelRendererPlayerFogRenderUpdate = nullptr;

constexpr float kNoFogFinalDistance = 1048576.0f;

bool BuildNoFogConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\NoFog.json", renderPath) >= 0;
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

    return module != nullptr ? reinterpret_cast<std::uint8_t*>(module) + rva : nullptr;
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

void SetFloat(void* address, float value) {
    std::memcpy(address, &value, sizeof(value));
}

void SetUInt32(void* address, std::uint32_t value) {
    std::memcpy(address, &value, sizeof(value));
}

void PatchFinalFogState(void* self) {
    if (self == nullptr || !g_enabled.load(std::memory_order_acquire)) {
        return;
    }

    auto* base = reinterpret_cast<std::uint8_t*>(self);

    SetFloat(base + 0x4E4, kNoFogFinalDistance);
    SetFloat(base + 0x4E8, kNoFogFinalDistance);
    SetUInt32(base + 0x4EC, 0);
    SetFloat(base + 0x500, kNoFogFinalDistance);
    SetFloat(base + 0x504, kNoFogFinalDistance);
    SetUInt32(base + 0x508, 0);
    SetFloat(base + 0x588, 0.0f);

    std::memset(base + 0x50C, 0, 0x10);
    std::memset(base + 0x51C, 0, 0x20);
    std::memset(base + 0x53C, 0, 0x20);
    std::memset(base + 0x55C, 0, 0x20);
    SetFloat(base + 0x57C, 0.0f);
    SetFloat(base + 0x580, 0.0f);
}

void __fastcall HookLevelRendererPlayerFogRenderUpdate(void* self, void* frameData, float interpolation) {
    if (g_originalLevelRendererPlayerFogRenderUpdate != nullptr) {
        g_originalLevelRendererPlayerFogRenderUpdate(self, frameData, interpolation);
    }

    PatchFinalFogState(self);
}

void __fastcall HookVolumetricFogRender(void* self, void* frameContext, void* frameData) {
    if (g_enabled.load(std::memory_order_acquire)) {
        return;
    }

    if (g_originalVolumetricFogRender != nullptr) {
        g_originalVolumetricFogRender(self, frameContext, frameData);
    }
}

bool InstallHook(std::uintptr_t rva, void* hook, void** original) {
    void* target = GetModuleAddress(rva);
    if (!IsExecutableAddress(target)) {
        return false;
    }

    const MH_STATUS createStatus = MH_CreateHook(target, hook, original);
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    return enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED;
}

bool EnsureVolumetricFogHookInstalled() {
    if (g_volumetricFogHookInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    if (!InstallHook(
            tane::offsets::render::kVolumetricFogRenderRva,
            reinterpret_cast<void*>(&HookVolumetricFogRender),
            reinterpret_cast<void**>(&g_originalVolumetricFogRender))) {
        return false;
    }

    g_volumetricFogHookInstalled.store(true, std::memory_order_release);
    return true;
}

bool EnsureLevelRendererPlayerFogRenderUpdateHookInstalled() {
    if (g_levelRendererPlayerFogRenderUpdateHookInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    if (!InstallHook(
            tane::offsets::render::kLevelRendererPlayerFogRenderUpdateRva,
            reinterpret_cast<void*>(&HookLevelRendererPlayerFogRenderUpdate),
            reinterpret_cast<void**>(&g_originalLevelRendererPlayerFogRenderUpdate))) {
        return false;
    }

    g_levelRendererPlayerFogRenderUpdateHookInstalled.store(true, std::memory_order_release);
    return true;
}

bool EnsureNoFogHookInstalled() {
    const bool volumetricHookInstalled = EnsureVolumetricFogHookInstalled();
    const bool fogRenderUpdateHookInstalled = EnsureLevelRendererPlayerFogRenderUpdateHookInstalled();
    return volumetricHookInstalled && fogRenderUpdateHookInstalled;
}

void SaveNoFogConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildNoFogConfigPath(path, MAX_PATH)) {
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

void EnsureNoFogConfigLoaded() {
    bool expected = false;
    if (!g_configLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildNoFogConfigPath(path, MAX_PATH)) {
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

bool IsNoFogEnabled() {
    EnsureNoFogConfigLoaded();
    if (g_enabled.load(std::memory_order_acquire)) {
        EnsureNoFogHookInstalled();
    }
    return g_enabled.load(std::memory_order_acquire);
}

void SetNoFogEnabled(bool enabled) {
    EnsureNoFogConfigLoaded();
    if (enabled) {
        EnsureNoFogHookInstalled();
    }
    g_enabled.store(enabled, std::memory_order_release);
    SaveNoFogConfig();
}

void TickNoFog(void* clientInstance) {
    (void)clientInstance;
    EnsureNoFogConfigLoaded();
    if (g_enabled.load(std::memory_order_acquire)) {
        EnsureNoFogHookInstalled();
    }
}

}  // namespace tane::render
