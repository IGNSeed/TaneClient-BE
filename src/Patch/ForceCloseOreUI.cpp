#include "Offsets.h"

#include <Windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>

namespace tane::patch {
namespace {

using IsOreUiSettingsTabEnabledFn = bool(__fastcall*)(std::uint64_t tabIndex);
using PlayScreenTechStackOreUiPredicateFn = bool(__fastcall*)(void* context);
using UseLegacyPlayScreenFn = bool(__fastcall*)(void* context);
using RegisterLegacyPlayRouteFn = void*(__fastcall*)(
    void* callback,
    void* routeProvider,
    void* sceneFactory,
    void* sceneStackNonOwner);
using NavigateSceneRouteFn =
    std::int64_t(__fastcall*)(void* router, void* routeRequest, int mode);
using ExtractSceneRouteFn =
    void(__fastcall*)(void* routeProvider, std::string* result, void* routeRequest);
using InitializeOreUiConfigRegistryFn = std::int64_t(__fastcall*)(
    void* registry,
    void* screenTechStack,
    std::uint8_t flag1,
    std::uint8_t flag2,
    void* callback1,
    void* callback2);

struct OreUiConfig {
    void* unknown1;
    void* unknown2;
    std::function<bool()> predicate1;
    std::function<bool()> predicate2;
};

struct OreUiConfigRegistry {
    std::unordered_map<std::string, OreUiConfig> configs;
};

struct SavedOreUiPredicates {
    std::function<bool()> predicate1;
    std::function<bool()> predicate2;
};

static_assert(sizeof(OreUiConfig) == 0x90);

std::atomic_bool g_enabled = false;
std::atomic_bool g_configLoaded = false;
std::atomic_bool g_hooksInstalled = false;
std::atomic_bool g_hookInstallAttempted = false;
std::atomic<void*> g_instrumentedClientInstance = nullptr;
std::atomic<void*> g_legacyPlayRegisteredProvider = nullptr;
std::atomic<void*> g_legacyPlayRegistrationInProgress = nullptr;

IsOreUiSettingsTabEnabledFn g_originalIsOreUiSettingsTabEnabled = nullptr;
PlayScreenTechStackOreUiPredicateFn g_originalPlayScreenTechStackOreUiPredicate = nullptr;
UseLegacyPlayScreenFn g_originalUseLegacyPlayScreen = nullptr;
NavigateSceneRouteFn g_originalNavigateSceneRoute = nullptr;
InitializeOreUiConfigRegistryFn g_originalInitializeOreUiConfigRegistry = nullptr;
SRWLOCK g_registryLock = SRWLOCK_INIT;
OreUiConfigRegistry* g_registry = nullptr;
std::unordered_map<std::string, SavedOreUiPredicates> g_originalPredicates;

constexpr std::uint8_t kIsOreUiSettingsTabEnabledPattern[] = {
    0x55, 0x41, 0x56, 0x56, 0x57, 0x53, 0x48, 0x83,
    0xEC, 0x50, 0x48, 0x8D, 0x6C, 0x24, 0x50, 0x48,
};

constexpr std::uint8_t kInitializeOreUiConfigRegistryPattern[] = {
    0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41,
    0x54, 0x56, 0x57, 0x53, 0x48, 0x81, 0xEC, 0xD8,
    0x01, 0x00, 0x00, 0x48, 0x8D, 0xAC, 0x24, 0x80,
    0x00, 0x00, 0x00,
};

constexpr std::uint8_t kPlayScreenTechStackOreUiPredicatePattern[] = {
    0x0F, 0xB6, 0x41, 0x09, 0x0A, 0x41, 0x08, 0xF6,
    0xD0, 0x24, 0x01, 0xC3,
};

constexpr std::uint8_t kUseLegacyPlayScreenPattern[] = {
    0x55, 0x56, 0x48, 0x83, 0xEC, 0x58, 0x48, 0x8D,
    0x6C, 0x24, 0x50, 0x48, 0xC7, 0x45, 0x00, 0xFE,
};

constexpr std::uint8_t kNavigateSceneRoutePattern[] = {
    0x55, 0x41, 0x57, 0x41, 0x56, 0x56, 0x57, 0x53,
    0x48, 0x81, 0xEC, 0x38, 0x01, 0x00, 0x00, 0x48,
    0x8D, 0xAC, 0x24, 0x80, 0x00, 0x00, 0x00,
};

constexpr std::uint8_t kRegisterLegacyPlayRoutePattern[] = {
    0x55, 0x41, 0x57, 0x41, 0x56, 0x56, 0x57, 0x53,
    0x48, 0x81, 0xEC, 0xA8, 0x01, 0x00, 0x00,
};

bool BuildConfigPath(wchar_t* path, DWORD pathCount) {
    if (path == nullptr || pathCount == 0) {
        return false;
    }

    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    if (tempLength == 0 || tempLength >= MAX_PATH) {
        return false;
    }

    wchar_t directory[MAX_PATH]{};
    if (swprintf_s(directory, L"%sTaneClient", tempPath) < 0) {
        return false;
    }
    CreateDirectoryW(directory, nullptr);

    if (swprintf_s(directory, L"%sTaneClient\\Config", tempPath) < 0) {
        return false;
    }
    CreateDirectoryW(directory, nullptr);

    if (swprintf_s(directory, L"%sTaneClient\\Config\\Patch", tempPath) < 0) {
        return false;
    }
    CreateDirectoryW(directory, nullptr);

    return swprintf_s(
        path,
        pathCount,
        L"%sTaneClient\\Config\\Patch\\ForceCloseOreUI.json",
        tempPath) >= 0;
}

void LoadConfig() {
    bool expected = false;
    if (!g_configLoaded.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char json[256]{};
    DWORD bytesRead = 0;
    const bool readSucceeded =
        ReadFile(file, json, static_cast<DWORD>(sizeof(json) - 1), &bytesRead, nullptr) != FALSE;
    CloseHandle(file);
    if (!readSucceeded) {
        return;
    }

    json[bytesRead] = '\0';
    const char* enabled = std::strstr(json, "\"enabled\"");
    if (enabled == nullptr) {
        return;
    }

    enabled = std::strchr(enabled, ':');
    if (enabled == nullptr) {
        return;
    }

    while (*++enabled == ' ' || *enabled == '\t') {
    }
    if (std::strncmp(enabled, "true", 4) == 0) {
        g_enabled.store(true, std::memory_order_release);
    } else if (std::strncmp(enabled, "false", 5) == 0) {
        g_enabled.store(false, std::memory_order_release);
    }
}

void SaveConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char json[96]{};
    const int length = std::snprintf(
        json,
        sizeof(json),
        "{\n  \"version\": 1,\n  \"enabled\": %s\n}\n",
        g_enabled.load(std::memory_order_relaxed) ? "true" : "false");
    if (length > 0) {
        DWORD bytesWritten = 0;
        WriteFile(file, json, static_cast<DWORD>(length), &bytesWritten, nullptr);
    }
    CloseHandle(file);
}

void* GetModuleAddress(std::uintptr_t rva) {
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
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr || VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }

