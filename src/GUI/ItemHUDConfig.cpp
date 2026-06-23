bool BuildItemHudPositionConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\ItemHUD.json", guiPath) >= 0;
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

bool ParseIntAfter(const char* section, const char* key, int& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    if (found == nullptr) {
        return false;
    }

    return std::sscanf(found + 1, " %d", &value) == 1;
}

void SaveItemHudPositionConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildItemHudPositionConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[512]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": 2,\n"
        "  \"enabled\": %s,\n"
        "  \"showDurability\": %s,\n"
        "  \"layout\": %d,\n"
        "  \"durabilityStyle\": %d,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"durabilityCustom\": %s,\n"
        "  \"durabilityX\": %.3f,\n"
        "  \"durabilityY\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        g_itemHudEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_itemHudShowDurability.load(std::memory_order_relaxed) ? "true" : "false",
        std::clamp(g_itemHudLayout.load(std::memory_order_relaxed), 0, 1),
        std::clamp(g_itemHudDurabilityStyle.load(std::memory_order_relaxed), 0, 1),
        g_itemHudCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_itemHudPositionX.load(std::memory_order_relaxed),
        g_itemHudPositionY.load(std::memory_order_relaxed),
        g_itemHudDurabilityCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_itemHudDurabilityPositionX.load(std::memory_order_relaxed),
        g_itemHudDurabilityPositionY.load(std::memory_order_relaxed),
        g_itemHudScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_itemHudPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsureItemHudPositionConfigLoaded() {
    bool expected = false;
    if (!g_itemHudPositionConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildItemHudPositionConfigPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char json[1024]{};
    DWORD read = 0;
    if (ReadFile(file, json, sizeof(json) - 1, &read, nullptr)) {
        json[std::min<DWORD>(read, sizeof(json) - 1)] = '\0';
        bool custom = false;
        bool durabilityCustom = false;
        bool enabled = true;
        bool showDurability = true;
        int layout = static_cast<int>(ItemHudLayout::Vertical);
        int durabilityStyle = static_cast<int>(ItemHudDurabilityStyle::Text);
        float x = 0.0f;
        float y = 0.0f;
        float durabilityX = 0.0f;
        float durabilityY = 0.0f;
        float scale = 1.0f;
        if (ParseBoolAfter(json, "\"enabled\"", enabled)) {
            g_itemHudEnabled.store(enabled, std::memory_order_relaxed);
        }
        if (ParseBoolAfter(json, "\"showDurability\"", showDurability)) {
            g_itemHudShowDurability.store(showDurability, std::memory_order_relaxed);
        }
        if (ParseIntAfter(json, "\"layout\"", layout)) {
            g_itemHudLayout.store(std::clamp(layout, 0, 1), std::memory_order_relaxed);
        }
        if (ParseIntAfter(json, "\"durabilityStyle\"", durabilityStyle)) {
            g_itemHudDurabilityStyle.store(std::clamp(durabilityStyle, 0, 1), std::memory_order_relaxed);
        }
        ParseBoolAfter(json, "\"custom\"", custom);
        if (ParseFloatAfter(json, "\"x\"", x) && ParseFloatAfter(json, "\"y\"", y)) {
            g_itemHudCustomPosition.store(custom, std::memory_order_relaxed);
            g_itemHudPositionX.store(x, std::memory_order_relaxed);
            g_itemHudPositionY.store(y, std::memory_order_relaxed);
        }
        ParseBoolAfter(json, "\"durabilityCustom\"", durabilityCustom);
        if (ParseFloatAfter(json, "\"durabilityX\"", durabilityX) &&
            ParseFloatAfter(json, "\"durabilityY\"", durabilityY)) {
            g_itemHudDurabilityCustomPosition.store(durabilityCustom, std::memory_order_relaxed);
            g_itemHudDurabilityPositionX.store(durabilityX, std::memory_order_relaxed);
            g_itemHudDurabilityPositionY.store(durabilityY, std::memory_order_relaxed);
        }
        if (ParseFloatAfter(json, "\"scale\"", scale)) {
            g_itemHudScale.store(std::clamp(scale, 0.55f, 2.75f), std::memory_order_relaxed);
        }
    }
    CloseHandle(file);
}

float GetClampedItemHudScale() {
    return std::clamp(g_itemHudScale.load(std::memory_order_relaxed), 0.55f, 2.75f);
}

ItemHudLayout GetItemHudLayoutValue() {
    return g_itemHudLayout.load(std::memory_order_relaxed) == static_cast<int>(ItemHudLayout::Horizontal)
        ? ItemHudLayout::Horizontal
        : ItemHudLayout::Vertical;
}

bool IsItemHudHorizontalLayout() {
    return GetItemHudLayoutValue() == ItemHudLayout::Horizontal;
}

ItemHudDurabilityStyle GetItemHudDurabilityStyleValue() {
    return g_itemHudDurabilityStyle.load(std::memory_order_relaxed) == static_cast<int>(ItemHudDurabilityStyle::Bar)
        ? ItemHudDurabilityStyle::Bar
        : ItemHudDurabilityStyle::Text;
}

bool ShouldUseItemHudDurabilityBar() {
    return IsItemHudHorizontalLayout() || GetItemHudDurabilityStyleValue() == ItemHudDurabilityStyle::Bar;
}

std::size_t GetFixedItemHudSlotCount() {
    return kItemSlotCount;
}

float GetItemHudSlotOffset(std::size_t slotIndex, float scale) {
    return static_cast<float>(slotIndex) * kItemSpacing * scale;
}

float GetItemHudDurabilitySlotOffset(std::size_t slotIndex, float scale) {
    constexpr float kHorizontalDurabilitySpacing = 58.0f;
    return static_cast<float>(slotIndex) *
        (IsItemHudHorizontalLayout() ? kHorizontalDurabilitySpacing : kItemSpacing) *
        scale;
}

float GetItemHudItemsNativeWidth(std::size_t slotCount, float scale) {
    if (IsItemHudHorizontalLayout()) {
        return (static_cast<float>(std::max<std::size_t>(1, slotCount) - 1) * kItemSpacing + kItemSize) * scale;
    }
    return kItemSize * scale;
}

float GetItemHudItemsNativeHeight(std::size_t slotCount, float scale) {
    if (IsItemHudHorizontalLayout()) {
        return kItemSize * scale;
    }
    return std::max(
        kItemSize * scale,
        (static_cast<float>(std::max<std::size_t>(1, slotCount) - 1) * kItemSpacing * scale) + kItemSize * scale);
}

float GetItemHudDurabilityNativeWidth(std::size_t slotCount, float scale) {
    if (IsItemHudHorizontalLayout()) {
        return (static_cast<float>(std::max<std::size_t>(1, slotCount) - 1) * 58.0f + kDurabilityTextWidth) * scale;
    }
    return kDurabilityTextWidth * scale;
}

float GetItemHudDurabilityNativeHeight(std::size_t slotCount, float scale) {
    if (IsItemHudHorizontalLayout()) {
        return kItemSize * scale;
    }
    return std::max(
        kItemSize * scale,
        (static_cast<float>(std::max<std::size_t>(1, slotCount) - 1) * kItemSpacing * scale) + kItemSize * scale);
}

Vec2 GetDefaultItemHudPosition(std::size_t slotCount, const Vec2& screenSize) {
    const float scale = GetClampedItemHudScale();
    const float height = GetItemHudItemsNativeHeight(slotCount, scale);
    return Vec2{kHudX, std::max(0.0f, (screenSize.y - height) * 0.5f)};
}

Vec2 ClampItemHudPosition(const Vec2& position, std::size_t slotCount, const Vec2& screenSize) {
    const float scale = GetClampedItemHudScale();
    const float width = GetItemHudItemsNativeWidth(slotCount, scale);
    const float height = GetItemHudItemsNativeHeight(slotCount, scale);
    return Vec2{
        std::clamp(position.x, 0.0f, std::max(0.0f, screenSize.x - width)),
        std::clamp(position.y, 0.0f, std::max(0.0f, screenSize.y - height)),
    };
}

Vec2 GetEffectiveItemHudPosition(std::size_t slotCount, const Vec2& screenSize) {
    EnsureItemHudPositionConfigLoaded();
    if (g_itemHudCustomPosition.load(std::memory_order_relaxed)) {
        return ClampItemHudPosition(
            Vec2{
                g_itemHudPositionX.load(std::memory_order_relaxed),
                g_itemHudPositionY.load(std::memory_order_relaxed),
            },
            slotCount,
            screenSize);
    }

    return GetDefaultItemHudPosition(slotCount, screenSize);
}

Vec2 GetDefaultItemHudDurabilityPosition(std::size_t slotCount, const Vec2& screenSize, const Vec2& itemPosition) {
    (void)slotCount;
    (void)screenSize;
    const float scale = GetClampedItemHudScale();
    if (IsItemHudHorizontalLayout()) {
        return Vec2{itemPosition.x, itemPosition.y + (kItemSize + 7.0f) * scale};
    }

    return Vec2{
        itemPosition.x + kDurabilityTextXOffset * scale,
        itemPosition.y + kDurabilityTextYOffset * scale,
    };
}

Vec2 ClampItemHudDurabilityPosition(const Vec2& position, std::size_t slotCount, const Vec2& screenSize) {
    const float scale = GetClampedItemHudScale();
    const float width = GetItemHudDurabilityNativeWidth(slotCount, scale);
    const float height = GetItemHudDurabilityNativeHeight(slotCount, scale);
    return Vec2{
        std::clamp(position.x, 0.0f, std::max(0.0f, screenSize.x - width)),
        std::clamp(position.y, 0.0f, std::max(0.0f, screenSize.y - height)),
    };
}

Vec2 GetEffectiveItemHudDurabilityPosition(std::size_t slotCount, const Vec2& screenSize, const Vec2& itemPosition) {
    EnsureItemHudPositionConfigLoaded();
    if (g_itemHudDurabilityCustomPosition.load(std::memory_order_relaxed)) {
        return ClampItemHudDurabilityPosition(
            Vec2{
                g_itemHudDurabilityPositionX.load(std::memory_order_relaxed),
                g_itemHudDurabilityPositionY.load(std::memory_order_relaxed),
            },
            slotCount,
            screenSize);
    }

    return ClampItemHudDurabilityPosition(GetDefaultItemHudDurabilityPosition(slotCount, screenSize, itemPosition), slotCount, screenSize);
}

template <typename T>
bool ReadOffsetUnchecked(const void* base, std::size_t offset, T& value) {
    if (base == nullptr) {
        return false;
    }

    __try {
        value = *reinterpret_cast<const T*>(reinterpret_cast<const std::uint8_t*>(base) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
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

void* SafeCallClientVfuncNoArg(void* clientInstance, std::size_t vtableOffset) {
    void* function = GetExecutableVfunc(clientInstance, vtableOffset);
    if (function == nullptr) {
        return nullptr;
    }

    __try {
        return reinterpret_cast<ClientVfuncNoArgFn>(function)(clientInstance);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void* SafeGetArmorItemStack(void* actor, int slot) {
    if (actor == nullptr || slot < 0 || slot >= 4) {
        return nullptr;
    }

    __try {
        if (g_getArmorItemStack != nullptr) {
            return g_getArmorItemStack(actor, slot);
        }

        return nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool SafeIsItemStackEnchanted(void* itemStack) {
    if (g_itemStackIsEnchanted == nullptr || itemStack == nullptr) {
        return false;
    }

    __try {
        return g_itemStackIsEnchanted(itemStack);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

short SafeGetItemStackDamageValue(void* itemStack) {
    if (g_itemStackGetDamageValue == nullptr || itemStack == nullptr) {
        return 0;
    }

    __try {
        return g_itemStackGetDamageValue(itemStack);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool IsPlausibleMaxDamage(int maxDamage) {
    return maxDamage > 1 && maxDamage < 100000;
}

void* GetItemStackItem(void* itemStack) {
    void* itemHolder = nullptr;
    void* item = nullptr;
    if (!ReadOffset(itemStack, kItemPointerOffset, itemHolder) || !ReadValue(itemHolder, item)) {
        return nullptr;
    }

    return item;
}

void* GetCachedDurabilityComponentKey() {
    if (g_getDurabilityComponentKey == nullptr) {
        return nullptr;
    }

    if (g_cachedDurabilityComponentKey != nullptr) {
        return g_cachedDurabilityComponentKey;
    }

    g_cachedDurabilityComponentKey = g_getDurabilityComponentKey();
    return g_cachedDurabilityComponentKey;
}

int GetItemDurabilityComponentMaxDamage(void* item) {
    if (item == nullptr) {
        return 0;
    }

    void* componentKey = GetCachedDurabilityComponentKey();
    void* getComponent = GetExecutableVfunc(item, kItemGetComponentVtableOffset);
    if (componentKey == nullptr || getComponent == nullptr) {
        return 0;
    }

    void* durabilityComponent = reinterpret_cast<ItemGetComponentFn>(getComponent)(item, componentKey);
    int maxDamage = 0;
    if (!ReadOffset(durabilityComponent, kDurabilityComponentMaxDamageOffset, maxDamage)) {
        return 0;
    }

    return IsPlausibleMaxDamage(maxDamage) ? maxDamage : 0;
}

int GetItemLegacyMaxDamage(void* item) {
    if (item == nullptr) {
        return 0;
    }

    void* function = GetExecutableVfunc(item, kItemLegacyMaxDamageVtableOffset);
    if (function == nullptr) {
        return 0;
    }

    __try {
        const int maxDamage = static_cast<int>(reinterpret_cast<ItemGetLegacyMaxDamageFn>(function)(item));
        return IsPlausibleMaxDamage(maxDamage) ? maxDamage : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

int ResolveItemMaxDamage(void* item) {
    if (item == nullptr) {
        return 0;
    }

    const auto key = reinterpret_cast<std::uintptr_t>(item);
    if (const auto it = g_itemMaxDamageCache.find(key); it != g_itemMaxDamageCache.end()) {
        return it->second;
    }

    int maxDamage = GetItemDurabilityComponentMaxDamage(item);
    if (maxDamage <= 0) {
        maxDamage = GetItemLegacyMaxDamage(item);
    }

    g_itemMaxDamageCache.emplace(key, maxDamage);
    return maxDamage;
}

int SafeGetItemStackMaxDamage(void* itemStack) {
    if (itemStack == nullptr) {
        return 0;
    }

    __try {
        if (g_itemStackGetDurabilityItem != nullptr) {
            if (const int maxDamage = ResolveItemMaxDamage(g_itemStackGetDurabilityItem(itemStack)); maxDamage > 0) {
                return maxDamage;
            }
        }

        void* item = GetItemStackItem(itemStack);
        if (const int maxDamage = ResolveItemMaxDamage(item); maxDamage > 0) {
            return maxDamage;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    return 0;
}

bool IsRenderableItemStack(void* itemStack);

void* GetMinecraftGameFromClientInstance(void* clientInstance) {
    if (clientInstance == nullptr) {
        g_cachedMinecraftGameClientInstance = nullptr;
        g_cachedMinecraftGame = nullptr;
        return nullptr;
    }

    if (g_cachedMinecraftGameClientInstance == clientInstance && g_cachedMinecraftGame != nullptr) {
        return g_cachedMinecraftGame;
    }

    if (void* minecraftGame = SafeCallClientVfuncNoArg(clientInstance, kClientInstanceMinecraftGameVtableOffset);
        IsReadableAddress(minecraftGame, sizeof(void*))) {
        g_cachedMinecraftGameClientInstance = clientInstance;
        g_cachedMinecraftGame = minecraftGame;
        return minecraftGame;
    }

    void* minecraftGame = nullptr;
    if (ReadOffset(clientInstance, kClientInstanceMinecraftGameOffset, minecraftGame) &&
        IsReadableAddress(minecraftGame, sizeof(void*))) {
        g_cachedMinecraftGameClientInstance = clientInstance;
        g_cachedMinecraftGame = minecraftGame;
        return minecraftGame;
    }

    g_cachedMinecraftGameClientInstance = nullptr;
    g_cachedMinecraftGame = nullptr;
    return nullptr;
}

void* GetBaseActorRenderContextItemRenderer(void* context, const char*& source) {
    source = "none";
    if (context == nullptr) {
        return nullptr;
    }

    if (g_baseActorRenderContextGetItemRenderer != nullptr) {
        __try {
            void* itemRenderer = g_baseActorRenderContextGetItemRenderer(context);
            if (itemRenderer != nullptr) {
                source = "getter";
                return itemRenderer;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void* itemRenderer = nullptr;
    if (ReadOffset(context, kBaseActorRenderContextItemRendererOffset, itemRenderer) && itemRenderer != nullptr) {
        source = "offset";
        return itemRenderer;
    }

    return nullptr;
}

bool RenderGuiItemWithDirectContext(
    void* baseActorRenderContext,
    void* itemRenderer,
    void* itemStack,
    float x,
    float y,
    float opacity,
    float scale,
    int renderVariant) {
    if (baseActorRenderContext == nullptr || itemRenderer == nullptr || g_itemRendererRenderGuiItemNew == nullptr) {
        return false;
    }

    constexpr int guiItemMode = 0;
    constexpr float kLatiteGuiItemA9 = 1.0f;
    g_itemRendererRenderGuiItemNew(
        itemRenderer,
        baseActorRenderContext,
        itemStack,
        guiItemMode,
        x,
        y,
        false,
        opacity,
        kLatiteGuiItemA9,
        scale,
        renderVariant);

    std::uint8_t glint = 0;
    if (ReadOffset(itemStack, kItemRenderEntryGlintFlagOffset, glint) && glint != 0) {
        g_itemRendererRenderGuiItemNew(
            itemRenderer,
            baseActorRenderContext,
            itemStack,
            guiItemMode,
            x,
            y,
            true,
            opacity,
            kLatiteGuiItemA9,
            scale,
            renderVariant);
    }

    return true;
}

void EndDirectGuiItemFrame() {
    if (!g_directGuiItemFrameContext.active) {
        return;
    }

    void* context = g_directGuiItemFrameContext.baseActorRenderContext;
    g_directGuiItemFrameContext.active = false;
    g_directGuiItemFrameContext.baseActorRenderContext = nullptr;
    g_directGuiItemFrameContext.itemRenderer = nullptr;
    g_directGuiItemFrameContext.guiItemContext = nullptr;
    g_directGuiItemFrameContext.minecraftUiRenderContext = nullptr;
    g_directGuiItemFrameContext.clientInstance = nullptr;
    if (context != nullptr && g_baseActorRenderContextDtor != nullptr) {
        __try {
            g_baseActorRenderContextDtor(context);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void DestroyBaseActorRenderContext(void* context) {
    if (context == nullptr) {
        return;
    }

    if (g_baseActorRenderContextDtor != nullptr) {
        g_baseActorRenderContextDtor(context);
    }
}

bool BeginDirectGuiItemFrame(void* minecraftUiRenderContext, void* clientInstance) {
    EndDirectGuiItemFrame();

    if (g_baseActorRenderContextCtor == nullptr ||
        g_baseActorRenderContextDtor == nullptr ||
        g_itemRendererRenderGuiItemNew == nullptr ||
        minecraftUiRenderContext == nullptr ||
        clientInstance == nullptr) {
        return false;
    }

    void* screenContext = nullptr;
    if (!ReadOffset(minecraftUiRenderContext, kMinecraftUIRenderContextScreenContextOffset, screenContext) ||
        screenContext == nullptr) {
        return false;
    }

    void* minecraftGame = GetMinecraftGameFromClientInstance(clientInstance);
    if (minecraftGame == nullptr) {
        return false;
    }

    __try {
        void* context = g_baseActorRenderContextCtor(g_directGuiItemFrameContext.storage.data(), screenContext, clientInstance, minecraftGame);
        if (context == nullptr) {
            context = g_directGuiItemFrameContext.storage.data();
        }

        const char* itemRendererSource = "none";
        void* itemRenderer = GetBaseActorRenderContextItemRenderer(context, itemRendererSource);
        if (itemRenderer == nullptr) {
            DestroyBaseActorRenderContext(context);
            return false;
        }

        g_directGuiItemFrameContext.frameId = g_uiFrameId.load(std::memory_order_relaxed);
        g_directGuiItemFrameContext.minecraftUiRenderContext = minecraftUiRenderContext;
        g_directGuiItemFrameContext.clientInstance = clientInstance;
        g_directGuiItemFrameContext.baseActorRenderContext = context;
        g_directGuiItemFrameContext.itemRenderer = itemRenderer;
        g_directGuiItemFrameContext.guiItemContext = nullptr;
        g_directGuiItemFrameContext.active = true;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TryRenderGuiItemNew(
    void* minecraftUiRenderContext,
    void* clientInstance,
    void* itemStack,
    float x,
    float y,
    float opacity,
    float scale,
    int renderVariant) {
    if (g_baseActorRenderContextCtor == nullptr ||
        g_baseActorRenderContextDtor == nullptr ||
        g_itemRendererRenderGuiItemNew == nullptr) {
        return false;
    }

    if (minecraftUiRenderContext == nullptr || clientInstance == nullptr) {
        return false;
    }

    if (!IsRenderableItemStack(itemStack)) {
        return false;
    }

    if (g_directGuiItemFrameContext.active &&
        g_directGuiItemFrameContext.frameId == g_uiFrameId.load(std::memory_order_relaxed) &&
        g_directGuiItemFrameContext.minecraftUiRenderContext == minecraftUiRenderContext &&
        g_directGuiItemFrameContext.clientInstance == clientInstance) {
        __try {
            const bool rendered = RenderGuiItemWithDirectContext(
                g_directGuiItemFrameContext.baseActorRenderContext,
                g_directGuiItemFrameContext.itemRenderer,
                itemStack,
                x,
                y,
                opacity,
                scale,
                renderVariant);
            if (rendered) {
                return true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void* screenContext = nullptr;
    if (!ReadOffset(minecraftUiRenderContext, kMinecraftUIRenderContextScreenContextOffset, screenContext) ||
        screenContext == nullptr) {
        return false;
    }

    void* minecraftGame = GetMinecraftGameFromClientInstance(clientInstance);
    if (minecraftGame == nullptr) {
        return false;
    }

    alignas(16) std::array<std::uint8_t, 0x500> baseActorRenderContext{};
    bool rendered = false;
    __try {
        void* context = g_baseActorRenderContextCtor(baseActorRenderContext.data(), screenContext, clientInstance, minecraftGame);
        if (context == nullptr) {
            context = baseActorRenderContext.data();
        }

        const char* itemRendererSource = "none";
        void* itemRenderer = GetBaseActorRenderContextItemRenderer(context, itemRendererSource);
        if (itemRenderer == nullptr) {
            DestroyBaseActorRenderContext(context);
            return false;
        }

        rendered = RenderGuiItemWithDirectContext(context, itemRenderer, itemStack, x, y, opacity, scale, renderVariant);

        DestroyBaseActorRenderContext(context);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return rendered;
}

void SafeRenderDecoratedGuiItem(
    void* minecraftUiRenderContext,
    void* clientInstance,
    void* itemStack,
    float x,
    float y,
    float opacity,
    float scale,
    int renderVariant) {
    if (TryRenderGuiItemNew(minecraftUiRenderContext, clientInstance, itemStack, x, y, opacity, scale, renderVariant)) {
        return;
    }

    if (g_renderDecoratedGuiItem == nullptr) {
        return;
    }

    __try {
        g_renderDecoratedGuiItem(
            nullptr,
            minecraftUiRenderContext,
            clientInstance,
            itemStack,
            x,
            y,
            opacity,
            scale,
            renderVariant);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
