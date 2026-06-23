#include <Windows.h>
#include <MinHook.h>

#include "Offsets.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace tane::gui {
bool IsMenuOpen();
}

namespace tane::imgui_menu {
namespace {

using namespace tane::offsets::movement;

using GetLocalPlayerFn = void*(__fastcall*)(void* clientInstance);
using ProcessAnalogInputStateFn = bool(__fastcall*)(void* inputEntry, void* analogValues, void* analogState);
using InputDispatch3Fn = void(__fastcall*)(void* self, int inputId, bool value);
using InputDispatch5Fn = void(__fastcall*)(void* self, int inputId, int buttonMask, bool active, bool pressed);
using AnalogVectorDispatchFn = void(__fastcall*)(void* self, int inputId, bool axis, int state, float x, float y);
using AnalogDispatchFn = void(__fastcall*)(void* self, int inputId, bool axis, float value);
using PointerDispatch4Fn = void(__fastcall*)(int eventType, int pointerId, int x, int y);
using PointerDispatch5Fn = void(__fastcall*)(int eventType, int pointerId, int x, int y, int flags);

ProcessAnalogInputStateFn g_originalProcessAnalogInputState = nullptr;
InputDispatch3Fn g_originalButtonPressDispatch = nullptr;
InputDispatch3Fn g_originalButtonTransitionDispatch = nullptr;
InputDispatch3Fn g_originalButtonStateDispatch = nullptr;
InputDispatch5Fn g_originalButtonAnalogDispatch = nullptr;
AnalogVectorDispatchFn g_originalAnalogVectorDispatch = nullptr;
AnalogDispatchFn g_originalAnalogDispatch = nullptr;
PointerDispatch4Fn g_originalPointerMotionDispatch = nullptr;
PointerDispatch4Fn g_originalPointerButtonDispatch = nullptr;
PointerDispatch5Fn g_originalUiNavigationDispatch = nullptr;
std::atomic_bool g_internalHooksInstalled = false;
std::atomic_bool g_resetInputForCurrentOpen = false;
std::atomic<ULONGLONG> g_blockInputUntilTick = 0;
std::atomic<ULONGLONG> g_lastComponentClearTick = 0;
thread_local bool g_allowAnalogReleaseDispatch = false;
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

bool IsWritableAddress(const void* address, std::size_t size) {
    if (!IsReadableAddress(address, size)) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0) {
        return false;
    }

    const DWORD protection = info.Protect & 0xFF;
    return protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
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

void* ResolveModuleRva(std::uintptr_t rva) {
    HMODULE module = GetModuleHandleW(nullptr);
    if (module == nullptr || rva == 0) {
        return nullptr;
    }

    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module) + rva);
}

bool CreateAndEnableHook(void* target, void* detour, void** original) {
    if (!IsExecutableAddress(target) || detour == nullptr || original == nullptr) {
        return false;
    }

    const MH_STATUS createStatus = MH_CreateHook(target, detour, original);
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    return enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED;
}

template <typename T>
bool ReadValue(const void* address, T& value);

template <typename T>
bool ReadOffset(const void* base, std::size_t offset, T& value);

template <typename T>
void WriteOffset(void* base, std::size_t offset, T value);

void ClearAnalogInputState(void* analogState);
void ZeroAnalogValues(void* analogValues);
void UpdateMenuOpenResetState();

bool ShouldBlockInternalInput() {
    if (tane::gui::IsMenuOpen()) {
        return true;
    }

    const ULONGLONG blockUntilTick = g_blockInputUntilTick.load(std::memory_order_acquire);
    if (blockUntilTick == 0) {
        return false;
    }

    const ULONGLONG now = GetTickCount64();
    if (now < blockUntilTick) {
        return true;
    }

    g_blockInputUntilTick.store(0, std::memory_order_release);
    return false;
}