    constexpr DWORD kExecutableProtection =
        PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return info.State == MEM_COMMIT &&
        (info.Protect & kExecutableProtection) != 0 &&
        (info.Protect & PAGE_GUARD) == 0;
}

bool IsReadableAddress(const void* address, std::size_t size) {
    if (address == nullptr || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    constexpr DWORD kReadableProtection =
        PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto regionEnd =
        reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return (info.Protect & kReadableProtection) != 0 &&
        start <= regionEnd &&
        size <= regionEnd - start;
}

template <typename T>
bool ReadOffset(const void* base, std::size_t offset, T& value) {
    const auto* address = reinterpret_cast<const std::uint8_t*>(base) + offset;
    if (!IsReadableAddress(address, sizeof(T))) {
        return false;
    }

    __try {
        std::memcpy(&value, address, sizeof(T));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void ApplyRegistryStateLocked(bool enabled) {
    if (g_registry == nullptr) {
        return;
    }

    for (auto& [name, config] : g_registry->configs) {
        if (enabled) {
            config.predicate1 = []() { return false; };
            config.predicate2 = []() { return false; };
            continue;
        }

        const auto original = g_originalPredicates.find(name);
        if (original != g_originalPredicates.end()) {
            config.predicate1 = original->second.predicate1;
            config.predicate2 = original->second.predicate2;
        }
    }
}

void CaptureAndApplyRegistry(OreUiConfigRegistry* registry) {
    if (registry == nullptr) {
        return;
    }

    AcquireSRWLockExclusive(&g_registryLock);
    if (g_registry != registry) {
        g_registry = registry;
        g_originalPredicates.clear();
        g_originalPredicates.reserve(registry->configs.size());
        for (const auto& [name, config] : registry->configs) {
            g_originalPredicates.emplace(
                name,
                SavedOreUiPredicates{config.predicate1, config.predicate2});
        }
    }
    ApplyRegistryStateLocked(g_enabled.load(std::memory_order_relaxed));
    ReleaseSRWLockExclusive(&g_registryLock);
}

void ApplyRegistryState(bool enabled) {
    AcquireSRWLockExclusive(&g_registryLock);
    ApplyRegistryStateLocked(enabled);
    ReleaseSRWLockExclusive(&g_registryLock);
}

void ResetRegistryState() {
    AcquireSRWLockExclusive(&g_registryLock);
    g_registry = nullptr;
    g_originalPredicates.clear();
    ReleaseSRWLockExclusive(&g_registryLock);
}

void TryInstrumentRegistry(void* clientInstance) {
    if (clientInstance == nullptr) {
        if (g_instrumentedClientInstance.exchange(nullptr, std::memory_order_acq_rel) != nullptr) {
            ResetRegistryState();
        }
        return;
    }
    if (g_instrumentedClientInstance.load(std::memory_order_acquire) == clientInstance) {
        return;
    }

    OreUiConfigRegistry* registry = nullptr;
    if (!ReadOffset(
            clientInstance,
            tane::offsets::patch::kClientInstanceOreUiConfigRegistryOffset,
            registry) ||
        !IsReadableAddress(registry, sizeof(OreUiConfigRegistry))) {
        return;
    }

    try {
        CaptureAndApplyRegistry(registry);
        g_instrumentedClientInstance.store(clientInstance, std::memory_order_release);
    } catch (...) {
        ResetRegistryState();
    }
}

bool __fastcall HookIsOreUiSettingsTabEnabled(std::uint64_t tabIndex) {
    if (g_enabled.load(std::memory_order_relaxed)) {
        return false;
    }
    return g_originalIsOreUiSettingsTabEnabled != nullptr
        ? g_originalIsOreUiSettingsTabEnabled(tabIndex)
        : false;
}

bool __fastcall HookPlayScreenTechStackOreUiPredicate(void* context) {
    if (g_enabled.load(std::memory_order_relaxed)) {
        return false;
    }
    return g_originalPlayScreenTechStackOreUiPredicate != nullptr
        ? g_originalPlayScreenTechStackOreUiPredicate(context)
        : false;
}

bool __fastcall HookUseLegacyPlayScreen(void* context) {
    if (g_enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    return g_originalUseLegacyPlayScreen != nullptr
        ? g_originalUseLegacyPlayScreen(context)
        : false;
}

std::string ExtractSceneRoute(void* router, void* routeRequest) {
    void* routeProvider = nullptr;
    if (!ReadOffset(
            router,
            tane::offsets::patch::kSceneRouterRouteProviderOffset,
            routeProvider) ||
        !IsReadableAddress(routeProvider, sizeof(void*))) {
        return {};
    }

    void** vtable = nullptr;
    if (!ReadOffset(routeProvider, 0, vtable) ||
        !IsReadableAddress(
            vtable + tane::offsets::patch::kRouteProviderExtractRouteVtableIndex,
            sizeof(void*))) {
        return {};
    }

    const auto extractRoute = reinterpret_cast<ExtractSceneRouteFn>(
        vtable[tane::offsets::patch::kRouteProviderExtractRouteVtableIndex]);
    if (!IsExecutableAddress(reinterpret_cast<void*>(extractRoute))) {
        return {};
    }

    std::string route;
    extractRoute(routeProvider, &route, routeRequest);
    return route;
}

bool IsPlayRoute(const std::string& route) {
    constexpr char kPlayRoute[] = "/play";
    if (route.compare(0, sizeof(kPlayRoute) - 1, kPlayRoute) != 0) {
        return false;
    }
    return route.size() == sizeof(kPlayRoute) - 1 ||
        route[sizeof(kPlayRoute) - 1] == '/' ||
        route[sizeof(kPlayRoute) - 1] == '?';
}

bool ReadStringData(
    const void* stringObject,
    const char*& data,
    std::size_t& length) {
    data = nullptr;
    length = 0;

    std::size_t capacity = 0;
    if (!ReadOffset(stringObject, 0x10, length) ||
        !ReadOffset(stringObject, 0x18, capacity) ||
        length > 512 ||
        capacity < length) {
        return false;
    }

    if (capacity < 0x10) {
        data = static_cast<const char*>(stringObject);
    } else if (!ReadOffset(stringObject, 0, data)) {
        return false;
    }
    return IsReadableAddress(data, length + 1);
}

bool IsStoredRoute(const void* stringObject, const char* route) {
    const char* data = nullptr;
    std::size_t length = 0;
    if (!ReadStringData(stringObject, data, length)) {
        return false;
    }

    const std::size_t routeLength = std::strlen(route);
    return length == routeLength && std::memcmp(data, route, routeLength) == 0;
}

bool IsStoredPlayRoute(const void* stringObject) {
    const char* data = nullptr;
    std::size_t length = 0;
    if (!ReadStringData(stringObject, data, length) || length < 5) {
        return false;
    }
    return std::memcmp(data, "/play", 5) == 0 &&
        (length == 5 || data[5] == '/' || data[5] == '?');
}

bool GetRouteProviderEntries(
    void* routeProvider,
    std::uint8_t*& begin,
    std::uint8_t*& end) {
    begin = nullptr;
    end = nullptr;
    if (!ReadOffset(
            routeProvider,
            tane::offsets::patch::kRouteProviderRoutesBeginOffset,
            begin) ||
        !ReadOffset(
            routeProvider,
            tane::offsets::patch::kRouteProviderRoutesEndOffset,
            end) ||
        begin == nullptr ||
        end < begin) {
        return false;
    }

    const std::size_t span = static_cast<std::size_t>(end - begin);
    return span % tane::offsets::patch::kRouteProviderRouteEntrySize == 0 &&
        span <= tane::offsets::patch::kRouteProviderRouteEntrySize * 512 &&
        (span == 0 || IsReadableAddress(begin, span));
}

bool HasRegisteredRoute(void* routeProvider, const char* route) {
    std::uint8_t* begin = nullptr;
    std::uint8_t* end = nullptr;
    if (!GetRouteProviderEntries(routeProvider, begin, end)) {
        return false;
    }

    for (auto* entry = begin; entry != end;
         entry += tane::offsets::patch::kRouteProviderRouteEntrySize) {
        if (IsStoredRoute(
                entry + tane::offsets::patch::kRouteProviderRoutePathOffset,
                route)) {
            return true;
        }
    }
    return false;
}

void* FindPlaySceneFactory(void* routeProvider) {
    std::uint8_t* begin = nullptr;
    std::uint8_t* end = nullptr;
    if (!GetRouteProviderEntries(routeProvider, begin, end)) {
        return nullptr;
    }

    for (auto* entry = begin; entry != end;
         entry += tane::offsets::patch::kRouteProviderRouteEntrySize) {
        if (!IsStoredPlayRoute(
                entry + tane::offsets::patch::kRouteProviderRoutePathOffset)) {
            continue;
        }

        void* factoryTarget = nullptr;
        void* factoryVtable = nullptr;
        void* sceneFactory = nullptr;
        if (ReadOffset(
                entry,
                tane::offsets::patch::kRouteProviderRouteFactoryTargetOffset,
                factoryTarget) &&
            IsReadableAddress(factoryTarget, 0x10) &&
            ReadOffset(factoryTarget, 0, factoryVtable) &&
            factoryVtable == GetModuleAddress(
                tane::offsets::patch::kRouteFactoryWrapperVtableRva) &&
            ReadOffset(
                factoryTarget,
                tane::offsets::patch::kRouteFactorySceneFactoryOffset,
                sceneFactory) &&
            IsReadableAddress(sceneFactory, sizeof(void*))) {
            return sceneFactory;
        }
    }
    return nullptr;
}

void TryRegisterLegacyPlayRoute(void* router) {
    if (router == nullptr) {
        return;
    }

    void* routeProvider = nullptr;
    if (!ReadOffset(
            router,
            tane::offsets::patch::kSceneRouterRouteProviderOffset,
            routeProvider) ||
        !IsReadableAddress(routeProvider, 0x50)) {
        return;
    }
    if (g_legacyPlayRegisteredProvider.load(std::memory_order_acquire) == routeProvider ||
        HasRegisteredRoute(routeProvider, "/legacy-play")) {
        g_legacyPlayRegisteredProvider.store(routeProvider, std::memory_order_release);
        return;
    }

    void* expected = nullptr;
    if (!g_legacyPlayRegistrationInProgress.compare_exchange_strong(
            expected,
            routeProvider,
            std::memory_order_acq_rel)) {
        return;
    }

    void* sceneFactory = FindPlaySceneFactory(routeProvider);
    auto* sceneStackNonOwner =
        static_cast<std::uint8_t*>(router) +
        tane::offsets::patch::kSceneRouterSceneStackNonOwnerOffset;
    auto registerLegacyPlayRoute = reinterpret_cast<RegisterLegacyPlayRouteFn>(
        GetModuleAddress(tane::offsets::patch::kRegisterLegacyPlayRouteRva));
    bool registered = false;
    if (sceneFactory != nullptr &&
        IsReadableAddress(sceneStackNonOwner, 0x18) &&
        IsExecutableAddress(reinterpret_cast<void*>(registerLegacyPlayRoute)) &&
        std::memcmp(
            reinterpret_cast<void*>(registerLegacyPlayRoute),
            kRegisterLegacyPlayRoutePattern,
            sizeof(kRegisterLegacyPlayRoutePattern)) == 0) {
        __try {
            registerLegacyPlayRoute(
                nullptr,
                routeProvider,
                sceneFactory,
                sceneStackNonOwner);
            registered = HasRegisteredRoute(routeProvider, "/legacy-play");
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            registered = false;
        }
    }

    if (registered) {
        g_legacyPlayRegisteredProvider.store(routeProvider, std::memory_order_release);
    }
    g_legacyPlayRegistrationInProgress.store(nullptr, std::memory_order_release);
}

std::int64_t __fastcall HookNavigateSceneRoute(
    void* router,
    void* routeRequest,
    int mode) {
    if (g_enabled.load(std::memory_order_relaxed)) {
        void* routeProvider = nullptr;
        const bool needsRegistration =
            ReadOffset(
                router,
                tane::offsets::patch::kSceneRouterRouteProviderOffset,
                routeProvider) &&
            routeProvider != nullptr &&
            g_legacyPlayRegisteredProvider.load(std::memory_order_acquire) !=
                routeProvider;
        if (needsRegistration && IsPlayRoute(ExtractSceneRoute(router, routeRequest))) {
            TryRegisterLegacyPlayRoute(router);
        }
    }
    return g_originalNavigateSceneRoute != nullptr
        ? g_originalNavigateSceneRoute(router, routeRequest, mode)
        : 0;
}

std::int64_t __fastcall HookInitializeOreUiConfigRegistry(
    void* registry,
    void* screenTechStack,
    std::uint8_t flag1,
    std::uint8_t flag2,
    void* callback1,
    void* callback2) {
    const std::int64_t result = g_originalInitializeOreUiConfigRegistry != nullptr
        ? g_originalInitializeOreUiConfigRegistry(
            registry,
            screenTechStack,
            flag1,
            flag2,
            callback1,
            callback2)
        : 0;
    try {
        CaptureAndApplyRegistry(static_cast<OreUiConfigRegistry*>(registry));
    } catch (...) {
    }
    return result;
}

template <typename Function>
bool InstallHook(
    std::uintptr_t rva,
    const std::uint8_t* pattern,
    std::size_t patternSize,
    void* detour,
    Function& original) {
    void* target = GetModuleAddress(rva);
    if (!IsExecutableAddress(target) || std::memcmp(target, pattern, patternSize) != 0) {
        return false;
    }

    const MH_STATUS createStatus =
        MH_CreateHook(target, detour, reinterpret_cast<void**>(&original));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    return enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED;
}

bool EnsureHooksInstalled() {
    if (g_hooksInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    bool expected = false;
    if (!g_hookInstallAttempted.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel)) {
        return g_hooksInstalled.load(std::memory_order_acquire);
    }

    const bool settingsInstalled = InstallHook(
        tane::offsets::patch::kIsOreUiSettingsTabEnabledRva,
        kIsOreUiSettingsTabEnabledPattern,
        sizeof(kIsOreUiSettingsTabEnabledPattern),
        reinterpret_cast<void*>(&HookIsOreUiSettingsTabEnabled),
        g_originalIsOreUiSettingsTabEnabled);
    const bool registryInitializerInstalled = InstallHook(
        tane::offsets::patch::kInitializeOreUiConfigRegistryRva,
        kInitializeOreUiConfigRegistryPattern,
        sizeof(kInitializeOreUiConfigRegistryPattern),
        reinterpret_cast<void*>(&HookInitializeOreUiConfigRegistry),
        g_originalInitializeOreUiConfigRegistry);
    const bool playPredicateInstalled = InstallHook(
        tane::offsets::patch::kPlayScreenTechStackOreUiPredicateRva,
        kPlayScreenTechStackOreUiPredicatePattern,
        sizeof(kPlayScreenTechStackOreUiPredicatePattern),
        reinterpret_cast<void*>(&HookPlayScreenTechStackOreUiPredicate),
        g_originalPlayScreenTechStackOreUiPredicate);
    const bool legacyRouteInstalled = InstallHook(
        tane::offsets::patch::kUseLegacyPlayScreenRva,
        kUseLegacyPlayScreenPattern,
        sizeof(kUseLegacyPlayScreenPattern),
        reinterpret_cast<void*>(&HookUseLegacyPlayScreen),
        g_originalUseLegacyPlayScreen);
    const bool sceneRouteInstalled = InstallHook(
        tane::offsets::patch::kNavigateSceneRouteRva,
        kNavigateSceneRoutePattern,
        sizeof(kNavigateSceneRoutePattern),
        reinterpret_cast<void*>(&HookNavigateSceneRoute),
        g_originalNavigateSceneRoute);

    const bool installed =
        settingsInstalled &&
        registryInitializerInstalled &&
        playPredicateInstalled &&
        (static_cast<std::uintptr_t>(tane::offsets::patch::kUseLegacyPlayScreenRva) == 0 || legacyRouteInstalled) &&
        sceneRouteInstalled;
    g_hooksInstalled.store(installed, std::memory_order_release);
    return installed;
}

}  // namespace

bool IsForceCloseOreUiEnabled() {
    LoadConfig();
    return g_enabled.load(std::memory_order_relaxed);
}

void SetForceCloseOreUiEnabled(bool enabled) {
    LoadConfig();
    if (!EnsureHooksInstalled()) {
        return;
    }
    g_enabled.store(enabled, std::memory_order_release);
    ApplyRegistryState(enabled);
    SaveConfig();
}

bool InstallForceCloseOreUiHooks() {
    LoadConfig();
    return EnsureHooksInstalled();
}

void TickForceCloseOreUi(void* clientInstance) {
    TryInstrumentRegistry(clientInstance);
}

}  // namespace tane::patch
