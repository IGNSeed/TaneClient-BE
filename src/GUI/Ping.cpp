#include <Windows.h>
#include <MinHook.h>
#include <imgui.h>

#include "Offsets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <cstdint>

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

constexpr float kMinPingScale = 0.65f;
constexpr float kMaxPingScale = 3.0f;
constexpr float kDefaultPingX = 3.0f;
constexpr float kDefaultPingY = 92.0f;
constexpr float kDefaultPingScale = 1.34f;
constexpr DWORD64 kPingRefreshMs = 250;
constexpr int kServerReportedPingCompensationMs = 20;

std::atomic_bool g_pingEnabled = false;
std::atomic_bool g_pingShowBackground = true;
std::atomic_bool g_pingConfigLoaded = false;
std::atomic_bool g_pingCustomPosition = false;
std::atomic_bool g_pingPositionDirty = false;
std::atomic<float> g_pingPositionX = kDefaultPingX;
std::atomic<float> g_pingPositionY = kDefaultPingY;
std::atomic<float> g_pingScale = kDefaultPingScale;
std::atomic<int> g_pingDisplayedMs = 0;
std::atomic<DWORD64> g_pingLastRefreshTick = 0;
std::atomic_bool g_pingHookInstalled = false;
std::atomic<DWORD64> g_pingLastHookInstallAttemptTick = 0;
std::atomic<DWORD64> g_pingHookLastCaptureTick = 0;

using RakPeerGetAveragePingFn = int(__fastcall*)(void* peer, void* addressOrGuid);
RakPeerGetAveragePingFn g_originalRakPeerGetAveragePing = nullptr;
void* g_rakPeerGetAveragePingTarget = nullptr;
RakPeerGetAveragePingFn g_rakPeerGetLastPingTarget = nullptr;
using NetworkStatusProviderGetStatusFn = void(__fastcall*)(void* provider, void* result);

bool BuildPingConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\Ping.json", guiPath) >= 0;
}

bool IsPlausiblePointer(const void* value) {
    const auto address = reinterpret_cast<std::uintptr_t>(value);
    return address >= 0x10000 && address < 0x0000800000000000ull;
}

bool IsPlausiblePing(int ping) {
    return ping >= 0 && ping <= 10000;
}