template <typename T>
bool ReadHotPathValue(const T* address, T& value) {
    if (address == nullptr) {
        return false;
    }

    __try {
        value = *address;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadHotPathPointerOffset(const void* base, std::size_t offset, void*& value) {
    if (base == nullptr) {
        return false;
    }

    __try {
        value = *reinterpret_cast<void* const*>(reinterpret_cast<const std::uint8_t*>(base) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool LooksLikeMinecraftInputHandler(void* inputHandler) {
    void** vtable = nullptr;
    void* getLocalPlayer = nullptr;
    return ReadOffset(inputHandler, 0, vtable) &&
        ReadOffset(vtable, kClientInstanceLocalPlayerVtableOffset, getLocalPlayer) &&
        IsExecutableAddress(getLocalPlayer);
}

bool HasQueuedInputEvents(void* entry) {
    void* begin = nullptr;
    void* end = nullptr;
    if (!ReadOffset(entry, 0x08, begin) || !ReadOffset(entry, 0x10, end)) {
        return false;
    }

    const auto beginAddress = reinterpret_cast<std::uintptr_t>(begin);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(end);
    if (beginAddress == 0 || endAddress < beginAddress) {
        return false;
    }

    const std::uintptr_t queueBytes = endAddress - beginAddress;
    constexpr std::uintptr_t kMaxQueueBytes = 0x79F8 * 0x20;
    if (queueBytes == 0 || queueBytes > kMaxQueueBytes || !IsReadableAddress(begin, static_cast<std::size_t>(queueBytes))) {
        return false;
    }

    __try {
        std::uint8_t* cursor = reinterpret_cast<std::uint8_t*>(begin);
        while (cursor < reinterpret_cast<std::uint8_t*>(end)) {
            const std::uint32_t eventType = *reinterpret_cast<const std::uint32_t*>(cursor + 0x08);
            const std::uint64_t queueRead = *reinterpret_cast<const std::uint64_t*>(cursor + 0x79E8);
            const std::uint64_t queueWrite = *reinterpret_cast<const std::uint64_t*>(cursor + 0x79F0);
            if (eventType != 0 || queueRead != 0x258 || queueWrite != 0x258) {
                return true;
            }
            cursor += 0x79F8;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return false;
}

void ResetQueuedInputEvents(void* entry) {
    void* begin = nullptr;
    void* end = nullptr;
    if (!ReadOffset(entry, 0x08, begin) || !ReadOffset(entry, 0x10, end)) {
        return;
    }

    const auto beginAddress = reinterpret_cast<std::uintptr_t>(begin);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(end);
    if (beginAddress == 0 || endAddress < beginAddress) {
        return;
    }

    const std::uintptr_t queueBytes = endAddress - beginAddress;
    constexpr std::uintptr_t kMaxQueueBytes = 0x79F8 * 0x20;
    if (queueBytes == 0 || queueBytes > kMaxQueueBytes || !IsWritableAddress(begin, static_cast<std::size_t>(queueBytes))) {
        return;
    }

    __try {
        std::uint8_t* cursor = reinterpret_cast<std::uint8_t*>(begin);
        while (cursor < reinterpret_cast<std::uint8_t*>(end)) {
            SecureZeroMemory(cursor, 0x79E8);
            *reinterpret_cast<std::uint64_t*>(cursor + 0x79E8) = 0x258;
            *reinterpret_cast<std::uint64_t*>(cursor + 0x79F0) = 0x258;
            cursor += 0x79F8;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void ReleaseInputEntry(void* inputService, int inputId, void* entry) {
    (void)inputService;
    (void)inputId;

    if (!IsWritableAddress(entry, 0x30)) {
        return;
    }

    std::uint8_t wasPressed = 0;
    ReadOffset(entry, 0x20, wasPressed);
    const bool hadQueuedInput = HasQueuedInputEvents(entry);
    if (wasPressed == 0 && !hadQueuedInput) {
        return;
    }

    ResetQueuedInputEvents(entry);
    WriteOffset<std::uint8_t>(entry, 0x20, 0);
}

void ResetInternalInputServiceState() {
    void* inputService = ResolveModuleRva(tane::offsets::input::kInputServiceRva);
    if (!IsReadableAddress(inputService, 0x30)) {
        return;
    }

    void* begin = nullptr;
    void* end = nullptr;
    if (!ReadOffset(inputService, 0x08, begin) || !ReadOffset(inputService, 0x10, end)) {
        return;
    }

    const auto beginAddress = reinterpret_cast<std::uintptr_t>(begin);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(end);
    if (beginAddress == 0 || endAddress < beginAddress) {
        return;
    }

    constexpr std::uintptr_t kEntryStride = 0x10;
    constexpr std::uintptr_t kMaxInputEntries = 1024;
    const std::uintptr_t entryCount = (endAddress - beginAddress) / kEntryStride;
    if (entryCount == 0 || entryCount > kMaxInputEntries) {
        return;
    }

    for (std::uintptr_t index = 0; index < entryCount; ++index) {
        void* entry = nullptr;
        if (!ReadOffset(begin, index * kEntryStride, entry) || entry == nullptr) {
            continue;
        }

        ReleaseInputEntry(inputService, static_cast<int>(index), entry);
    }
}

void UpdateMenuOpenResetState() {
    constexpr ULONGLONG kCloseDrainMilliseconds = 220;
    const ULONGLONG now = GetTickCount64();

    if (!tane::gui::IsMenuOpen()) {
        g_resetInputForCurrentOpen.store(false, std::memory_order_release);
        if (now >= g_blockInputUntilTick.load(std::memory_order_acquire)) {
            g_blockInputUntilTick.store(0, std::memory_order_release);
        }
        return;
    }

    g_blockInputUntilTick.store(now + kCloseDrainMilliseconds, std::memory_order_release);
    g_resetInputForCurrentOpen.store(true, std::memory_order_release);
    ResetInternalInputServiceState();
}

bool __fastcall HookProcessAnalogInputState(void* inputEntry, void* analogValues, void* analogState) {
    if (ShouldBlockInternalInput()) {
        ZeroAnalogValues(analogValues);
        g_allowAnalogReleaseDispatch = true;
        if (g_originalProcessAnalogInputState != nullptr) {
            g_originalProcessAnalogInputState(inputEntry, analogValues, analogState);
        }
        g_allowAnalogReleaseDispatch = false;
        ClearAnalogInputState(analogState);
        return false;
    }

    const bool result = g_originalProcessAnalogInputState != nullptr
        ? g_originalProcessAnalogInputState(inputEntry, analogValues, analogState)
        : false;

    if (ShouldBlockInternalInput()) {
        ClearAnalogInputState(analogState);
        return false;
    }

    return result;
}

void __fastcall HookButtonPressDispatch(void* self, int inputId, bool value) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalButtonPressDispatch != nullptr) {
        g_originalButtonPressDispatch(self, inputId, value);
    }
}

void __fastcall HookButtonTransitionDispatch(void* self, int inputId, bool value) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalButtonTransitionDispatch != nullptr) {
        g_originalButtonTransitionDispatch(self, inputId, value);
    }
}

void __fastcall HookButtonStateDispatch(void* self, int inputId, bool value) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalButtonStateDispatch != nullptr) {
        g_originalButtonStateDispatch(self, inputId, value);
    }
}

void __fastcall HookButtonAnalogDispatch(void* self, int inputId, int buttonMask, bool active, bool pressed) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalButtonAnalogDispatch != nullptr) {
        g_originalButtonAnalogDispatch(self, inputId, buttonMask, active, pressed);
    }
}

void __fastcall HookAnalogVectorDispatch(void* self, int inputId, bool axis, int state, float x, float y) {
    if (ShouldBlockInternalInput() && !(g_allowAnalogReleaseDispatch && state == 2)) {
        return;
    }

    if (g_originalAnalogVectorDispatch != nullptr) {
        g_originalAnalogVectorDispatch(self, inputId, axis, state, x, y);
    }
}

void __fastcall HookAnalogDispatch(void* self, int inputId, bool axis, float value) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalAnalogDispatch != nullptr) {
        g_originalAnalogDispatch(self, inputId, axis, value);
    }
}

