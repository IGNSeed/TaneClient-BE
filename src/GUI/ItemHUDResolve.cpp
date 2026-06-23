bool ReadStdStringValue(const void* stringObject, std::string& value) {
    if (!IsReadableAddress(stringObject, 32)) {
        return false;
    }

    std::uint64_t size = 0;
    std::uint64_t capacity = 0;
    if (!ReadOffset(stringObject, 16, size) ||
        !ReadOffset(stringObject, 24, capacity) ||
        size == 0 ||
        size > 64 ||
        capacity > 256) {
        return false;
    }

    const char* data = reinterpret_cast<const char*>(stringObject);
    if (capacity >= 16) {
        if (!ReadValue(stringObject, data)) {
            return false;
        }
    }

    if (!IsReadableAddress(data, static_cast<std::size_t>(size))) {
        return false;
    }

    value.assign(data, data + size);
    return std::all_of(value.begin(), value.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte >= 0x20 && byte < 0x7F;
    });
}

std::string GetItemStackItemName(void* itemStack) {
    void* itemHolder = nullptr;
    void* item = nullptr;
    if (!ReadOffset(itemStack, kItemPointerOffset, itemHolder) ||
        !ReadValue(itemHolder, item) ||
        item == nullptr) {
        return {};
    }

    for (std::size_t offset : kItemNameOffsetCandidates) {
        std::string name;
        if (ReadStdStringValue(reinterpret_cast<const std::uint8_t*>(item) + offset, name)) {
            return name;
        }
    }

    return {};
}

std::string GetItemObjectName(void* item) {
    if (item == nullptr) {
        return {};
    }

    for (std::size_t offset : kItemNameOffsetCandidates) {
        std::string name;
        if (ReadStdStringValue(reinterpret_cast<const std::uint8_t*>(item) + offset, name)) {
            return name;
        }
    }

    return {};
}