bool TryGetLastPingFromRakPeer(void* peer, void* addressOrGuid, int& lastPing) {
    lastPing = -1;
    if (!IsPlausiblePointer(peer) || !IsPlausiblePointer(reinterpret_cast<void*>(g_rakPeerGetLastPingTarget))) {
        return false;
    }

    __try {
        const int value = g_rakPeerGetLastPingTarget(peer, addressOrGuid);
        if (IsPlausiblePing(value)) {
            lastPing = value;
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        lastPing = -1;
        return false;
    }

    return false;
}

int EstimateServerReportedRakNetPing(int averagePing, int lastPing) {
    const int safeAveragePing = IsPlausiblePing(averagePing) ? averagePing : 0;
    const int safeLastPing = IsPlausiblePing(lastPing) ? lastPing : 0;
    const int compensatedAverage = safeAveragePing + kServerReportedPingCompensationMs;
    return std::clamp(std::max(safeLastPing, compensatedAverage), 0, 10000);
}

int __fastcall HookRakPeerGetAveragePing(void* peer, void* addressOrGuid) {
    const RakPeerGetAveragePingFn original = g_originalRakPeerGetAveragePing;
    const int averagePing = original != nullptr ? original(peer, addressOrGuid) : 0;
    if (IsPlausiblePing(averagePing)) {
        int lastPing = -1;
        const bool hasLastPing = TryGetLastPingFromRakPeer(peer, addressOrGuid, lastPing);
        const int displayPing = EstimateServerReportedRakNetPing(averagePing, hasLastPing ? lastPing : 0);

        g_pingDisplayedMs.store(displayPing, std::memory_order_relaxed);
        g_pingHookLastCaptureTick.store(GetTickCount64(), std::memory_order_relaxed);
    }
    return averagePing;
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

void SavePingConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildPingConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[320]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 1,\n"
        "  \"enabled\": %s,\n"
        "  \"showBackground\": %s,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        g_pingEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_pingShowBackground.load(std::memory_order_relaxed) ? "true" : "false",
        g_pingCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_pingPositionX.load(std::memory_order_relaxed),
        g_pingPositionY.load(std::memory_order_relaxed),
        g_pingScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_pingPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsurePingConfigLoaded() {
    bool expected = false;
    if (!g_pingConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildPingConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char json[768]{};
    DWORD read = 0;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';

        bool enabled = false;
        bool showBackground = true;
        bool custom = false;
        float x = kDefaultPingX;
        float y = kDefaultPingY;
        float scale = kDefaultPingScale;

        ParseBoolAfter(json, "\"enabled\"", enabled);
        ParseBoolAfter(json, "\"showBackground\"", showBackground);
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);

        g_pingEnabled.store(enabled, std::memory_order_relaxed);
        g_pingShowBackground.store(showBackground, std::memory_order_relaxed);
        g_pingCustomPosition.store(custom, std::memory_order_relaxed);
        g_pingPositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_pingPositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_pingScale.store(std::clamp(scale, kMinPingScale, kMaxPingScale), std::memory_order_relaxed);
    }
    CloseHandle(file);
}

float GetClampedPingScale() {
    return std::clamp(g_pingScale.load(std::memory_order_relaxed), kMinPingScale, kMaxPingScale);
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

ImVec2 GetPingTextSize(float scale) {
    return CalcGuiTextSize("Ping 0000ms", ImGui::GetFontSize() * scale);
}

ImVec2 GetPingRectSize(float scale) {
    const ImVec2 textSize = GetPingTextSize(scale);
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    return ImVec2(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
}

ImVec2 ClampPingPosition(const ImVec2& position, const ImVec2& displaySize) {
    const ImVec2 rectSize = GetPingRectSize(GetClampedPingScale());
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, displaySize.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, displaySize.y - rectSize.y)));
}

ImVec2 GetEffectivePingPosition(const ImVec2& displaySize) {
    EnsurePingConfigLoaded();
    if (!g_pingCustomPosition.load(std::memory_order_relaxed)) {
        return ClampPingPosition(ImVec2(kDefaultPingX, kDefaultPingY), displaySize);
    }

    return ClampPingPosition(
        ImVec2(
            g_pingPositionX.load(std::memory_order_relaxed),
            g_pingPositionY.load(std::memory_order_relaxed)),
        displaySize);
}