void __fastcall HookPointerMotionDispatch(int eventType, int pointerId, int x, int y) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalPointerMotionDispatch != nullptr) {
        g_originalPointerMotionDispatch(eventType, pointerId, x, y);
    }
}

void __fastcall HookPointerButtonDispatch(int eventType, int pointerId, int x, int y) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalPointerButtonDispatch != nullptr) {
        g_originalPointerButtonDispatch(eventType, pointerId, x, y);
    }
}

void __fastcall HookUiNavigationDispatch(int eventType, int pointerId, int x, int y, int flags) {
    if (ShouldBlockInternalInput()) {
        return;
    }

    if (g_originalUiNavigationDispatch != nullptr) {
        g_originalUiNavigationDispatch(eventType, pointerId, x, y, flags);
    }
}

template <typename T>
bool ReadValue(const void* address, T& value) {
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

template <typename T>
bool ReadOffset(const void* base, std::size_t offset, T& value) {
    if (base == nullptr) {
        return false;
    }

    return ReadValue(reinterpret_cast<const std::uint8_t*>(base) + offset, value);
}

template <typename T>
void WriteOffset(void* base, std::size_t offset, T value) {
    if (base == nullptr) {
        return;
    }

    void* address = reinterpret_cast<std::uint8_t*>(base) + offset;
    if (!IsWritableAddress(address, sizeof(T))) {
        return;
    }

    __try {
        *reinterpret_cast<T*>(address) = value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void ZeroMemoryRegion(void* address, std::size_t size) {
    if (!IsWritableAddress(address, size)) {
        return;
    }

    __try {
        SecureZeroMemory(address, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void ClearVectorStorage(void* owner, std::size_t offset, std::size_t maxBytes) {
    void* begin = nullptr;
    void* end = nullptr;
    if (!ReadOffset(owner, offset, begin) || !ReadOffset(owner, offset + sizeof(void*), end)) {
        return;
    }

    const auto beginAddress = reinterpret_cast<std::uintptr_t>(begin);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(end);
    if (beginAddress == 0 || endAddress < beginAddress) {
        return;
    }

    const std::uintptr_t byteCount = endAddress - beginAddress;
    if (byteCount == 0 || byteCount > maxBytes) {
        return;
    }

    ZeroMemoryRegion(begin, static_cast<std::size_t>(byteCount));
}

void ClearAnalogInputState(void* analogState) {
    if (!IsReadableAddress(analogState, 0xB0)) {
        return;
    }

    constexpr std::size_t kMaxAnalogStateBytes = 0x4000;
    ClearVectorStorage(analogState, 0x48, kMaxAnalogStateBytes);
    ClearVectorStorage(analogState, 0x60, kMaxAnalogStateBytes);
    ClearVectorStorage(analogState, 0x78, kMaxAnalogStateBytes);
    ClearVectorStorage(analogState, 0x98, kMaxAnalogStateBytes);
}

void ZeroAnalogValues(void* analogValues) {
    WriteOffset<float>(analogValues, 0x0C, 0.0f);
    WriteOffset<float>(analogValues, 0x10, 0.0f);
    WriteOffset<float>(analogValues, 0x14, 0.0f);
    WriteOffset<float>(analogValues, 0x18, 0.0f);
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
        return reinterpret_cast<GetLocalPlayerFn>(function)(clientInstance);
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
    if (!ResolvePackedComponentId(storage, entityId, packedComponentId) ||
        !ReadOffset(storage, 0x50, componentPages)) {
        return nullptr;
    }

    const std::size_t pageOffset = ((packedComponentId >> 4) & 0x3FF8);
    void* componentPage = nullptr;
    if (!ReadOffset(componentPages, pageOffset, componentPage) || componentPage == nullptr) {
        return nullptr;
    }

    const std::size_t entryOffset = (packedComponentId & 0x7F) * entrySize;
    void* component = reinterpret_cast<std::uint8_t*>(componentPage) + entryOffset;
    return IsReadableAddress(component, entrySize) ? component : nullptr;
}

void* ResolveComponent(void* actor, std::uint32_t componentHash, std::size_t entrySize) {
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

    void* slot = reinterpret_cast<void*>(begin + ((slotCount - 1) & componentHash) * sizeof(void*));
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

        if (hash == componentHash) {
            if (entry == hashSentinel) {
                return nullptr;
            }

            void* storage = nullptr;
            return ReadOffset(entry, 0x10, storage) && storage != nullptr
                ? ResolveComponentFromStorage(storage, entityId, entrySize)
                : nullptr;
        }

        slot = entry;
    }

    return nullptr;
}

void ClearMoveInputState(void* state) {
    ZeroMemoryRegion(state, kMoveInputStateSize);
}

void ClearMoveInputComponent(void* component) {
    if (!IsReadableAddress(component, kMoveInputComponentEntrySize)) {
        return;
    }

    ClearMoveInputState(component);
    ClearMoveInputState(reinterpret_cast<std::uint8_t*>(component) + 0x10);
    ZeroMemoryRegion(reinterpret_cast<std::uint8_t*>(component) + kMoveInputTransientOffset, kMoveInputTransientSize);
}

void ClearRawMoveInputComponent(void* component) {
    ZeroMemoryRegion(component, kRawMoveInputComponentEntrySize);
}

}  // namespace

bool InstallInputBlockHooks() {
    if (g_internalHooksInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    void* processAnalogInputState = ResolveModuleRva(tane::offsets::input::kProcessAnalogInputStateRva);
    void* buttonPressDispatch = ResolveModuleRva(tane::offsets::input::kButtonPressDispatchRva);
    void* buttonTransitionDispatch = ResolveModuleRva(tane::offsets::input::kButtonTransitionDispatchRva);
    void* buttonStateDispatch = ResolveModuleRva(tane::offsets::input::kButtonStateDispatchRva);
    void* buttonAnalogDispatch = ResolveModuleRva(tane::offsets::input::kButtonAnalogDispatchRva);
    void* analogVectorDispatch = ResolveModuleRva(tane::offsets::input::kAnalogVectorDispatchRva);
    void* analogDispatch = ResolveModuleRva(tane::offsets::input::kAnalogDispatchRva);
    void* pointerMotionDispatch = ResolveModuleRva(tane::offsets::input::kPointerMotionDispatchRva);
    void* pointerButtonDispatch = ResolveModuleRva(tane::offsets::input::kPointerButtonDispatchRva);
    void* uiNavigationDispatch = ResolveModuleRva(tane::offsets::input::kUiNavigationDispatchRva);

    bool processAnalogInputStateInstalled = g_originalProcessAnalogInputState != nullptr;
    if (!processAnalogInputStateInstalled) {
        processAnalogInputStateInstalled = CreateAndEnableHook(
            processAnalogInputState,
            reinterpret_cast<void*>(&HookProcessAnalogInputState),
            reinterpret_cast<void**>(&g_originalProcessAnalogInputState));
    }

    bool buttonPressDispatchInstalled = g_originalButtonPressDispatch != nullptr;
    if (!buttonPressDispatchInstalled) {
        buttonPressDispatchInstalled = CreateAndEnableHook(
            buttonPressDispatch,
            reinterpret_cast<void*>(&HookButtonPressDispatch),
            reinterpret_cast<void**>(&g_originalButtonPressDispatch));
    }

    bool buttonTransitionDispatchInstalled = g_originalButtonTransitionDispatch != nullptr;
    if (!buttonTransitionDispatchInstalled) {
        buttonTransitionDispatchInstalled = CreateAndEnableHook(
            buttonTransitionDispatch,
            reinterpret_cast<void*>(&HookButtonTransitionDispatch),
            reinterpret_cast<void**>(&g_originalButtonTransitionDispatch));
    }

    bool buttonStateDispatchInstalled = g_originalButtonStateDispatch != nullptr;
    if (!buttonStateDispatchInstalled) {
        buttonStateDispatchInstalled = CreateAndEnableHook(
            buttonStateDispatch,
            reinterpret_cast<void*>(&HookButtonStateDispatch),
            reinterpret_cast<void**>(&g_originalButtonStateDispatch));
    }

    bool buttonAnalogDispatchInstalled = g_originalButtonAnalogDispatch != nullptr;
    if (!buttonAnalogDispatchInstalled) {
        buttonAnalogDispatchInstalled = CreateAndEnableHook(
            buttonAnalogDispatch,
            reinterpret_cast<void*>(&HookButtonAnalogDispatch),
            reinterpret_cast<void**>(&g_originalButtonAnalogDispatch));
    }

    bool analogVectorDispatchInstalled = g_originalAnalogVectorDispatch != nullptr;
    if (!analogVectorDispatchInstalled) {
        analogVectorDispatchInstalled = CreateAndEnableHook(
            analogVectorDispatch,
            reinterpret_cast<void*>(&HookAnalogVectorDispatch),
            reinterpret_cast<void**>(&g_originalAnalogVectorDispatch));
    }

    bool analogDispatchInstalled = g_originalAnalogDispatch != nullptr;
    if (!analogDispatchInstalled) {
        analogDispatchInstalled = CreateAndEnableHook(
            analogDispatch,
            reinterpret_cast<void*>(&HookAnalogDispatch),
            reinterpret_cast<void**>(&g_originalAnalogDispatch));
    }

    bool pointerMotionDispatchInstalled = g_originalPointerMotionDispatch != nullptr;
    if (!pointerMotionDispatchInstalled) {
        pointerMotionDispatchInstalled = CreateAndEnableHook(
            pointerMotionDispatch,
            reinterpret_cast<void*>(&HookPointerMotionDispatch),
            reinterpret_cast<void**>(&g_originalPointerMotionDispatch));
    }

    bool pointerButtonDispatchInstalled = g_originalPointerButtonDispatch != nullptr;
    if (!pointerButtonDispatchInstalled) {
        pointerButtonDispatchInstalled = CreateAndEnableHook(
            pointerButtonDispatch,
            reinterpret_cast<void*>(&HookPointerButtonDispatch),
            reinterpret_cast<void**>(&g_originalPointerButtonDispatch));
    }

    bool uiNavigationDispatchInstalled = g_originalUiNavigationDispatch != nullptr;
    if (!uiNavigationDispatchInstalled) {
        uiNavigationDispatchInstalled = CreateAndEnableHook(
            uiNavigationDispatch,
            reinterpret_cast<void*>(&HookUiNavigationDispatch),
            reinterpret_cast<void**>(&g_originalUiNavigationDispatch));
    }

    const bool installed = (processAnalogInputState == nullptr || processAnalogInputStateInstalled) &&
        (buttonPressDispatch == nullptr || buttonPressDispatchInstalled) &&
        (buttonTransitionDispatch == nullptr || buttonTransitionDispatchInstalled) &&
        (buttonStateDispatch == nullptr || buttonStateDispatchInstalled) &&
        (buttonAnalogDispatch == nullptr || buttonAnalogDispatchInstalled) &&
        (analogVectorDispatch == nullptr || analogVectorDispatchInstalled) &&
        (analogDispatch == nullptr || analogDispatchInstalled) &&
        (pointerMotionDispatch == nullptr || pointerMotionDispatchInstalled) &&
        (pointerButtonDispatch == nullptr || pointerButtonDispatchInstalled) &&
        (uiNavigationDispatch == nullptr || uiNavigationDispatchInstalled);
    g_internalHooksInstalled.store(installed, std::memory_order_release);
    return installed;
}

void TickInputBlock(void* clientInstance) {
    UpdateMenuOpenResetState();

    if (!tane::gui::IsMenuOpen() || !IsReadableAddress(clientInstance, sizeof(void*))) {
        return;
    }

    const ULONGLONG now = GetTickCount64();
    ULONGLONG lastClearTick = g_lastComponentClearTick.load(std::memory_order_acquire);
    if (lastClearTick == now) {
        return;
    }
    if (!g_lastComponentClearTick.compare_exchange_strong(lastClearTick, now, std::memory_order_acq_rel)) {
        return;
    }

    void* localPlayer = GetLocalPlayer(clientInstance);
    if (!IsReadableAddress(localPlayer, sizeof(void*))) {
        return;
    }

    void* moveInput = ResolveComponent(localPlayer, kMoveInputComponentHash, kMoveInputComponentEntrySize);
    if (moveInput != nullptr) {
        ClearMoveInputComponent(moveInput);
    }

    void* rawMoveInput = ResolveComponent(localPlayer, kRawMoveInputComponentHash, kRawMoveInputComponentEntrySize);
    if (rawMoveInput != nullptr) {
        ClearRawMoveInputComponent(rawMoveInput);
    }
}

}  // namespace tane::imgui_menu
