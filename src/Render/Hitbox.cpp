#include "Offsets.h"

#include <Windows.h>
#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace tane::gui {
bool CanRunGameplayModules();
}

namespace tane::render {

void RenderTracers3D(void* levelRenderer, void* screenContext, void* clientInstance);

namespace {

using LevelRendererRenderLevelFn = void(__fastcall*)(void* levelRenderer, void* screenContext, void* unknown);
using GetLocalPlayerFn = void*(__fastcall*)(void* clientInstance);
using LevelGetRuntimeActorListFn = void(__fastcall*)(void* level, std::vector<void*>& actors);
using TessellatorBeginFn = void(__fastcall*)(void* tessellator, int unknown0, int primitive, int vertexCount, bool buildFaceData);
using TessellatorColorFn = void(__fastcall*)(void* tessellator, const void* color);
using TessellatorVertexFn = void(__fastcall*)(void* tessellator, float x, float y, float z);
using MeshHelpersRenderMeshImmediatelyFn = void(__fastcall*)(void* screenContext, void* tessellator, void* material, void* pad);
using HashedStringCtorFn = void*(__fastcall*)(void* hashedString, const char* text);
using RenderMaterialGroupCreateMaterialFn = void*(__fastcall*)(void* materialGroup, const void* materialName);

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Aabb {
    Vec3 lower{};
    Vec3 upper{};
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct ActorIdentity {
    void* entityContext = nullptr;
    std::uint32_t entityId = 0;
};

struct ComponentStorageCache {
    void* playerEntityContext = nullptr;
    void* geometryEntityContext = nullptr;
    void* player = nullptr;
    void* aabb = nullptr;
    void* renderPosition = nullptr;
    void* stateVector = nullptr;
};

std::atomic_bool g_enabled = false;
std::atomic_bool g_configLoaded = false;
std::atomic<void*> g_lastClientInstance = nullptr;
std::atomic<ULONGLONG> g_lastClientInstanceTick = 0;
std::atomic_bool g_renderLevelHookInstalled = false;
std::atomic<void*> g_localPlayerTarget = nullptr;
std::atomic<void*> g_runtimeActorListTarget = nullptr;
std::atomic<void*> g_tessellatorBeginTarget = nullptr;
std::atomic<void*> g_tessellatorColorTarget = nullptr;
std::atomic<void*> g_tessellatorVertexTarget = nullptr;
std::atomic<void*> g_meshFlushTarget = nullptr;
std::atomic<float> g_boxColorR = 1.0f;
std::atomic<float> g_boxColorG = 1.0f;
std::atomic<float> g_boxColorB = 1.0f;
std::atomic<float> g_boxColorA = 0.85f;
LevelRendererRenderLevelFn g_originalLevelRendererRenderLevel = nullptr;

bool BuildHitboxConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\Hitbox.json", renderPath) >= 0;
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

bool ParseFloatAfter(const char* section, const char* key, float& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    char* end = nullptr;
    const float parsed = std::strtof(found + 1, &end);
    if (end == found + 1 || !std::isfinite(parsed)) {
        return false;
    }

    value = std::clamp(parsed, 0.0f, 1.0f);
    return true;
}

Color LoadHitboxColor() {
    return Color{
        std::clamp(g_boxColorR.load(std::memory_order_acquire), 0.0f, 1.0f),
        std::clamp(g_boxColorG.load(std::memory_order_acquire), 0.0f, 1.0f),
        std::clamp(g_boxColorB.load(std::memory_order_acquire), 0.0f, 1.0f),
        std::clamp(g_boxColorA.load(std::memory_order_acquire), 0.0f, 1.0f),
    };
}

void StoreHitboxColor(const Color& color) {
    g_boxColorR.store(std::clamp(color.r, 0.0f, 1.0f), std::memory_order_release);
    g_boxColorG.store(std::clamp(color.g, 0.0f, 1.0f), std::memory_order_release);
    g_boxColorB.store(std::clamp(color.b, 0.0f, 1.0f), std::memory_order_release);
    g_boxColorA.store(std::clamp(color.a, 0.0f, 1.0f), std::memory_order_release);
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

template <typename T>
bool SafeRead(const void* address, T& value) {
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

    return SafeRead(reinterpret_cast<const std::uint8_t*>(base) + offset, value);
}

template <typename T>
bool SafeWrite(void* address, const T& value) {
    if (address == nullptr) {
        return false;
    }

    __try {
        *reinterpret_cast<T*>(address) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* GetExecutableVfunc(void* object, std::size_t vtableOffset) {
    void** vtable = nullptr;
    void* function = nullptr;
    if (!SafeRead(object, vtable) ||
        vtable == nullptr ||
        !ReadOffset(vtable, vtableOffset, function) ||
        !IsExecutableAddress(function)) {
        return nullptr;
    }

    return function;
}

void* GetCachedExecutableVfunc(std::atomic<void*>& cache, void* object, std::size_t vtableOffset) {
    void* cached = cache.load(std::memory_order_acquire);
    if (cached != nullptr) {
        return cached;
    }

    void* function = GetExecutableVfunc(object, vtableOffset);
    if (function == nullptr) {
        return nullptr;
    }

    void* expected = nullptr;
    cache.compare_exchange_strong(expected, function, std::memory_order_acq_rel);
    return cache.load(std::memory_order_acquire);
}

bool ReadActorIdentity(void* actor, ActorIdentity& identity) {
    void** vtable = nullptr;
    void* entityContext = nullptr;
    std::uint32_t entityId = 0;
    const bool valid = SafeRead(actor, vtable) &&
        vtable != nullptr &&
        ReadOffset(actor, tane::offsets::render::kActorEntityContextOffset, entityContext) &&
        ReadOffset(actor, tane::offsets::render::kActorEntityIdOffset, entityId) &&
        entityContext != nullptr &&
        entityId != 0;
    if (valid) {
        identity.entityContext = entityContext;
        identity.entityId = entityId;
    }
    return valid;
}

bool LooksLikeActor(void* actor) {
    ActorIdentity identity{};
    return ReadActorIdentity(actor, identity);
}

void* GetLocalPlayer(void* clientInstance) {
    void* function = GetCachedExecutableVfunc(
        g_localPlayerTarget,
        clientInstance,
        tane::offsets::render::kClientInstanceLocalPlayerVtableOffset);
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

void* ResolveComponentFromStorage(void* storage, std::uint32_t entityId, std::size_t entrySize) {
    std::uint32_t packedComponentId = 0;
    void* componentPages = nullptr;
    if (entrySize == 0 ||
        !ResolvePackedComponentId(storage, entityId, packedComponentId) ||
        !ReadOffset(storage, 0x50, componentPages) ||
        componentPages == nullptr) {
        return nullptr;
    }

    const std::size_t pageOffset = ((packedComponentId >> 4) & 0x3FF8);
    void* componentPage = nullptr;
    if (!ReadOffset(componentPages, pageOffset, componentPage) || componentPage == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<std::uint8_t*>(componentPage) + (packedComponentId & 0x7F) * entrySize;
}

void* FindComponentStorage(void* entityContext, std::uint32_t hash) {
    if (entityContext == nullptr) {
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

    void* slot = reinterpret_cast<void*>(begin + ((slotCount - 1) & hash) * sizeof(void*));
    for (int attempt = 0; attempt < 64; ++attempt) {
        std::uint64_t entryIndex = 0;
        if (!SafeRead(slot, entryIndex) || entryIndex == UINT64_MAX) {
            return nullptr;
        }

        void* entry = reinterpret_cast<std::uint8_t*>(hashEntries) + entryIndex * 0x20;
        std::uint32_t entryHash = 0;
        if (!ReadOffset(entry, 0x08, entryHash)) {
            return nullptr;
        }

        if (entryHash == hash) {
            if (entry == hashSentinel) {
                return nullptr;
            }

            void* storage = nullptr;
            return ReadOffset(entry, 0x10, storage) ? storage : nullptr;
        }

        slot = entry;
    }

    return nullptr;
}

void* ResolveComponent(const ActorIdentity& identity, std::uint32_t hash, std::size_t entrySize) {
    if (identity.entityContext == nullptr || identity.entityId == 0) {
        return nullptr;
    }

    void* storage = FindComponentStorage(identity.entityContext, hash);
    return storage != nullptr ? ResolveComponentFromStorage(storage, identity.entityId, entrySize) : nullptr;
}

void* ResolveComponent(void* actor, std::uint32_t hash, std::size_t entrySize) {
    ActorIdentity identity{};
    return ReadActorIdentity(actor, identity) ? ResolveComponent(identity, hash, entrySize) : nullptr;
}

void RefreshPlayerStorageCache(const ActorIdentity& identity, ComponentStorageCache& cache) {
    if (cache.playerEntityContext == identity.entityContext) {
        return;
    }

    cache.playerEntityContext = identity.entityContext;
    cache.player = FindComponentStorage(identity.entityContext, tane::offsets::render::kPlayerComponentHash);
}

void RefreshGeometryStorageCache(const ActorIdentity& identity, ComponentStorageCache& cache) {
    if (cache.geometryEntityContext == identity.entityContext) {
        return;
    }

    cache.geometryEntityContext = identity.entityContext;
    cache.aabb = FindComponentStorage(identity.entityContext, tane::offsets::render::kAABBShapeComponentHash);
    cache.renderPosition = FindComponentStorage(identity.entityContext, tane::offsets::render::kRenderPositionComponentHash);
    cache.stateVector = FindComponentStorage(identity.entityContext, tane::offsets::render::kStateVectorComponentHash);
}

bool HasComponentInStorage(void* storage, std::uint32_t entityId) {
    std::uint32_t packedComponentId = 0;
    return storage != nullptr && entityId != 0 && ResolvePackedComponentId(storage, entityId, packedComponentId);
}

void* ResolveCachedComponent(void* storage, const ActorIdentity& identity, std::size_t entrySize) {
    return storage != nullptr
        ? ResolveComponentFromStorage(storage, identity.entityId, entrySize)
        : nullptr;
}

bool HasPlayerComponent(const ActorIdentity& identity, const ComponentStorageCache& cache) {
    return HasComponentInStorage(cache.player, identity.entityId);
}

bool ReadAabb(const ActorIdentity& identity, const ComponentStorageCache& cache, Aabb& box) {
    void* aabb = ResolveCachedComponent(
        cache.aabb,
        identity,
        tane::offsets::render::kAABBShapeComponentEntrySize);
    if (aabb == nullptr) {
        return false;
    }

    if (!ReadOffset(aabb, 0x00, box.lower.x) ||
        !ReadOffset(aabb, 0x04, box.lower.y) ||
        !ReadOffset(aabb, 0x08, box.lower.z) ||
        !ReadOffset(aabb, 0x0C, box.upper.x) ||
        !ReadOffset(aabb, 0x10, box.upper.y) ||
        !ReadOffset(aabb, 0x14, box.upper.z)) {
        return false;
    }

    const float width = box.upper.x - box.lower.x;
    const float height = box.upper.y - box.lower.y;
    const float depth = box.upper.z - box.lower.z;
    return std::isfinite(box.lower.x) &&
        std::isfinite(box.lower.y) &&
        std::isfinite(box.lower.z) &&
        std::isfinite(box.upper.x) &&
        std::isfinite(box.upper.y) &&
        std::isfinite(box.upper.z) &&
        width > 0.0f &&
        height > 0.0f &&
        depth > 0.0f &&
        width < 16.0f &&
        height < 16.0f &&
        depth < 16.0f;
}

bool ReadRenderPosition(const ActorIdentity& identity, const ComponentStorageCache& cache, Vec3& renderPosition) {
    void* renderPositionComponent = ResolveCachedComponent(
        cache.renderPosition,
        identity,
        tane::offsets::render::kRenderPositionComponentEntrySize);
    if (renderPositionComponent == nullptr) {
        return false;
    }

    return ReadOffset(renderPositionComponent, 0x00, renderPosition.x) &&
        ReadOffset(renderPositionComponent, 0x04, renderPosition.y) &&
        ReadOffset(renderPositionComponent, 0x08, renderPosition.z) &&
        std::isfinite(renderPosition.x) &&
        std::isfinite(renderPosition.y) &&
        std::isfinite(renderPosition.z);
}

bool ReadPosition(const ActorIdentity& identity, const ComponentStorageCache& cache, Vec3& position) {
    void* stateVector = ResolveCachedComponent(
        cache.stateVector,
        identity,
        tane::offsets::render::kStateVectorComponentEntrySize);
    if (stateVector == nullptr) {
        return false;
    }

    return ReadOffset(stateVector, 0x00, position.x) &&
        ReadOffset(stateVector, 0x04, position.y) &&
        ReadOffset(stateVector, 0x08, position.z) &&
        std::isfinite(position.x) &&
        std::isfinite(position.y) &&
        std::isfinite(position.z);
}

void OffsetAabb(Aabb& box, const Vec3& delta) {
    box.lower.x += delta.x;
    box.lower.y += delta.y;
    box.lower.z += delta.z;
    box.upper.x += delta.x;
    box.upper.y += delta.y;
    box.upper.z += delta.z;
}

void RebaseAabbToRenderPosition(Aabb& box, const Vec3& renderPosition, const Vec3* currentPosition) {
    if (currentPosition != nullptr) {
        const Vec3 delta{
            renderPosition.x - currentPosition->x,
            renderPosition.y - currentPosition->y,
            renderPosition.z - currentPosition->z};
        constexpr float kMaxRenderDeltaSq = 8.0f * 8.0f;
        if ((delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) <= kMaxRenderDeltaSq) {
            OffsetAabb(box, delta);
        }
        return;
    }

    const float centerX = (box.lower.x + box.upper.x) * 0.5f;
    const float centerZ = (box.lower.z + box.upper.z) * 0.5f;
    const float dx = renderPosition.x - centerX;
    const float dz = renderPosition.z - centerZ;
    constexpr float kMaxCenterOffset = 2.5f;
    if ((dx * dx + dz * dz) > kMaxCenterOffset * kMaxCenterOffset) {
        return;
    }

    box.lower.x += dx;
    box.upper.x += dx;
    box.lower.z += dz;
    box.upper.z += dz;
}

void* GetHitboxRenderThroughMaterial() {
    static std::atomic<void*> material = nullptr;
    void* cached = material.load(std::memory_order_acquire);
    if (cached != nullptr) {
        return cached;
    }

    void* materialGroup = GetModuleAddress(tane::offsets::render::kRenderMaterialGroupCommonRva);
    void* hashedStringCtorTarget = GetModuleAddress(tane::offsets::render::kHashedStringCtorRva);
    if (materialGroup == nullptr || !IsExecutableAddress(hashedStringCtorTarget)) {
        return nullptr;
    }

    void* createMaterialTarget = GetExecutableVfunc(
        materialGroup,
        tane::offsets::render::kRenderMaterialGroupCreateMaterialVtableOffset);
    if (createMaterialTarget == nullptr) {
        return nullptr;
    }

    alignas(16) static std::array<std::uint8_t, 0x80> hashedStringStorage{};
    auto hashedStringCtor = reinterpret_cast<HashedStringCtorFn>(hashedStringCtorTarget);
    auto createMaterial = reinterpret_cast<RenderMaterialGroupCreateMaterialFn>(createMaterialTarget);

    void* created = nullptr;
    const char* materialNames[] = {"ui_fill_color", "selection_box"};
    for (const char* materialName : materialNames) {
        __try {
            hashedStringCtor(hashedStringStorage.data(), materialName);
            created = createMaterial(materialGroup, hashedStringStorage.data());
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            created = nullptr;
        }

        if (created != nullptr) {
            break;
        }
    }

    if (created == nullptr) {
        return nullptr;
    }

    void* expected = nullptr;
    material.compare_exchange_strong(expected, created, std::memory_order_acq_rel);
    return material.load(std::memory_order_acquire);
}

bool ReadLevelRendererOrigin(void* levelRenderer, Vec3& origin) {
    void* levelRendererPlayer = nullptr;
    if (!ReadOffset(
            levelRenderer,
            tane::offsets::render::kLevelRendererPlayerOffset,
            levelRendererPlayer) ||
        levelRendererPlayer == nullptr) {
        return false;
    }

    if (!ReadOffset(
            levelRendererPlayer,
            tane::offsets::render::kLevelRendererPlayerOriginOffset,
            origin)) {
        return false;
    }

    return std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z);
}

bool GetRuntimeActorList(void* level, std::vector<void*>& actors) {
    void* function = GetCachedExecutableVfunc(
        g_runtimeActorListTarget,
        level,
        tane::offsets::render::kLevelGetRuntimeActorListVtableOffset);
    if (function == nullptr) {
        return false;
    }

    __try {
        reinterpret_cast<LevelGetRuntimeActorListFn>(function)(level, actors);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        actors.clear();
        return false;
    }

    if (actors.size() > 4096) {
        actors.clear();
        return false;
    }

    return true;
}

void* GetCachedExecutableAddress(std::atomic<void*>& cache, std::uintptr_t rva) {
    void* cached = cache.load(std::memory_order_acquire);
    if (cached != nullptr) {
        return cached;
    }

    void* target = GetModuleAddress(rva);
    if (!IsExecutableAddress(target)) {
        return nullptr;
    }

    void* expected = nullptr;
    cache.compare_exchange_strong(expected, target, std::memory_order_acq_rel);
    return cache.load(std::memory_order_acquire);
}

bool ResolveDrawFunctions(
    TessellatorBeginFn& begin,
    TessellatorColorFn& setColor,
    TessellatorVertexFn& vertex,
    MeshHelpersRenderMeshImmediatelyFn& flush) {
    void* beginTarget = GetCachedExecutableAddress(g_tessellatorBeginTarget, tane::offsets::render::kTessellatorBeginRva);
    void* colorTarget = GetCachedExecutableAddress(g_tessellatorColorTarget, tane::offsets::render::kTessellatorColorRva);
    void* vertexTarget = GetCachedExecutableAddress(g_tessellatorVertexTarget, tane::offsets::render::kTessellatorVertexRva);
    void* flushTarget = GetCachedExecutableAddress(g_meshFlushTarget, tane::offsets::render::kMeshHelpersRenderMeshImmediatelyRva);
    if (beginTarget == nullptr || colorTarget == nullptr || vertexTarget == nullptr || flushTarget == nullptr) {
        return false;
    }

    begin = reinterpret_cast<TessellatorBeginFn>(beginTarget);
    setColor = reinterpret_cast<TessellatorColorFn>(colorTarget);
    vertex = reinterpret_cast<TessellatorVertexFn>(vertexTarget);
    flush = reinterpret_cast<MeshHelpersRenderMeshImmediatelyFn>(flushTarget);
    return true;
}

void PushLineVertex(TessellatorVertexFn vertex, void* tessellator, const Vec3& origin, const Vec3& point) {
    vertex(tessellator, point.x - origin.x, point.y - origin.y, point.z - origin.z);
}

void PushAabbVertices(TessellatorVertexFn vertex, void* tessellator, const Vec3& origin, const Aabb& box) {
    const Vec3 c000{box.lower.x, box.lower.y, box.lower.z};
    const Vec3 c100{box.upper.x, box.lower.y, box.lower.z};
    const Vec3 c010{box.lower.x, box.upper.y, box.lower.z};
    const Vec3 c110{box.upper.x, box.upper.y, box.lower.z};
    const Vec3 c001{box.lower.x, box.lower.y, box.upper.z};
    const Vec3 c101{box.upper.x, box.lower.y, box.upper.z};
    const Vec3 c011{box.lower.x, box.upper.y, box.upper.z};
    const Vec3 c111{box.upper.x, box.upper.y, box.upper.z};

    const std::pair<Vec3, Vec3> edges[] = {
        {c000, c100},
        {c100, c101},
        {c101, c001},
        {c001, c000},
        {c010, c110},
        {c110, c111},
        {c111, c011},
        {c011, c010},
        {c000, c010},
        {c100, c110},
        {c101, c111},
        {c001, c011},
    };

    for (const auto& edge : edges) {
        PushLineVertex(vertex, tessellator, origin, edge.first);
        PushLineVertex(vertex, tessellator, origin, edge.second);
    }
}

bool DrawAabbs3D(
    void* screenContext,
    void* tessellator,
    void* material,
    const Vec3& origin,
    const std::vector<Aabb>& boxes,
    const Color& color) {
    if (screenContext == nullptr || tessellator == nullptr || material == nullptr || boxes.empty()) {
        return false;
    }

    TessellatorBeginFn begin = nullptr;
    TessellatorColorFn setColor = nullptr;
    TessellatorVertexFn vertex = nullptr;
    MeshHelpersRenderMeshImmediatelyFn flush = nullptr;
    if (!ResolveDrawFunctions(begin, setColor, vertex, flush)) {
        return false;
    }

    void* shaderColor = nullptr;
    if (ReadOffset(screenContext, tane::offsets::render::kScreenContextShaderColorOffset, shaderColor) &&
        shaderColor != nullptr) {
        const Color white{1.0f, 1.0f, 1.0f, 1.0f};
        SafeWrite(shaderColor, white);
    }

    alignas(16) std::array<std::uint8_t, 0x58> pad{};
    __try {
        constexpr int kLineListPrimitive = 4;
        begin(tessellator, 0, kLineListPrimitive, static_cast<int>(boxes.size() * 24), false);
        setColor(tessellator, &color);
        for (const Aabb& box : boxes) {
            PushAabbVertices(vertex, tessellator, origin, box);
        }
        flush(screenContext, tessellator, material, pad.data());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return true;
}

void RenderCustomHitboxes(void* levelRenderer, void* screenContext) {
    if (!g_enabled.load(std::memory_order_acquire)) {
        return;
    }

    if (!tane::gui::CanRunGameplayModules()) {
        return;
    }

    void* clientInstance = g_lastClientInstance.load(std::memory_order_acquire);
    const ULONGLONG lastClientTick = g_lastClientInstanceTick.load(std::memory_order_acquire);
    if (clientInstance == nullptr || lastClientTick == 0 || GetTickCount64() - lastClientTick > 250) {
        return;
    }

    void* localPlayer = GetLocalPlayer(clientInstance);
    if (localPlayer == nullptr) {
        return;
    }

    void* level = nullptr;
    if (!ReadOffset(localPlayer, tane::offsets::render::kActorLevelOffset, level) || level == nullptr) {
        return;
    }

    void* tessellator = nullptr;
    if (!ReadOffset(screenContext, tane::offsets::render::kScreenContextTessellatorOffset, tessellator) ||
        tessellator == nullptr) {
        return;
    }

    Vec3 origin{};
    if (!ReadLevelRendererOrigin(levelRenderer, origin)) {
        return;
    }

    void* material = GetHitboxRenderThroughMaterial();
    if (material == nullptr) {
        return;
    }

    static std::vector<void*> actors;
    actors.clear();
    if (!GetRuntimeActorList(level, actors)) {
        return;
    }

    static std::vector<Aabb> boxesToDraw;
    boxesToDraw.clear();
    if (boxesToDraw.capacity() < actors.size()) {
        boxesToDraw.reserve(actors.size());
    }
    constexpr float kMaxDistanceSq = 160.0f * 160.0f;
    const Color color = LoadHitboxColor();
    ComponentStorageCache componentCache{};
    for (void* actor : actors) {
        if (actor == nullptr || actor == localPlayer) {
            continue;
        }

        ActorIdentity identity{};
        if (!ReadActorIdentity(actor, identity)) {
            continue;
        }
        RefreshPlayerStorageCache(identity, componentCache);
        if (!HasPlayerComponent(identity, componentCache)) {
            continue;
        }
        RefreshGeometryStorageCache(identity, componentCache);

        Vec3 renderPosition{};
        Vec3 currentPosition{};
        const bool hasRenderPosition = ReadRenderPosition(identity, componentCache, renderPosition);
        const bool hasCurrentPosition = ReadPosition(identity, componentCache, currentPosition);
        if (hasRenderPosition || hasCurrentPosition) {
            const Vec3& distancePosition = hasRenderPosition ? renderPosition : currentPosition;
            const float dx = distancePosition.x - origin.x;
            const float dy = distancePosition.y - origin.y;
            const float dz = distancePosition.z - origin.z;
            if ((dx * dx + dy * dy + dz * dz) > kMaxDistanceSq) {
                continue;
            }
        }

        Aabb box{};
        if (!ReadAabb(identity, componentCache, box)) {
            continue;
        }
        if (hasRenderPosition) {
            RebaseAabbToRenderPosition(box, renderPosition, hasCurrentPosition ? &currentPosition : nullptr);
        }

        const float centerX = (box.lower.x + box.upper.x) * 0.5f;
        const float centerY = (box.lower.y + box.upper.y) * 0.5f;
        const float centerZ = (box.lower.z + box.upper.z) * 0.5f;
        const float dx = centerX - origin.x;
        const float dy = centerY - origin.y;
        const float dz = centerZ - origin.z;
        if ((dx * dx + dy * dy + dz * dz) > kMaxDistanceSq) {
            continue;
        }

        boxesToDraw.push_back(box);
    }

    DrawAabbs3D(screenContext, tessellator, material, origin, boxesToDraw, color);
}

void __fastcall HookLevelRendererRenderLevel(void* levelRenderer, void* screenContext, void* unknown) {
    if (g_originalLevelRendererRenderLevel != nullptr) {
        g_originalLevelRendererRenderLevel(levelRenderer, screenContext, unknown);
    }

    if (levelRenderer == nullptr || screenContext == nullptr) {
        return;
    }

    __try {
        RenderCustomHitboxes(levelRenderer, screenContext);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    __try {
        RenderTracers3D(levelRenderer, screenContext, g_lastClientInstance.load(std::memory_order_acquire));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool EnsureCustomHitboxRenderHookInstalled() {
    if (g_renderLevelHookInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    void* target = GetModuleAddress(tane::offsets::render::kLevelRendererRenderLevelRva);
    if (!IsExecutableAddress(target)) {
        return false;
    }

    const MH_STATUS createStatus = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookLevelRendererRenderLevel),
        reinterpret_cast<void**>(&g_originalLevelRendererRenderLevel));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        return false;
    }

    g_renderLevelHookInstalled.store(true, std::memory_order_release);
    return true;
}

void SaveHitboxConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildHitboxConfigPath(path, MAX_PATH)) {
        return;
    }

    const Color color = LoadHitboxColor();
    char json[256]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 2,\n"
        "  \"enabled\": %s,\n"
        "  \"color\": {\n"
        "    \"r\": %.6f,\n"
        "    \"g\": %.6f,\n"
        "    \"b\": %.6f,\n"
        "    \"a\": %.6f\n"
        "  }\n"
        "}\n",
        g_enabled.load(std::memory_order_relaxed) ? "true" : "false",
        color.r,
        color.g,
        color.b,
        color.a);

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
}

void EnsureHitboxConfigLoaded() {
    EnsureCustomHitboxRenderHookInstalled();

    bool expected = false;
    if (!g_configLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildHitboxConfigPath(path, MAX_PATH)) {
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

        Color color = LoadHitboxColor();
        ParseFloatAfter(json, "\"r\"", color.r);
        ParseFloatAfter(json, "\"g\"", color.g);
        ParseFloatAfter(json, "\"b\"", color.b);
        ParseFloatAfter(json, "\"a\"", color.a);
        StoreHitboxColor(color);
    }

    CloseHandle(file);
}

}  // namespace

void EnsureRenderLevelRenderHookInstalled() {
    EnsureCustomHitboxRenderHookInstalled();
}

bool IsHitboxEnabled() {
    EnsureHitboxConfigLoaded();
    return g_enabled.load(std::memory_order_acquire);
}

void SetHitboxEnabled(bool enabled) {
    EnsureHitboxConfigLoaded();
    g_enabled.store(enabled, std::memory_order_release);
    if (!enabled) {
        g_lastClientInstance.store(nullptr, std::memory_order_release);
        g_lastClientInstanceTick.store(0, std::memory_order_release);
    }
    SaveHitboxConfig();
}

void GetHitboxColor(float& r, float& g, float& b, float& a) {
    EnsureHitboxConfigLoaded();
    const Color color = LoadHitboxColor();
    r = color.r;
    g = color.g;
    b = color.b;
    a = color.a;
}

void SetHitboxColor(float r, float g, float b, float a) {
    EnsureHitboxConfigLoaded();
    StoreHitboxColor(Color{r, g, b, a});
    SaveHitboxConfig();
}

void TickHitbox(void* clientInstance) {
    EnsureHitboxConfigLoaded();
    g_lastClientInstance.store(clientInstance, std::memory_order_release);
    g_lastClientInstanceTick.store(clientInstance != nullptr ? GetTickCount64() : 0, std::memory_order_release);
}

}  // namespace tane::render