bool ReadPointerAt(const void* base, std::size_t offset, void*& out) {
    out = nullptr;
    if (!IsPlausiblePointer(base)) {
        return false;
    }

    __try {
        out = *reinterpret_cast<void* const*>(reinterpret_cast<const std::uint8_t*>(base) + offset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = nullptr;
        return false;
    }

    return IsPlausiblePointer(out);
}

bool ReadPointerRaw(const void* address, void*& out) {
    out = nullptr;
    if (!IsPlausiblePointer(address)) {
        return false;
    }

    __try {
        out = *reinterpret_cast<void* const*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = nullptr;
        return false;
    }

    return IsPlausiblePointer(out);
}

bool ReadIntAt(const void* base, std::size_t offset, int& out) {
    out = 0;
    if (!IsPlausiblePointer(base)) {
        return false;
    }

    __try {
        out = *reinterpret_cast<const int*>(reinterpret_cast<const std::uint8_t*>(base) + offset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0;
        return false;
    }

    return true;
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

bool IsScannableProtection(DWORD protection) {
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    switch (protection & 0xFF) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

std::size_t GetModuleImageSize(HMODULE module) {
    if (module == nullptr) {
        return 0;
    }

    const auto* base = reinterpret_cast<const std::uint8_t*>(module);
    if (!IsReadableAddress(base, sizeof(IMAGE_DOS_HEADER))) {
        return 0;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (!IsReadableAddress(nt, sizeof(IMAGE_NT_HEADERS64)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    return nt->OptionalHeader.SizeOfImage;
}

bool MatchesPattern(const std::uint8_t* candidate, const int* pattern, std::size_t patternSize) {
    for (std::size_t index = 0; index < patternSize; ++index) {
        if (pattern[index] >= 0 && candidate[index] != static_cast<std::uint8_t>(pattern[index])) {
            return false;
        }
    }

    return true;
}

template <std::size_t PatternSize>
void* FindPatternInModule(HMODULE module, const int (&pattern)[PatternSize]) {
    const auto imageBase = reinterpret_cast<std::uintptr_t>(module);
    const std::size_t imageSize = GetModuleImageSize(module);
    if (imageBase == 0 || imageSize < PatternSize) {
        return nullptr;
    }

    const std::uintptr_t imageEnd = imageBase + imageSize;
    std::uintptr_t cursor = imageBase;
    while (cursor < imageEnd) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) {
            break;
        }

        const auto regionBase = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        const std::uintptr_t regionEnd = std::min(regionBase + info.RegionSize, imageEnd);
        const std::uintptr_t scanBegin = std::max(cursor, regionBase);
        if (info.State == MEM_COMMIT && IsScannableProtection(info.Protect) && regionEnd > scanBegin) {
            const std::size_t regionSize = static_cast<std::size_t>(regionEnd - scanBegin);
            if (regionSize >= PatternSize) {
                const auto* bytes = reinterpret_cast<const std::uint8_t*>(scanBegin);
                for (std::size_t offset = 0; offset <= regionSize - PatternSize; ++offset) {
                    if (MatchesPattern(bytes + offset, pattern, PatternSize)) {
                        return const_cast<std::uint8_t*>(bytes + offset);
                    }
                }
            }
        }

        cursor = regionEnd > cursor ? regionEnd : cursor + 0x1000;
    }

    return nullptr;
}

void* ResolveRakPeerGetAveragePingTarget() {
    HMODULE module = GetModuleHandleW(L"Minecraft.Windows.exe");
    if (module == nullptr) {
        module = GetModuleHandleW(nullptr);
    }

    if (module != nullptr) {
        const std::uintptr_t rva = tane::offsets::ping::kRakPeerGetAveragePingRva;
        const std::uintptr_t lastPingRva = tane::offsets::ping::kRakPeerGetLastPingRva;
        void* lastPingTarget = lastPingRva != 0
            ? reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module) + lastPingRva)
            : nullptr;
        g_rakPeerGetLastPingTarget = IsExecutableAddress(lastPingTarget)
            ? reinterpret_cast<RakPeerGetAveragePingFn>(lastPingTarget)
            : nullptr;
        if (rva != 0) {
            void* target = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module) + rva);
            if (IsExecutableAddress(target)) {
                return target;
            }
        }
    }

    constexpr int kRakPeerGetAveragePingPattern[] = {
        0x48, 0x81, 0xEC, -1, -1, -1, -1, 0x48, 0x8B, 0x05, -1, -1, -1, -1,
        0x48, 0x31, 0xE0, 0x48, 0x89, 0x84, 0x24, -1, -1, -1, -1,
        0x48, 0x8B, 0x02, 0x48, 0x3B, 0x05, -1, -1, -1, -1, 0x0F, 0x85,
    };

    void* target = FindPatternInModule(module, kRakPeerGetAveragePingPattern);
    return IsExecutableAddress(target) ? target : nullptr;
}

bool EnsurePingHookInstalled() {
    const bool rakPeerHookInstalled = g_pingHookInstalled.load(std::memory_order_relaxed);
    if (rakPeerHookInstalled) {
        return true;
    }

    const DWORD64 now = GetTickCount64();
    const DWORD64 previousAttempt = g_pingLastHookInstallAttemptTick.load(std::memory_order_relaxed);
    if (previousAttempt != 0 && now - previousAttempt < 5000) {
        return false;
    }
    g_pingLastHookInstallAttemptTick.store(now, std::memory_order_relaxed);

    HMODULE module = GetModuleHandleW(L"Minecraft.Windows.exe");
    if (module == nullptr) {
        module = GetModuleHandleW(nullptr);
    }
    if (!rakPeerHookInstalled) {
        void* target = ResolveRakPeerGetAveragePingTarget();
        if (target != nullptr) {
            const MH_STATUS createStatus = MH_CreateHook(
                target,
                reinterpret_cast<void*>(&HookRakPeerGetAveragePing),
                reinterpret_cast<void**>(&g_originalRakPeerGetAveragePing));
            if (createStatus == MH_OK) {
                g_rakPeerGetAveragePingTarget = target;

                if (MH_EnableHook(target) == MH_OK) {
                    g_pingHookInstalled.store(true, std::memory_order_relaxed);
                }
            }
        }
    }
    return g_pingHookInstalled.load(std::memory_order_relaxed);
}

bool TryReadNestedPointer(void* base, const std::size_t* offsets, std::size_t offsetCount, void*& out) {
    for (std::size_t i = 0; i < offsetCount; ++i) {
        if (ReadPointerAt(base, offsets[i], out)) {
            return true;
        }
    }

    out = nullptr;
    return false;
}

bool TryGetRemoteConnectorComposite(void* clientInstance, void*& remoteConnectorComposite) {
    using namespace tane::offsets::ping;

    void* packetSender = nullptr;
    if (!TryReadNestedPointer(
            clientInstance,
            kClientInstancePacketSenderOffsetCandidates,
            sizeof(kClientInstancePacketSenderOffsetCandidates) / sizeof(kClientInstancePacketSenderOffsetCandidates[0]),
            packetSender)) {
        return false;
    }

    void* networkSystem = nullptr;
    if (!ReadPointerAt(packetSender, kPacketSenderNetworkSystemOffset, networkSystem)) {
        return false;
    }

    return TryReadNestedPointer(
        networkSystem,
        kNetworkSystemRemoteConnectorCompositeOffsetCandidates,
        sizeof(kNetworkSystemRemoteConnectorCompositeOffsetCandidates) / sizeof(kNetworkSystemRemoteConnectorCompositeOffsetCandidates[0]),
        remoteConnectorComposite);
}

bool TryGetNetherNetPing(void* remoteConnectorComposite, int& ping) {
    using namespace tane::offsets::ping;

    void* netherNetConnector = nullptr;
    if (!ReadPointerAt(remoteConnectorComposite, kRemoteConnectorCompositeNetherNetConnectorOffset, netherNetConnector)) {
        return false;
    }

    void* beginPointer = nullptr;
    void* endPointer = nullptr;
    if (!ReadPointerAt(netherNetConnector, kNetherNetConnectorPeersOffset, beginPointer) ||
        !ReadPointerAt(netherNetConnector, kNetherNetConnectorPeersOffset + sizeof(void*), endPointer)) {
        return false;
    }

    const auto begin = reinterpret_cast<std::uintptr_t>(beginPointer);
    const auto end = reinterpret_cast<std::uintptr_t>(endPointer);
    if (end <= begin || end - begin > 0x4000) {
        return false;
    }

    constexpr std::size_t kWeakPtrStride = sizeof(void*) * 2;
    std::size_t inspected = 0;
    for (std::uintptr_t entry = begin; entry + kWeakPtrStride <= end && inspected < 64; entry += kWeakPtrStride, ++inspected) {
        void* peer = nullptr;
        void* controlBlock = nullptr;
        if (!ReadPointerRaw(reinterpret_cast<const void*>(entry), peer) ||
            !ReadPointerRaw(reinterpret_cast<const void*>(entry + sizeof(void*)), controlBlock)) {
            continue;
        }

        int strongCount = 0;
        if (!ReadIntAt(controlBlock, sizeof(void*), strongCount) || strongCount <= 0) {
            continue;
        }

        void* statusProvider = nullptr;
        if (ReadPointerAt(peer, kWebRtcNetworkPeerStatusProviderOffset, statusProvider)) {
            void* statusVtableValue = nullptr;
            void* getStatus = nullptr;
            if (ReadPointerRaw(statusProvider, statusVtableValue) &&
                ReadPointerAt(statusVtableValue, kNetworkStatusProviderGetStatusVtableOffset, getStatus) &&
                IsExecutableAddress(getStatus)) {
                alignas(8) std::array<std::uint8_t, 0x20> status{};
                __try {
                    reinterpret_cast<NetworkStatusProviderGetStatusFn>(getStatus)(statusProvider, status.data());
                    const auto currentPing64 = *reinterpret_cast<const std::int64_t*>(
                        status.data() + kNetworkStatusCurrentPingOffset);
                    const auto averagePing64 = *reinterpret_cast<const std::int64_t*>(
                        status.data() + kNetworkStatusAveragePingOffset);
                    if (averagePing64 >= 0 && averagePing64 <= 10000) {
                        ping = static_cast<int>(averagePing64);
                        return true;
                    }
                    if (currentPing64 >= 0 && currentPing64 <= 10000) {
                        ping = static_cast<int>(currentPing64);
                        return true;
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }
        }

        int averagePing = -1;
        const bool averageOk = ReadIntAt(peer, kWebRtcNetworkPeerAveragePingOffset, averagePing);
        if (averageOk && IsPlausiblePing(averagePing)) {
            ping = averagePing;
            return true;
        }
        int currentPing = -1;
        const bool currentOk = ReadIntAt(peer, kWebRtcNetworkPeerCurrentPingOffset, currentPing);
        if (currentOk && IsPlausiblePing(currentPing)) {
            ping = currentPing;
            return true;
        }
    }

    return false;
}

bool TryQueryPing(void* clientInstance, int& ping) {
    void* remoteConnectorComposite = nullptr;
    if (!TryGetRemoteConnectorComposite(clientInstance, remoteConnectorComposite)) {
        return false;
    }

    return TryGetNetherNetPing(remoteConnectorComposite, ping);
}

} // namespace

bool IsPingEnabled() {
    EnsurePingConfigLoaded();
    return g_pingEnabled.load(std::memory_order_relaxed);
}

void SetPingEnabled(bool enabled) {
    EnsurePingConfigLoaded();
    g_pingEnabled.store(enabled, std::memory_order_relaxed);
    SavePingConfig();
}

bool IsPingBackgroundEnabled() {
    EnsurePingConfigLoaded();
    return g_pingShowBackground.load(std::memory_order_relaxed);
}

void SetPingBackgroundEnabled(bool enabled) {
    EnsurePingConfigLoaded();
    g_pingShowBackground.store(enabled, std::memory_order_relaxed);
    SavePingConfig();
}

bool GetPingEditorRect(float displayWidth, float displayHeight, float& x, float& y, float& width, float& height) {
    EnsurePingConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImVec2 displaySize(displayWidth, displayHeight);
    const float scale = GetClampedPingScale();
    const ImVec2 position = GetEffectivePingPosition(displaySize);
    const ImVec2 size = GetPingRectSize(scale);

    x = position.x;
    y = position.y;
    width = size.x;
    height = size.y;
    return true;
}

void SetPingEditorDisplayPosition(float displayX, float displayY, float displayWidth, float displayHeight) {
    EnsurePingConfigLoaded();
    if (displayWidth <= 0.0f || displayHeight <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const ImVec2 clamped = ClampPingPosition(ImVec2(displayX, displayY), ImVec2(displayWidth, displayHeight));
    g_pingCustomPosition.store(true, std::memory_order_relaxed);
    g_pingPositionX.store(clamped.x, std::memory_order_relaxed);
    g_pingPositionY.store(clamped.y, std::memory_order_relaxed);
    g_pingPositionDirty.store(true, std::memory_order_relaxed);
}

void MovePingEditorDisplayPosition(float deltaX, float deltaY, float displayWidth, float displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!GetPingEditorRect(displayWidth, displayHeight, x, y, width, height)) {
        return;
    }

    SetPingEditorDisplayPosition(x + deltaX, y + deltaY, displayWidth, displayHeight);
}

float GetPingEditorScale() {
    EnsurePingConfigLoaded();
    return GetClampedPingScale();
}

void SetPingEditorScale(float scale) {
    EnsurePingConfigLoaded();
    g_pingScale.store(std::clamp(scale, kMinPingScale, kMaxPingScale), std::memory_order_relaxed);
    g_pingPositionDirty.store(true, std::memory_order_relaxed);
}

void ResetPingEditorPosition() {
    EnsurePingConfigLoaded();
    g_pingCustomPosition.store(false, std::memory_order_relaxed);
    g_pingPositionX.store(kDefaultPingX, std::memory_order_relaxed);
    g_pingPositionY.store(kDefaultPingY, std::memory_order_relaxed);
    g_pingScale.store(kDefaultPingScale, std::memory_order_relaxed);
    g_pingPositionDirty.store(true, std::memory_order_relaxed);
    SavePingConfig();
}

void CommitPingEditorPosition() {
    EnsurePingConfigLoaded();
    if (g_pingPositionDirty.load(std::memory_order_relaxed)) {
        SavePingConfig();
    }
}

void TickPing(void* clientInstance) {
    EnsurePingConfigLoaded();
    if (!g_pingEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    const DWORD64 now = GetTickCount64();
    const DWORD64 previous = g_pingLastRefreshTick.load(std::memory_order_relaxed);
    if (previous != 0 && now - previous < kPingRefreshMs) {
        return;
    }
    g_pingLastRefreshTick.store(now, std::memory_order_relaxed);

    EnsurePingHookInstalled();
    const DWORD64 hookCaptureTick = g_pingHookLastCaptureTick.load(std::memory_order_relaxed);
    if (hookCaptureTick != 0 && now - hookCaptureTick <= 10000) {
        return;
    }

    if (clientInstance == nullptr) {
        g_pingDisplayedMs.store(0, std::memory_order_relaxed);
        return;
    }

    int ping = -1;
    if (TryQueryPing(clientInstance, ping)) {
        g_pingDisplayedMs.store(std::clamp(ping, 0, 10000), std::memory_order_relaxed);
    }
}

void RenderPingOverlay() {
    EnsurePingConfigLoaded();
    if (IsGuiPositionEditorActive()) {
        return;
    }

    if (ImGui::GetCurrentContext() == nullptr ||
        !g_pingEnabled.load(std::memory_order_relaxed) ||
        !ShouldShowGuiOverlay()) {
        return;
    }

    char text[32]{};
    std::snprintf(text, sizeof(text), "Ping %dms", std::max(0, g_pingDisplayedMs.load(std::memory_order_relaxed)));

    ImGuiIO& io = ImGui::GetIO();
    const float scale = GetClampedPingScale();
    const ImVec2 position = GetEffectivePingPosition(io.DisplaySize);
    ImFont* font = GetGuiTextFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    const ImVec2 textSize = CalcGuiTextSize(text, fontSize);
    const float paddingX = 8.0f * scale;
    const float paddingY = 5.0f * scale;
    const ImVec2 rectSize(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
    const ImVec2 rectMax(position.x + rectSize.x, position.y + rectSize.y);
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if (g_pingShowBackground.load(std::memory_order_relaxed)) {
        drawList->AddRectFilled(position, rectMax, IM_COL32(0, 0, 0, 142), 5.0f * scale);
        drawList->AddRect(position, rectMax, IM_COL32(255, 255, 255, 44), 5.0f * scale, 0, 1.0f);
    }

    const ImVec2 textPos(position.x + paddingX, position.y + paddingY);
    DrawGuiTextWithShadow(
        drawList,
        font,
        fontSize,
        textPos,
        IM_COL32(246, 248, 250, 238),
        text,
        scale,
        IM_COL32(0, 0, 0, 150));
}

} // namespace tane::gui
