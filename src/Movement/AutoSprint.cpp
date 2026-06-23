#include <Windows.h>
#include <MinHook.h>
#include <imgui.h>

#include "Offsets.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tane::movement {
namespace {

using namespace tane::offsets::movement;

using SetMoveInputFlagFn = void(__fastcall*)(void* flags, int flag, bool enabled);
using GetLocalPlayerFn = void*(__fastcall*)(void* clientInstance);

constexpr int kMoveInputStateSprintDownFlag = 8;
constexpr int kMoveInputStateUpFlag = 13;
constexpr int kMoveInputStateDownFlag = 14;
constexpr int kMoveInputStateLeftFlag = 15;
constexpr int kMoveInputStateRightFlag = 16;
constexpr int kMoveInputComponentSprintingFlag = 1;
constexpr float kAnalogMoveThreshold = 0.01f;
constexpr std::size_t kMoveInputComponentFlagsOffset = 0x60;
constexpr ULONGLONG kCachedMoveInputFreshMs = 1000;
constexpr ULONGLONG kMoveInputResolveRetryMs = 250;
constexpr ULONGLONG kAutoSprintApplyIntervalMs = 50;

std::atomic_bool g_autoSprintEnabled = false;
std::atomic_bool g_autoSprintConfigLoaded = false;
std::atomic_bool g_hooksInstalled = false;
std::atomic<void*> g_moveInputComponent = nullptr;
std::atomic<void*> g_moveInputLocalPlayer = nullptr;
std::atomic<void*> g_moveInputLevel = nullptr;
std::atomic<ULONGLONG> g_moveInputLastSeenTick = 0;
std::atomic<ULONGLONG> g_moveInputLastResolveAttemptTick = 0;
std::atomic<ULONGLONG> g_autoSprintLastApplyTick = 0;
SetMoveInputFlagFn g_originalSetMoveInputFlag = nullptr;
SetMoveInputFlagFn g_setMoveInputComponentFlag = nullptr;

bool BuildAutoSprintConfigPath(wchar_t* path, DWORD pathCount) {
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

    wchar_t movementPath[MAX_PATH]{};
    if (swprintf_s(movementPath, L"%s\\Movement", configPath) < 0) {
        return false;
    }
    CreateDirectoryW(movementPath, nullptr);

    return swprintf_s(path, pathCount, L"%s\\AutoSprint.json", movementPath) >= 0;
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

void SaveAutoSprintConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildAutoSprintConfigPath(path, MAX_PATH)) {
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
        g_autoSprintEnabled.load(std::memory_order_relaxed) ? "true" : "false");

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
}

void EnsureAutoSprintConfigLoaded() {
    bool expected = false;
    if (!g_autoSprintConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildAutoSprintConfigPath(path, MAX_PATH)) {
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
            g_autoSprintEnabled.store(enabled, std::memory_order_relaxed);
        }
    }
    CloseHandle(file);
}

