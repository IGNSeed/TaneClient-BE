#include "Offsets.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tane::render {
namespace {

constexpr std::size_t kPatchSize = tane::offsets::render::kNameTagShortPatchSize;
using PatchBytes = std::array<std::uint8_t, kPatchSize>;

struct PatchSite {
    std::uintptr_t rva;
    PatchBytes expectedBytes;
    PatchBytes replacementBytes;
    PatchBytes originalBytes{};
    std::atomic_bool originalBytesReady = false;
    std::atomic_bool applied = false;
};

std::atomic_bool g_enabled = false;
std::atomic_bool g_configLoaded = false;

PatchSite g_thirdPersonSelfSkipPatch{
    0,
    {0x74, 0xD7},
    {0x90, 0x90},
};

bool BuildNameTagsConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\NameTags.json", renderPath) >= 0;
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

bool IsExecutableAddress(const void* address, std::size_t size) {
    if (address == nullptr || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(address);
    const auto regionBase = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto regionEnd = regionBase + info.RegionSize;
    const bool executable =
        (info.Protect & PAGE_EXECUTE) != 0 ||
        (info.Protect & PAGE_EXECUTE_READ) != 0 ||
        (info.Protect & PAGE_EXECUTE_READWRITE) != 0 ||
        (info.Protect & PAGE_EXECUTE_WRITECOPY) != 0;

    return base >= regionBase &&
        size <= regionEnd - base &&
        info.State == MEM_COMMIT &&
        executable &&
        (info.Protect & PAGE_GUARD) == 0;
}

bool EnsureOriginalBytes(PatchSite& patch) {
    if (patch.originalBytesReady.load(std::memory_order_acquire)) {
        return true;
    }

    const std::uint8_t* address = GetModuleAddress(patch.rva);
    if (!IsExecutableAddress(address, kPatchSize)) {
        return false;
    }

    PatchBytes currentBytes{};
    std::memcpy(currentBytes.data(), address, kPatchSize);
    if (currentBytes != patch.expectedBytes) {
        return false;
    }

    patch.originalBytes = currentBytes;
    patch.originalBytesReady.store(true, std::memory_order_release);
    return true;
}

bool WritePatchBytes(PatchSite& patch, const PatchBytes& bytes) {
    std::uint8_t* address = GetModuleAddress(patch.rva);
    if (!IsExecutableAddress(address, kPatchSize)) {
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(address, kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    std::memcpy(address, bytes.data(), kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), address, kPatchSize);

    DWORD ignored = 0;
    VirtualProtect(address, kPatchSize, oldProtect, &ignored);
    return true;
}

void ApplyPatchState(PatchSite& patch, bool enabled) {
    if (!EnsureOriginalBytes(patch)) {
        return;
    }

    if (enabled) {
        if (!patch.applied.load(std::memory_order_acquire) &&
            WritePatchBytes(patch, patch.replacementBytes)) {
            patch.applied.store(true, std::memory_order_release);
        }
        return;
    }

    if (patch.applied.load(std::memory_order_acquire) &&
        WritePatchBytes(patch, patch.originalBytes)) {
        patch.applied.store(false, std::memory_order_release);
    }
}

void ApplyNameTagsState(bool enabled) {
    if (g_thirdPersonSelfSkipPatch.rva == 0) {
        g_thirdPersonSelfSkipPatch.rva =
            tane::offsets::render::kNameTagThirdPersonSelfSkipPatchRva;
    }
    ApplyPatchState(g_thirdPersonSelfSkipPatch, enabled);
}

void SaveNameTagsConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildNameTagsConfigPath(path, MAX_PATH)) {
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

void EnsureNameTagsConfigLoaded() {
    bool expected = false;
    if (!g_configLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildNameTagsConfigPath(path, MAX_PATH)) {
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
            ApplyNameTagsState(enabled);
        }
    }

    CloseHandle(file);
}

}  // namespace

bool IsNameTagsEnabled() {
    EnsureNameTagsConfigLoaded();
    return g_enabled.load(std::memory_order_acquire);
}

void SetNameTagsEnabled(bool enabled) {
    EnsureNameTagsConfigLoaded();
    g_enabled.store(enabled, std::memory_order_release);
    ApplyNameTagsState(enabled);
    SaveNameTagsConfig();
}

void TickNameTags(void* clientInstance) {
    (void)clientInstance;
    EnsureNameTagsConfigLoaded();
    if (g_enabled.load(std::memory_order_acquire)) {
        ApplyNameTagsState(true);
    }
}

}  // namespace tane::render