bool IsReadableMemoryRegion(const MEMORY_BASIC_INFORMATION& info) {
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const DWORD protection = info.Protect & 0xFF;
    return protection == PAGE_READONLY ||
        protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

bool AreEditorPreviewItemsResolved() {
    return std::all_of(g_editorPreviewItems.begin(), g_editorPreviewItems.end(), [](void* item) {
        return item != nullptr;
    });
}

bool SafeBytesEqual(const std::uint8_t* address, const char* text, std::size_t length) {
    __try {
        return std::memcmp(address, text, length) == 0 && address[length] == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SafeReadPointerInKnownReadableRegion(std::uintptr_t address, const void*& value) {
    __try {
        value = *reinterpret_cast<const void* const*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = nullptr;
        return false;
    }
}

std::string_view StripMinecraftNamespace(std::string_view value) {
    constexpr std::string_view kMinecraftNamespace = "minecraft:";
    return value.rfind(kMinecraftNamespace, 0) == 0 ? value.substr(kMinecraftNamespace.size()) : value;
}

bool ItemNameMatches(std::string_view resolvedName, std::string_view expectedName) {
    return resolvedName == expectedName || StripMinecraftNamespace(resolvedName) == StripMinecraftNamespace(expectedName);
}

bool LooksLikeItemObject(void* item, const char* expectedName) {
    if (item == nullptr || expectedName == nullptr || !IsReadableAddress(item, sizeof(void*))) {
        return false;
    }

    void** vtable = nullptr;
    if (!ReadValue(item, vtable) || !IsReadableAddress(vtable, sizeof(void*) * 0x20)) {
        return false;
    }

    const std::string resolvedName = GetItemObjectName(item);
    return ItemNameMatches(resolvedName, expectedName);
}

void* ResolveItemFromWeakCounter(std::uintptr_t counter, const char* expectedName) {
    if (counter == 0 || expectedName == nullptr || !IsReadableAddress(reinterpret_cast<void*>(counter), sizeof(void*))) {
        return nullptr;
    }

    void* item = nullptr;
    if (!ReadValue(reinterpret_cast<void*>(counter), item) || !LooksLikeItemObject(item, expectedName)) {
        return nullptr;
    }

    return item;
}

std::shared_ptr<void> LockItemRegistryFromPlayer(void* localPlayer) {
    void* level = nullptr;
    if (!ReadOffset(localPlayer, kActorLevelOffset, level) ||
        !IsReadableAddress(
            reinterpret_cast<const std::uint8_t*>(level) + kLevelItemRegistryRefOffset,
            sizeof(GameItemRegistryRef))) {
        return {};
    }

    const auto* registryRef =
        reinterpret_cast<const GameItemRegistryRef*>(reinterpret_cast<const std::uint8_t*>(level) + kLevelItemRegistryRefOffset);
    return registryRef->weakRegistry.lock();
}

bool IsPlausibleItemNameMap(const ItemNameMap* map) {
    if (!IsReadableAddress(map, sizeof(ItemNameMap))) {
        return false;
    }

    const std::size_t size = map->size();
    return size > 0 && size < 20000;
}

void* LookupNativeItemCounterInMap(
    void* itemRegistry,
    std::size_t mapOffset,
    const char* name,
    void*& item) {
    item = nullptr;
    if (itemRegistry == nullptr || name == nullptr || name[0] == '\0') {
        return nullptr;
    }

    const auto* map =
        reinterpret_cast<const ItemNameMap*>(reinterpret_cast<const std::uint8_t*>(itemRegistry) + mapOffset);
    if (!IsPlausibleItemNameMap(map)) {
        return nullptr;
    }

    const GameHashedString key(name);
    const auto iterator = map->find(key);
    if (iterator == map->end()) {
        return nullptr;
    }

    void* resolvedItem = ResolveItemFromWeakCounter(iterator->second, name);
    if (resolvedItem == nullptr) {
        return nullptr;
    }

    item = resolvedItem;
    return reinterpret_cast<void*>(iterator->second);
}

void* LookupNativeItemCounter(void* itemRegistry, const char* shortName, void*& item) {
    item = nullptr;
    if (shortName == nullptr || shortName[0] == '\0') {
        return nullptr;
    }

    char namespacedName[96]{};
    if (sprintf_s(namespacedName, "minecraft:%s", shortName) < 0) {
        return nullptr;
    }

    if (void* counter = LookupNativeItemCounterInMap(
            itemRegistry,
            kItemRegistryNameToItemMapOffset,
            namespacedName,
            item);
        counter != nullptr) {
        return counter;
    }

    if (void* counter = LookupNativeItemCounterInMap(
            itemRegistry,
            kItemRegistryNameToItemMapOffset,
            shortName,
            item);
        counter != nullptr) {
        return counter;
    }

    return LookupNativeItemCounterInMap(itemRegistry, kItemRegistryTileItemNameToItemMapOffset, shortName, item);
}

void* FindFirstPattern(HMODULE module, const std::initializer_list<std::string_view>& patterns);

ItemRegistryLookupByNameFn ResolveItemRegistryLookupByName() {
    if (g_itemRegistryLookupByName != nullptr) {
        return g_itemRegistryLookupByName;
    }

    HMODULE module = GetModuleHandleW(L"Minecraft.Windows.exe");
    if (module == nullptr) {
        module = GetModuleHandleW(nullptr);
    }

    void* function = FindFirstPattern(
        module,
        {
            "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 4C 8B EA 48 89 54 24 ? 48 89 4C 24 ? 48 89 4D",
        });
    if (function != nullptr) {
        g_itemRegistryLookupByName = reinterpret_cast<ItemRegistryLookupByNameFn>(function);
    }

    return g_itemRegistryLookupByName;
}

void*** SafeLookupNativeItemByName(
    ItemRegistryLookupByNameFn lookupByName,
    void* resultA,
    void* resultB,
    TextHolder& text) {
    if (lookupByName == nullptr) {
        return nullptr;
    }

    __try {
        return lookupByName(resultA, resultB, text);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void* LookupNativeItemByName(const char* name) {
    ItemRegistryLookupByNameFn lookupByName = ResolveItemRegistryLookupByName();
    if (lookupByName == nullptr || name == nullptr || name[0] == '\0') {
        return nullptr;
    }

    TextHolder text(name);
    std::uintptr_t resultAStorage = 0;
    std::uintptr_t resultBStorage = 0;
    void*** result = SafeLookupNativeItemByName(lookupByName, &resultAStorage, &resultBStorage, text);

    if (result == nullptr || !IsReadableAddress(result, sizeof(void*))) {
        return nullptr;
    }

    void** itemHolder = nullptr;
    if (!ReadValue(result, itemHolder) || !IsReadableAddress(itemHolder, sizeof(void*))) {
        return nullptr;
    }

    void* item = nullptr;
    if (!ReadValue(itemHolder, item) || !LooksLikeItemObject(item, name)) {
        return nullptr;
    }

    return item;
}

void ResolveEditorPreviewItems(void* localPlayer) {
    if (AreEditorPreviewItemsResolved()) {
        return;
    }

    g_editorPreviewItemScanAttempted = true;

    std::shared_ptr<void> registry = LockItemRegistryFromPlayer(localPlayer);
    if (!registry) {
        return;
    }

    for (std::size_t itemIndex = 0; itemIndex < kEditorPreviewItemNames.size(); ++itemIndex) {
        if (g_editorPreviewItems[itemIndex] != nullptr && g_editorPreviewItemHolders[itemIndex] != nullptr) {
            continue;
        }

        void* item = nullptr;
        void* counter = LookupNativeItemCounter(registry.get(), kEditorPreviewItemNames[itemIndex], item);
        g_editorPreviewItems[itemIndex] = item;
        g_editorPreviewItemHolders[itemIndex] = counter;
    }
}

bool GetScreenViewRootLayerName(void* screenView, std::string& layerName) {
    void* visualTree = nullptr;
    void* rootControl = nullptr;
    if (!ReadOffset(screenView, kScreenViewVisualTreeOffset, visualTree) ||
        !ReadOffset(visualTree, kVisualTreeRootOffset, rootControl) ||
        rootControl == nullptr) {
        return false;
    }

    for (std::size_t offset : kUIControlLayerNameOffsetCandidates) {
        std::string candidate;
        if (ReadStdStringValue(reinterpret_cast<const std::uint8_t*>(rootControl) + offset, candidate)) {
            layerName = std::move(candidate);
            return true;
        }
    }

    return false;
}

bool IsHudLayerName(const std::string& layerName) {
    return layerName == "hud_screen";
}

std::string ToLowerLayerName(std::string layerName) {
    for (char& c : layerName) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return layerName;
}

enum class BlockingGuiKind {
    None,
    Gameplay,
    Form,
};

BlockingGuiKind GetBlockingGuiKind(const std::string& layerName) {
    const std::string normalizedLayerName = ToLowerLayerName(layerName);
    if (normalizedLayerName == "pause_screen" ||
        normalizedLayerName == "start_screen" ||
        normalizedLayerName == "title_screen") {
        return BlockingGuiKind::Gameplay;
    }

    constexpr std::string_view kFormTokens[] = {
        "server_form",
        "serverform",
        "server form",
        "custom_form",
        "modal_form",
        "modalform",
        "action_form",
        "actionform",
        "form_screen",
        "modal_screen",
        "third_party_server_screen",
        "form.",
        "#form_",
        "showmodalform",
    };

    for (std::string_view token : kFormTokens) {
        if (normalizedLayerName.find(token) != std::string::npos) {
            return BlockingGuiKind::Form;
        }
    }

    constexpr std::string_view kGameplayBlockingTokens[] = {
        "chat",
        "open_chat",
        "gameplay/chat",
        "chat_screen",
        "chat_screen_messages",
        "menu",
        "inventory",
        "container",
        "crafting",
        "chest",
        "barrel",
        "shulker",
        "furnace",
        "hopper",
        "dispenser",
        "dropper",
        "anvil",
        "enchant",
        "grindstone",
        "loom",
        "stonecutter",
        "cartography",
        "brewing",
        "smithing",
        "beacon",
        "horse",
        "merchant",
        "trade",
        "crafter",
    };

    for (std::string_view token : kGameplayBlockingTokens) {
        if (normalizedLayerName.find(token) != std::string::npos) {
            return BlockingGuiKind::Gameplay;
        }
    }

    return BlockingGuiKind::None;
}

void ClearHudRenderCache();

void BlockGameplayModulesForClientTransition() {
    const DWORD64 now = GetTickCount64();
    g_guiOverlayBlockedUntilTick.store(now + kClientTransitionBlockingGraceMs, std::memory_order_relaxed);
    g_guiOverlayAllowedUntilTick.store(0, std::memory_order_relaxed);
    ClearHudRenderCache();
}

bool IsHudScreenView(void* screenView) {
    std::string layerName;
    return GetScreenViewRootLayerName(screenView, layerName) && IsHudLayerName(layerName);
}

bool UpdateGuiScreenVisibilityForScreenView(void* screenView) {
    std::string layerName;
    if (!GetScreenViewRootLayerName(screenView, layerName)) {
        return false;
    }

    const bool isHudScreen = IsHudLayerName(layerName);
    const BlockingGuiKind blockingGuiKind = isHudScreen ? BlockingGuiKind::None : GetBlockingGuiKind(layerName);
    const bool hasBlockingGui = blockingGuiKind != BlockingGuiKind::None;
    if (isHudScreen && !hasBlockingGui) {
        g_guiOverlayAllowedUntilTick.store(GetTickCount64() + kGuiOverlayVisibilityGraceMs, std::memory_order_relaxed);
    } else if (hasBlockingGui) {
        const DWORD64 now = GetTickCount64();
        const DWORD64 blockMs =
            blockingGuiKind == BlockingGuiKind::Form ? kFormGuiOverlayBlockingGraceMs : kGuiOverlayBlockingGraceMs;
        g_guiOverlayBlockedUntilTick.store(now + blockMs, std::memory_order_relaxed);
        g_guiOverlayAllowedUntilTick.store(0, std::memory_order_relaxed);
        ClearHudRenderCache();
    }
    return isHudScreen && !hasBlockingGui;
}

bool ShouldDrawItemHudForFrame(void* screenView) {
    const std::uint64_t frameId = g_uiFrameId.load(std::memory_order_relaxed);
    if (frameId != 0 && g_lastHudDrawFrameId == frameId) {
        return false;
    }

    if (screenView != g_cachedHudScreenView) {
        if (!IsHudScreenView(screenView)) {
            return false;
        }

        if (g_cachedHudScreenView != nullptr) {
            ClearHudRenderCache();
        }
        g_cachedHudScreenView = screenView;
    }

    g_lastHudDrawFrameId = frameId;
    return true;
}

bool HasIntrinsicItemGlint(void* itemStack) {
    const std::string name = GetItemStackItemName(itemStack);
    return name == "appleenchanted" ||
        name == "minecraft:appleenchanted" ||
        name == "enchanted_golden_apple" ||
        name == "minecraft:enchanted_golden_apple";
}

bool ShouldRenderGlint(void* itemStack) {
    return SafeIsItemStackEnchanted(itemStack) || HasIntrinsicItemGlint(itemStack);
}

bool BuildRenderEntry(void* itemStack, std::array<std::uint8_t, kItemRenderEntrySize>& entry) {
    if (itemStack == nullptr) {
        return false;
    }

    __try {
        std::memcpy(entry.data(), itemStack, entry.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    entry[kItemRenderEntryGlintFlagOffset] = ShouldRenderGlint(itemStack) ? 1 : 0;
    return true;
}

bool CopyLiveItemStackToRenderEntry(
    void* itemStack,
    std::array<std::uint8_t, kItemRenderEntrySize>& entry,
    std::uint8_t glintFlag) {
    if (itemStack == nullptr) {
        return false;
    }

    __try {
        std::memcpy(entry.data(), itemStack, entry.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    entry[kItemRenderEntryGlintFlagOffset] = glintFlag;
    return true;
}

bool ResolveNativeTextFunctions(void* minecraftUiRenderContext) {
    void** vtable = nullptr;
    if (!ReadValue(minecraftUiRenderContext, vtable) || vtable == nullptr) {
        return false;
    }

    if (vtable == g_failedTextRenderContextVtable) {
        return false;
    }

    if (vtable == g_cachedTextRenderContextVtable && g_cachedDrawDebugText != nullptr && g_cachedFlushText != nullptr) {
        return true;
    }

    void* drawDebugText = nullptr;
    void* flushText = nullptr;
    if (!ReadOffset(vtable, kMinecraftUiRenderContextDrawDebugTextVtableOffset, drawDebugText) ||
        !ReadOffset(vtable, kMinecraftUiRenderContextFlushTextVtableOffset, flushText) ||
        !IsExecutableAddress(drawDebugText) ||
        !IsExecutableAddress(flushText)) {
        g_cachedTextRenderContextVtable = nullptr;
        g_cachedDrawDebugText = nullptr;
        g_cachedFlushText = nullptr;
        return false;
    }

    g_cachedTextRenderContextVtable = vtable;
    g_cachedDrawDebugText = reinterpret_cast<DrawDebugTextFn>(drawDebugText);
    g_cachedFlushText = reinterpret_cast<FlushTextFn>(flushText);
    return true;
}

bool SafeDrawDebugText(
    void* minecraftUiRenderContext,
    const RectangleArea& rect,
    const std::string& text,
    const MceColor& color,
    TextAlignment alignment,
    float textSize) {
    if (g_cachedDrawDebugText == nullptr || text.empty()) {
        return false;
    }

    const TextMeasureData textData{textSize, 0.0f, true, false, false};
    const CaretMeasureData caretData{-1, true};
    __try {
        g_cachedDrawDebugText(minecraftUiRenderContext, rect, text, color, 1.0f, alignment, textData, caretData);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_failedTextRenderContextVtable = g_cachedTextRenderContextVtable;
        g_cachedTextRenderContextVtable = nullptr;
        g_cachedDrawDebugText = nullptr;
        g_cachedFlushText = nullptr;
        return false;
    }
}

void SafeFlushText(void* minecraftUiRenderContext) {
    if (g_cachedFlushText == nullptr) {
        return;
    }

    __try {
        g_cachedFlushText(minecraftUiRenderContext, 0.0f, OptionalFloat{});
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_failedTextRenderContextVtable = g_cachedTextRenderContextVtable;
        g_cachedTextRenderContextVtable = nullptr;
        g_cachedDrawDebugText = nullptr;
        g_cachedFlushText = nullptr;
    }
}

bool ResolveNativeRectangleFunctions(void* minecraftUiRenderContext) {
    if (!IsReadableAddress(minecraftUiRenderContext, sizeof(void*))) {
        return false;
    }

    void** vtable = nullptr;
    if (!ReadValue(minecraftUiRenderContext, vtable) || !IsReadableAddress(vtable, sizeof(void*) * 0x20)) {
        return false;
    }

    if (vtable == g_cachedRectangleRenderContextVtable && g_cachedFillRectangle != nullptr) {
        return true;
    }

    void* fillRectangle = nullptr;
    if (!ReadOffset(vtable, kMinecraftUiRenderContextFillRectangleVtableOffset, fillRectangle) ||
        !IsExecutableAddress(fillRectangle)) {
        g_cachedRectangleRenderContextVtable = nullptr;
        g_cachedFillRectangle = nullptr;
        return false;
    }

    g_cachedRectangleRenderContextVtable = vtable;
    g_cachedFillRectangle = reinterpret_cast<FillRectangleFn>(fillRectangle);
    return true;
}

bool SafeFillRectangle(void* minecraftUiRenderContext, const RectangleArea& rect, const MceColor& color, float alpha) {
    if (!ResolveNativeRectangleFunctions(minecraftUiRenderContext) || g_cachedFillRectangle == nullptr) {
        return false;
    }

    __try {
        g_cachedFillRectangle(minecraftUiRenderContext, rect, color, alpha);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_cachedRectangleRenderContextVtable = nullptr;
        g_cachedFillRectangle = nullptr;
        return false;
    }
}

bool DrawNativeItemCountText(void* minecraftUiRenderContext, const std::string& text, float x, float y, float scale) {
    if (text.empty()) {
        return false;
    }

    const MceColor textColor{1.0f, 1.0f, 1.0f, 1.0f};
    const RectangleArea textRect{
        x,
        x + (kItemSize + 1.0f) * scale,
        y + kItemSize * scale * 0.5f,
        y + (kItemSize + 2.0f) * scale,
    };
    return SafeDrawDebugText(
        minecraftUiRenderContext,
        textRect,
        text,
        textColor,
        TextAlignment::Right,
        kNativeCountTextSize * scale);
}

bool DrawNativeDurabilityText(
    void* minecraftUiRenderContext,
    const std::string& text,
    const MceColor& color,
    float x,
    float y,
    float scale) {
    if (text.empty()) {
        return false;
    }

    const float textLeft = x + kDurabilityTextXOffset * scale;
    const RectangleArea textRect{
        textLeft,
        textLeft + kDurabilityTextWidth * scale,
        y + kDurabilityTextYOffset * scale,
        y + (kItemSize + 3.0f) * scale,
    };
    return SafeDrawDebugText(
        minecraftUiRenderContext,
        textRect,
        text,
        color,
        TextAlignment::Left,
        kNativeDurabilityTextSize * scale);
}

std::vector<int> ParsePattern(std::string_view pattern) {
    std::vector<int> bytes;
    for (std::size_t i = 0; i < pattern.size();) {
        while (i < pattern.size() && std::isspace(static_cast<unsigned char>(pattern[i])) != 0) {
            ++i;
        }

        if (i >= pattern.size()) {
            break;
        }

        if (pattern[i] == '?') {
            bytes.push_back(-1);
            while (i < pattern.size() && pattern[i] == '?') {
                ++i;
            }
            continue;
        }

        if (i + 1 >= pattern.size()) {
            break;
        }

        char hex[3] = {pattern[i], pattern[i + 1], '\0'};
        bytes.push_back(static_cast<int>(std::strtoul(hex, nullptr, 16)));
        i += 2;
    }

    return bytes;
}

bool GetTextSection(HMODULE module, const std::uint8_t*& begin, std::size_t& size) {
    if (module == nullptr) {
        return false;
    }

    const auto base = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!IsReadableAddress(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (!IsReadableAddress(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        char name[9]{};
        std::copy_n(section[i].Name, 8, reinterpret_cast<unsigned char*>(name));
        if (std::string_view(name) == ".text") {
            begin = base + section[i].VirtualAddress;
            size = section[i].Misc.VirtualSize;
            return IsReadableAddress(begin, size);
        }
    }

    begin = base;
    size = nt->OptionalHeader.SizeOfImage;
    return IsReadableAddress(begin, size);
}

void* FindPattern(HMODULE module, std::string_view pattern) {
    const std::vector<int> bytes = ParsePattern(pattern);
    if (bytes.empty()) {
        return nullptr;
    }

    const std::uint8_t* begin = nullptr;
    std::size_t size = 0;
    if (!GetTextSection(module, begin, size) || size < bytes.size()) {
        return nullptr;
    }

    for (std::size_t i = 0; i <= size - bytes.size(); ++i) {
        bool matched = true;
        for (std::size_t j = 0; j < bytes.size(); ++j) {
            if (bytes[j] >= 0 && begin[i + j] != static_cast<std::uint8_t>(bytes[j])) {
                matched = false;
                break;
            }
        }

        if (matched) {
            return const_cast<std::uint8_t*>(begin + i);
        }
    }

    return nullptr;
}

void* FindFirstPattern(HMODULE module, const std::initializer_list<std::string_view>& patterns) {
    for (std::string_view pattern : patterns) {
        void* result = FindPattern(module, pattern);
        if (result != nullptr && IsExecutableAddress(result)) {
            return result;
        }
    }

    return nullptr;
}

std::uintptr_t ResolveLocalPlayerVtableOffset(HMODULE module) {
    const auto* probe = reinterpret_cast<const std::uint8_t*>(FindFirstPattern(
        module,
        {
            "49 8B 00 49 8B C8 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 0F",
            "49 8B 00 49 8B C8 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 48 8B C8",
        }));
    if (probe == nullptr || !IsReadableAddress(probe + 9, sizeof(std::int32_t))) {
        return 0;
    }

    std::int32_t offset = 0;
    ReadValue(probe + 9, offset);
    return offset > 0 ? static_cast<std::uintptr_t>(offset) : 0;
}

bool LooksLikeActor(void* actor);

bool TryGetFrameCachedLocalPlayer(void* clientInstance, void*& localPlayer) {
    const std::uint64_t frameId = g_uiFrameId.load(std::memory_order_relaxed);
    if (frameId == 0 ||
        !g_currentUiFrameLocalPlayerResolved ||
        g_currentUiFrameLocalPlayerFrameId != frameId ||
        g_currentUiFrameLocalPlayerClientInstance != clientInstance) {
        return false;
    }

    localPlayer = g_currentUiFrameLocalPlayer;
    return true;
}

void StoreFrameCachedLocalPlayer(void* clientInstance, void* localPlayer) {
    const std::uint64_t frameId = g_uiFrameId.load(std::memory_order_relaxed);
    if (frameId == 0) {
        return;
    }

    g_currentUiFrameLocalPlayerFrameId = frameId;
    g_currentUiFrameLocalPlayerClientInstance = clientInstance;
    g_currentUiFrameLocalPlayer = localPlayer;
    g_currentUiFrameLocalPlayerResolved = true;
}

void* CallClientInstanceGetLocalPlayer(void* clientInstance) {
    if (clientInstance == nullptr) {
        return nullptr;
    }

    void* frameCachedLocalPlayer = nullptr;
    if (TryGetFrameCachedLocalPlayer(clientInstance, frameCachedLocalPlayer)) {
        return frameCachedLocalPlayer;
    }

    if (g_cachedLocalPlayerVtableOffset != 0) {
        if (void* localPlayer = SafeCallClientVfuncNoArg(clientInstance, g_cachedLocalPlayerVtableOffset)) {
            if (LooksLikeActor(localPlayer)) {
                StoreFrameCachedLocalPlayer(clientInstance, localPlayer);
                return localPlayer;
            }
        }

        g_cachedLocalPlayerVtableOffset = 0;
    }

    if (void* localPlayer = SafeCallClientVfuncNoArg(clientInstance, kClientInstanceLocalPlayerVtableOffset)) {
        if (LooksLikeActor(localPlayer)) {
            g_cachedLocalPlayerVtableOffset = kClientInstanceLocalPlayerVtableOffset;
            StoreFrameCachedLocalPlayer(clientInstance, localPlayer);
            return localPlayer;
        }
    }

    if (g_getLocalPlayerVtableOffset != 0) {
        if (void* localPlayer = SafeCallClientVfuncNoArg(clientInstance, g_getLocalPlayerVtableOffset)) {
            if (LooksLikeActor(localPlayer)) {
                g_cachedLocalPlayerVtableOffset = g_getLocalPlayerVtableOffset;
                StoreFrameCachedLocalPlayer(clientInstance, localPlayer);
                return localPlayer;
            }
        }
    }

    for (std::size_t candidate : kLocalPlayerVtableCandidates) {
        if (candidate == g_getLocalPlayerVtableOffset) {
            continue;
        }

        void* localPlayer = SafeCallClientVfuncNoArg(clientInstance, candidate);
        if (LooksLikeActor(localPlayer)) {
            g_cachedLocalPlayerVtableOffset = candidate;
            StoreFrameCachedLocalPlayer(clientInstance, localPlayer);
            return localPlayer;
        }
    }

    StoreFrameCachedLocalPlayer(clientInstance, nullptr);
    return nullptr;
}

bool IsLikelyItemStack(void* itemStack) {
    if (itemStack == nullptr) {
        return false;
    }

    std::uint8_t valid = 0;
    std::uint8_t count = 0;
    if (!ReadOffset(itemStack, kItemValidOffset, valid) ||
        !ReadOffset(itemStack, kItemCountOffset, count) ||
        valid > 1 ||
        count > 127) {
        return false;
    }

    void* itemHolder = nullptr;
    if (!ReadOffset(itemStack, kItemPointerOffset, itemHolder)) {
        return false;
    }

    if (valid == 0 || count == 0) {
        return true;
    }

    void* item = nullptr;
    return ReadValue(itemHolder, item) && item != nullptr;
}