bool IsReadableAddress(const void* address, std::size_t size) {
    if (address == nullptr || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0 ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto regionEnd =
        reinterpret_cast<std::uintptr_t>(info.BaseAddress) + static_cast<std::uintptr_t>(info.RegionSize);
    return begin <= regionEnd && size <= regionEnd - begin;
}

bool IsExecutableAddress(const void* address) {
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr || VirtualQuery(address, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT) {
        return false;
    }

    const DWORD protection = info.Protect & 0xFF;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
bool ReadValue(const void* address, T& value) {
    if (address == nullptr) {
        return false;
    }

    __try {
        value = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool ReadOffset(const void* base, std::size_t offset, T& value) {
    if (base == nullptr) {
        return false;
    }

    return ReadValue(reinterpret_cast<const std::uint8_t*>(base) + offset, value);
}

bool LooksLikeActor(void* actor) {
    void** vtable = nullptr;
    void* entityContext = nullptr;
    std::uint32_t entityId = 0;
    return ReadValue(actor, vtable) &&
        vtable != nullptr &&
        ReadOffset(actor, kActorEntityContextOffset, entityContext) &&
        ReadOffset(actor, kActorEntityIdOffset, entityId) &&
        entityContext != nullptr &&
        entityId != 0;
}

bool ReadActorLevel(void* actor, void*& level) {
    level = nullptr;
    return LooksLikeActor(actor) &&
        ReadOffset(actor, tane::offsets::movement::kActorLevelOffset, level) &&
        level != nullptr;
}

void* GetImageAddress(std::uintptr_t imageBase, std::uintptr_t rva) {
    return imageBase != 0 && rva != 0 ? reinterpret_cast<void*>(imageBase + rva) : nullptr;
}

void* GetExecutableVfunc(void* object, std::size_t vtableOffset) {
    void** vtable = nullptr;
    void* function = nullptr;
    if (!ReadValue(object, vtable) ||
        !ReadOffset(vtable, vtableOffset, function) ||
        !IsExecutableAddress(function)) {
        return nullptr;
    }

    return function;
}

void* GetLocalPlayer(void* clientInstance) {
    void* function = GetExecutableVfunc(clientInstance, kClientInstanceLocalPlayerVtableOffset);
    if (function == nullptr) {
        return nullptr;
    }

    __try {
        void* localPlayer = reinterpret_cast<GetLocalPlayerFn>(function)(clientInstance);
        return LooksLikeActor(localPlayer) ? localPlayer : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool ResolvePackedComponentId(void* storage, std::uint32_t entityId, std::uint32_t& packedComponentId) {
    void* pageListBegin = nullptr;
    void* pageListEnd = nullptr;
    if (!ReadOffset(storage, 0x08, pageListBegin) || !ReadOffset(storage, 0x10, pageListEnd)) {
        return false;
    }

    const auto begin = reinterpret_cast<std::uintptr_t>(pageListBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(pageListEnd);
    if (end < begin) {
        return false;
    }

    const std::uint32_t sparseId = entityId & 0x3FFFF;
    const std::uintptr_t pageIndex = sparseId >> 11;
    const std::uintptr_t pageCount = (end - begin) / sizeof(void*);
    if (pageIndex >= pageCount) {
        return false;
    }

    void* page = nullptr;
    if (!ReadOffset(pageListBegin, pageIndex * sizeof(void*), page) || page == nullptr) {
        return false;
    }

    if (!ReadOffset(page, (sparseId & 0x7FF) * sizeof(std::uint32_t), packedComponentId)) {
        return false;
    }

    return ((entityId & 0xFFFC0000) ^ packedComponentId) <= 0x3FFFE;
}

void* ResolveComponentFromStorage(void* storage, std::uint32_t entityId) {
    std::uint32_t packedComponentId = 0;
    void* componentPages = nullptr;
    if (!ResolvePackedComponentId(storage, entityId, packedComponentId) ||
        !ReadOffset(storage, 0x50, componentPages)) {
        return nullptr;
    }

    const std::size_t pageOffset = ((packedComponentId >> 4) & 0x3FF8);
    void* componentPage = nullptr;
    if (!ReadOffset(componentPages, pageOffset, componentPage) || componentPage == nullptr) {
        return nullptr;
    }

    const std::size_t entryOffset = (packedComponentId & 0x7F) * kMoveInputComponentEntrySize;
    return reinterpret_cast<std::uint8_t*>(componentPage) + entryOffset;
}

void* ResolveMoveInputComponent(void* actor) {
    void* entityContext = nullptr;
    std::uint32_t entityId = 0;
    if (!ReadOffset(actor, kActorEntityContextOffset, entityContext) ||
        !ReadOffset(actor, kActorEntityIdOffset, entityId) ||
        entityContext == nullptr) {
        return nullptr;
    }

    void* hashBegin = nullptr;
    void* hashEnd = nullptr;
    void* hashEntries = nullptr;
    void* hashSentinel = nullptr;
    if (!ReadOffset(entityContext, 0x48, hashBegin) ||
        !ReadOffset(entityContext, 0x50, hashEnd) ||
        !ReadOffset(entityContext, 0x68, hashEntries) ||
        !ReadOffset(entityContext, 0x70, hashSentinel) ||
        hashBegin == nullptr ||
        hashEntries == nullptr) {
        return nullptr;
    }

    const auto begin = reinterpret_cast<std::uintptr_t>(hashBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(hashEnd);
    if (end <= begin) {
        return nullptr;
    }

    const std::uintptr_t slotCount = (end - begin) / sizeof(void*);
    if (slotCount == 0) {
        return nullptr;
    }

    void* slot = reinterpret_cast<void*>(begin + ((slotCount - 1) & kMoveInputComponentHash) * sizeof(void*));
    for (int attempt = 0; attempt < 64; ++attempt) {
        std::uint64_t entryIndex = 0;
        if (!ReadValue(slot, entryIndex) || entryIndex == UINT64_MAX) {
            return nullptr;
        }

        void* entry = reinterpret_cast<std::uint8_t*>(hashEntries) + entryIndex * 0x20;
        std::uint32_t hash = 0;
        if (!ReadOffset(entry, 0x08, hash)) {
            return nullptr;
        }

        if (hash == kMoveInputComponentHash) {
            if (entry == hashSentinel) {
                return nullptr;
            }

            void* storage = nullptr;
            return ReadOffset(entry, 0x10, storage) && storage != nullptr
                ? ResolveComponentFromStorage(storage, entityId)
                : nullptr;
        }

        slot = entry;
    }

    return nullptr;
}

bool TestBit(std::uint32_t value, int bit) {
    return (value & (1u << static_cast<unsigned>(bit))) != 0;
}

bool ReadMoveInputStateFlag(const void* state, int flag, bool& enabled) {
    enabled = false;
    if (state == nullptr || flag < 0) {
        return false;
    }

    std::uint32_t flags = 0;
    const std::size_t wordOffset = static_cast<std::size_t>(flag >> 5) * sizeof(std::uint32_t);
    if (!ReadOffset(state, wordOffset, flags)) {
        return false;
    }

    enabled = TestBit(flags, flag & 31);
    return true;
}

bool ReadMoveInputComponentFlag(const void* component, int flag, bool& enabled) {
    enabled = false;
    if (component == nullptr || flag < 0) {
        return false;
    }

    std::uint16_t flags = 0;
    const std::size_t wordOffset =
        kMoveInputComponentFlagsOffset + static_cast<std::size_t>(flag >> 4) * sizeof(std::uint16_t);
    if (!ReadOffset(component, wordOffset, flags)) {
        return false;
    }

    enabled = (flags & static_cast<std::uint16_t>(1u << static_cast<unsigned>(flag & 0xF))) != 0;
    return true;
}

bool IsAnalogMoving(const void* state) {
    float x = 0.0f;
    float y = 0.0f;
    if (!ReadOffset(state, 0x04, x) || !ReadOffset(state, 0x08, y)) {
        return false;
    }

    return x < -kAnalogMoveThreshold || x > kAnalogMoveThreshold ||
        y < -kAnalogMoveThreshold || y > kAnalogMoveThreshold;
}

bool IsMoveInputStateMoving(const void* state) {
    std::uint32_t flags = 0;
    if (!ReadValue(state, flags)) {
        return false;
    }

    return TestBit(flags, kMoveInputStateUpFlag) ||
        TestBit(flags, kMoveInputStateDownFlag) ||
        TestBit(flags, kMoveInputStateLeftFlag) ||
        TestBit(flags, kMoveInputStateRightFlag) ||
        IsAnalogMoving(state);
}

bool IsMoveInputComponentMoving(void* component) {
    if (component == nullptr) {
        return false;
    }

    const auto bytes = reinterpret_cast<const std::uint8_t*>(component);
    return IsMoveInputStateMoving(bytes) || IsMoveInputStateMoving(bytes + 0x10);
}

void SetMoveInputStateSprintDown(void* state, bool enabled) {
    if (g_originalSetMoveInputFlag == nullptr || state == nullptr) {
        return;
    }

    bool current = false;
    if (ReadMoveInputStateFlag(state, kMoveInputStateSprintDownFlag, current) && current == enabled) {
        return;
    }

    __try {
        g_originalSetMoveInputFlag(state, kMoveInputStateSprintDownFlag, enabled);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void SetMoveInputComponentSprinting(void* component, bool enabled) {
    if (g_setMoveInputComponentFlag == nullptr || component == nullptr) {
        return;
    }

    bool current = false;
    if (ReadMoveInputComponentFlag(component, kMoveInputComponentSprintingFlag, current) && current == enabled) {
        return;
    }

    __try {
        g_setMoveInputComponentFlag(component, kMoveInputComponentSprintingFlag, enabled);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void ApplyAutoSprint(void* component, bool enabled) {
    if (component == nullptr) {
        return;
    }

    SetMoveInputStateSprintDown(component, enabled);
    SetMoveInputStateSprintDown(reinterpret_cast<std::uint8_t*>(component) + 0x10, enabled);
    SetMoveInputComponentSprinting(component, enabled);
}

void ClearCachedMoveInput() {
    g_moveInputComponent.store(nullptr, std::memory_order_relaxed);
    g_moveInputLocalPlayer.store(nullptr, std::memory_order_relaxed);
    g_moveInputLevel.store(nullptr, std::memory_order_relaxed);
    g_moveInputLastSeenTick.store(0, std::memory_order_relaxed);
    g_autoSprintLastApplyTick.store(0, std::memory_order_relaxed);
}

bool IsCachedMoveInputFresh() {
    const ULONGLONG lastSeen = g_moveInputLastSeenTick.load(std::memory_order_relaxed);
    if (lastSeen == 0 || GetTickCount64() - lastSeen > kCachedMoveInputFreshMs) {
        ClearCachedMoveInput();
        return false;
    }

    void* localPlayer = g_moveInputLocalPlayer.load(std::memory_order_relaxed);
    void* expectedLevel = g_moveInputLevel.load(std::memory_order_relaxed);
    void* currentLevel = nullptr;
    if (localPlayer == nullptr ||
        expectedLevel == nullptr ||
        !ReadActorLevel(localPlayer, currentLevel) ||
        currentLevel != expectedLevel) {
        ClearCachedMoveInput();
        return false;
    }

    return true;
}

void SyncAutoSprintForCachedComponent() {
    void* component = g_moveInputComponent.load(std::memory_order_relaxed);
    if (component == nullptr ||
        !g_autoSprintEnabled.load(std::memory_order_relaxed) ||
        !IsCachedMoveInputFresh()) {
        return;
    }

    ApplyAutoSprint(component, IsMoveInputComponentMoving(component));
    g_autoSprintLastApplyTick.store(GetTickCount64(), std::memory_order_relaxed);
}

bool IsCachedMoveInputState(void* state) {
    void* component = g_moveInputComponent.load(std::memory_order_relaxed);
    if (component == nullptr || state == nullptr || !IsCachedMoveInputFresh()) {
        return false;
    }

    const auto componentBytes = reinterpret_cast<std::uint8_t*>(component);
    return state == componentBytes || state == componentBytes + 0x10;
}

void __fastcall HookSetMoveInputFlag(void* flags, int flag, bool enabled) {
    if (g_originalSetMoveInputFlag == nullptr) {
        return;
    }

    g_originalSetMoveInputFlag(flags, flag, enabled);

    if (g_autoSprintEnabled.load(std::memory_order_relaxed) &&
        IsCachedMoveInputState(flags) &&
        (flag == kMoveInputStateUpFlag ||
            flag == kMoveInputStateDownFlag ||
            flag == kMoveInputStateLeftFlag ||
            flag == kMoveInputStateRightFlag ||
            flag == kMoveInputStateSprintDownFlag)) {
        SyncAutoSprintForCachedComponent();
    }
}

}  // namespace

bool InstallAutoSprintHooks() {
    EnsureAutoSprintConfigLoaded();
    if (g_hooksInstalled.load(std::memory_order_relaxed)) {
        return true;
    }

    HMODULE module = GetModuleHandleW(nullptr);
    const auto imageBase = reinterpret_cast<std::uintptr_t>(module);
    if (module == nullptr || imageBase == 0) {
        return false;
    }

    void* setMoveInputFlag = GetImageAddress(imageBase, kMoveInputStateSetFlagRva);
    if (!IsExecutableAddress(setMoveInputFlag)) {
        return false;
    }

    void* setMoveInputComponentFlag = GetImageAddress(imageBase, kMoveInputComponentSetFlagRva);
    if (!IsExecutableAddress(setMoveInputComponentFlag)) {
        return false;
    }
    g_setMoveInputComponentFlag = reinterpret_cast<SetMoveInputFlagFn>(setMoveInputComponentFlag);

    MH_STATUS createStatus = MH_CreateHook(
        setMoveInputFlag,
        &HookSetMoveInputFlag,
        reinterpret_cast<void**>(&g_originalSetMoveInputFlag));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    MH_STATUS enableStatus = MH_EnableHook(setMoveInputFlag);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        return false;
    }

    g_hooksInstalled.store(true, std::memory_order_relaxed);
    return true;
}

void TickAutoSprint(void* clientInstance) {
    EnsureAutoSprintConfigLoaded();
    if (clientInstance == nullptr || !g_autoSprintEnabled.load(std::memory_order_relaxed)) {
        ClearCachedMoveInput();
        return;
    }

    const ULONGLONG now = GetTickCount64();
    if (IsCachedMoveInputFresh()) {
        const ULONGLONG lastApply = g_autoSprintLastApplyTick.load(std::memory_order_relaxed);
        if (lastApply != 0 && now - lastApply < kAutoSprintApplyIntervalMs) {
            return;
        }

        SyncAutoSprintForCachedComponent();
        return;
    }

    const ULONGLONG lastResolveAttempt = g_moveInputLastResolveAttemptTick.load(std::memory_order_relaxed);
    if (lastResolveAttempt != 0 && now - lastResolveAttempt < kMoveInputResolveRetryMs) {
        return;
    }
    g_moveInputLastResolveAttemptTick.store(now, std::memory_order_relaxed);

    void* localPlayer = GetLocalPlayer(clientInstance);
    void* level = nullptr;
    if (!ReadActorLevel(localPlayer, level)) {
        ClearCachedMoveInput();
        return;
    }

    void* component = ResolveMoveInputComponent(localPlayer);
    if (component == nullptr) {
        ClearCachedMoveInput();
        return;
    }

    g_moveInputComponent.store(component, std::memory_order_relaxed);
    g_moveInputLocalPlayer.store(localPlayer, std::memory_order_relaxed);
    g_moveInputLevel.store(level, std::memory_order_relaxed);
    g_moveInputLastSeenTick.store(now, std::memory_order_relaxed);
    ApplyAutoSprint(component, IsMoveInputComponentMoving(component));
    g_autoSprintLastApplyTick.store(now, std::memory_order_relaxed);
}

void RenderAutoSprintControls() {
    EnsureAutoSprintConfigLoaded();
    bool autoSprintEnabled = g_autoSprintEnabled.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Auto Sprint", &autoSprintEnabled)) {
        g_autoSprintEnabled.store(autoSprintEnabled, std::memory_order_relaxed);
        if (!autoSprintEnabled) {
            ClearCachedMoveInput();
        }
        SaveAutoSprintConfig();
    }
}

bool IsAutoSprintEnabled() {
    EnsureAutoSprintConfigLoaded();
    return g_autoSprintEnabled.load(std::memory_order_relaxed);
}

void SetAutoSprintEnabled(bool enabled) {
    EnsureAutoSprintConfigLoaded();
    g_autoSprintEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        ClearCachedMoveInput();
    }
    SaveAutoSprintConfig();
}

}  // namespace tane::movement
