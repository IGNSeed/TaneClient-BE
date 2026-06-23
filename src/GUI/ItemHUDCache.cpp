bool ReadItemStackRenderState(void* itemStack, ItemStackRenderState& state) {
    state = ItemStackRenderState{};
    return ReadOffset(itemStack, kItemValidOffset, state.valid) &&
        ReadOffset(itemStack, kItemCountOffset, state.count) &&
        ReadOffset(itemStack, kItemAuxValueOffset, state.aux) &&
        ReadOffset(itemStack, kItemPointerOffset, state.itemHolder);
}

bool IsRenderableItemStackState(const ItemStackRenderState& state) {
    void* item = nullptr;
    return state.valid != 0 &&
        state.count > 0 &&
        state.itemHolder != nullptr &&
        ReadValue(state.itemHolder, item) &&
        item != nullptr;
}

bool IsRenderableItemStack(void* itemStack) {
    ItemStackRenderState state{};
    return ReadItemStackRenderState(itemStack, state) && IsRenderableItemStackState(state);
}

int GetItemStackCount(void* itemStack) {
    std::uint8_t count = 0;
    return ReadOffset(itemStack, kItemCountOffset, count) ? static_cast<int>(count) : 0;
}

bool GetItemStackDurability(void* itemStack, int& damage, int& maxDamage) {
    maxDamage = static_cast<int>(SafeGetItemStackMaxDamage(itemStack));
    damage = static_cast<int>(SafeGetItemStackDamageValue(itemStack));

    if (damage < 0) {
        damage = 0;
    } else if (maxDamage > 0 && damage > maxDamage) {
        damage = maxDamage;
    }

    return maxDamage > 0;
}

void AssignItemStackCountText(HudRenderSlot& slot, int count) {
    if (count <= 1) {
        slot.countText.clear();
        return;
    }

    slot.countText = std::to_string(count);
}

void UpdateItemStackCountText(HudRenderSlot& slot, int count, bool& changed) {
    if (count <= 1) {
        if (!slot.countText.empty()) {
            slot.countText.clear();
            changed = true;
        }
        return;
    }

    char buffer[4]{};
    const int written = std::snprintf(buffer, sizeof(buffer), "%d", count);
    if (written <= 0) {
        return;
    }

    if (slot.countText != buffer) {
        slot.countText.assign(buffer);
        changed = true;
    }
}

float LerpFloat(float from, float to, float progress) {
    return from + (to - from) * std::clamp(progress, 0.0f, 1.0f);
}

MceColor LerpColor(const MceColor& from, const MceColor& to, float progress) {
    return MceColor{
        LerpFloat(from.r, to.r, progress),
        LerpFloat(from.g, to.g, progress),
        LerpFloat(from.b, to.b, progress),
        LerpFloat(from.a, to.a, progress),
    };
}

MceColor GetDurabilityTextColor(int currentDurability, int maxDamage) {
    constexpr MceColor low{1.0f, 0.22f, 0.22f, 1.0f};
    constexpr MceColor middle{1.0f, 0.92f, 0.24f, 1.0f};
    constexpr MceColor high{0.35f, 1.0f, 0.35f, 1.0f};

    if (maxDamage <= 0) {
        return high;
    }

    const float ratio = std::clamp(static_cast<float>(currentDurability) / static_cast<float>(maxDamage), 0.0f, 1.0f);
    if (ratio < 0.5f) {
        return LerpColor(low, middle, ratio * 2.0f);
    }

    return LerpColor(middle, high, (ratio - 0.5f) * 2.0f);
}

void HsvToRgb(float hue, float saturation, float value, float& red, float& green, float& blue) {
    hue = hue - std::floor(hue);
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    value = std::clamp(value, 0.0f, 1.0f);

    const float scaledHue = hue * 6.0f;
    const int sector = static_cast<int>(std::floor(scaledHue));
    const float fraction = scaledHue - static_cast<float>(sector);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - fraction * saturation);
    const float t = value * (1.0f - (1.0f - fraction) * saturation);

    switch (sector % 6) {
    case 0:
        red = value;
        green = t;
        blue = p;
        break;
    case 1:
        red = q;
        green = value;
        blue = p;
        break;
    case 2:
        red = p;
        green = value;
        blue = t;
        break;
    case 3:
        red = p;
        green = q;
        blue = value;
        break;
    case 4:
        red = t;
        green = p;
        blue = value;
        break;
    default:
        red = value;
        green = p;
        blue = q;
        break;
    }
}

std::string GetItemStackDurabilityText(void* itemStack, MceColor& color) {
    int damage = 0;
    int maxDamage = 0;
    if (!GetItemStackDurability(itemStack, damage, maxDamage)) {
        return {};
    }

    const int currentDurability = std::clamp(maxDamage - damage, 0, maxDamage);
    color = GetDurabilityTextColor(currentDurability, maxDamage);
    return std::to_string(currentDurability) + "/" + std::to_string(maxDamage);
}

void PopulateHudSlotText(HudRenderSlot& slot, void* itemStack) {
    AssignItemStackCountText(slot, slot.liveStateValid ? static_cast<int>(slot.liveState.count) : GetItemStackCount(itemStack));
    slot.durabilityTextColor = MceColor{1.0f, 1.0f, 1.0f, 1.0f};
    slot.currentDurability = 0;
    slot.maxDurability = 0;
    slot.durabilityStateDirty = true;

    int damage = 0;
    int maxDamage = 0;
    if (!GetItemStackDurability(itemStack, damage, maxDamage)) {
        slot.durabilityText.clear();
        slot.durabilityStateDirty = false;
        return;
    }

    slot.maxDurability = maxDamage;
    slot.currentDurability = std::clamp(maxDamage - damage, 0, maxDamage);
    slot.durabilityTextColor = GetDurabilityTextColor(slot.currentDurability, maxDamage);
    slot.durabilityText = std::to_string(slot.currentDurability) + "/" + std::to_string(maxDamage);
    slot.durabilityStateDirty = false;
}

bool RefreshCachedHudSlotLiveState(bool updateDurability, bool& changed) {
    changed = false;
    for (std::size_t i = 0; i < g_cachedHudSlotCount; ++i) {
        HudRenderSlot& slot = g_cachedHudSlots[i];
        if (!slot.hasItem) {
            continue;
        }

        void* itemStack = slot.sourceItemStack;
        ItemStackRenderState liveState{};
        if (!ReadItemStackRenderState(itemStack, liveState) || !IsRenderableItemStackState(liveState)) {
            return false;
        }

        const ItemStackRenderState previousState = slot.liveState;
        const bool hadLiveState = slot.liveStateValid;
        const bool itemChanged = !hadLiveState || previousState.itemHolder != liveState.itemHolder;
        const bool renderStateChanged =
            !hadLiveState ||
            previousState.itemHolder != liveState.itemHolder ||
            previousState.aux != liveState.aux ||
            previousState.count != liveState.count ||
            previousState.valid != liveState.valid;
        bool renderEntryUpdated = false;
        if (renderStateChanged) {
            if (itemChanged) {
                if (!BuildRenderEntry(itemStack, slot.renderEntry)) {
                    return false;
                }
                slot.durabilityStateDirty = true;
            } else {
                const std::uint8_t previousGlint = slot.renderEntry[kItemRenderEntryGlintFlagOffset];
                if (!CopyLiveItemStackToRenderEntry(itemStack, slot.renderEntry, previousGlint)) {
                    return false;
                }
            }
            renderEntryUpdated = true;
        }

        if (!hadLiveState || previousState.count != liveState.count) {
            UpdateItemStackCountText(slot, static_cast<int>(liveState.count), changed);
        }
        slot.liveState = liveState;
        slot.liveStateValid = true;

        if (itemChanged) {
            changed = true;
        }

        if (!updateDurability) {
            continue;
        }

        int damage = 0;
        int maxDamage = 0;
        bool hasDurability = false;
        if (!slot.durabilityStateDirty && slot.maxDurability > 0) {
            maxDamage = slot.maxDurability;
            damage = static_cast<int>(SafeGetItemStackDamageValue(itemStack));
            if (damage < 0) {
                damage = 0;
            } else if (damage > maxDamage) {
                damage = maxDamage;
            }
            hasDurability = true;
        } else if (slot.durabilityStateDirty || slot.maxDurability > 0) {
            hasDurability = GetItemStackDurability(itemStack, damage, maxDamage);
        }

        if (!hasDurability) {
            if (!slot.durabilityText.empty() || slot.currentDurability != 0 || slot.maxDurability != 0) {
                slot.durabilityText.clear();
                slot.currentDurability = 0;
                slot.maxDurability = 0;
                slot.durabilityTextColor = MceColor{1.0f, 1.0f, 1.0f, 1.0f};
                changed = true;
            }
            slot.durabilityStateDirty = false;
            continue;
        }

        const int currentDurability = std::clamp(maxDamage - damage, 0, maxDamage);
        if (slot.currentDurability != currentDurability || slot.maxDurability != maxDamage) {
            if (!renderEntryUpdated) {
                const std::uint8_t previousGlint = slot.renderEntry[kItemRenderEntryGlintFlagOffset];
                if (!CopyLiveItemStackToRenderEntry(itemStack, slot.renderEntry, previousGlint)) {
                    return false;
                }
            }
            slot.currentDurability = currentDurability;
            slot.maxDurability = maxDamage;
            slot.durabilityTextColor = GetDurabilityTextColor(currentDurability, maxDamage);
            slot.durabilityText = std::to_string(currentDurability) + "/" + std::to_string(maxDamage);
            changed = true;
        }
        slot.durabilityStateDirty = false;
    }

    return true;
}

bool LooksLikeActor(void* actor) {
    if (!IsReadableAddress(actor, sizeof(void*))) {
        return false;
    }

    void** vtable = nullptr;
    if (!ReadValue(actor, vtable) || !IsReadableAddress(vtable, sizeof(void*) * 0x100)) {
        return false;
    }

    for (std::size_t offset : {kGetCarriedItemVtableOffset, kGetCarriedItemVtableOffset + sizeof(void*)}) {
        if (IsExecutableAddress(GetExecutableVfunc(actor, offset))) {
            return true;
        }
    }

    return false;
}

void* CallActorVfunc(void* actor, std::size_t vtableOffset) {
    void* function = GetExecutableVfunc(actor, vtableOffset);
    if (function == nullptr) {
        return nullptr;
    }

    __try {
        return reinterpret_cast<GetCarriedItemFn>(function)(actor);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

InventoryGetItemFn ResolveInventoryGetItem(void* inventory) {
    void** vtable = nullptr;
    if (!ReadValue(inventory, vtable) || vtable == nullptr) {
        g_cachedInventoryVtable = nullptr;
        g_cachedInventoryGetItem = nullptr;
        return nullptr;
    }

    if (vtable == g_cachedInventoryVtable && g_cachedInventoryGetItem != nullptr) {
        return g_cachedInventoryGetItem;
    }

    void* function = nullptr;
    if (!ReadOffset(vtable, kInventoryGetItemVtableOffset, function) || !IsExecutableAddress(function)) {
        g_cachedInventoryVtable = nullptr;
        g_cachedInventoryGetItem = nullptr;
        return nullptr;
    }

    g_cachedInventoryVtable = vtable;
    g_cachedInventoryGetItem = reinterpret_cast<InventoryGetItemFn>(function);
    return g_cachedInventoryGetItem;
}

bool IsUsableInventory(void* inventory) {
    return ResolveInventoryGetItem(inventory) != nullptr;
}

void* GetInventoryItem(void* inventory, int slot) {
    if (slot < 0 || slot > kPlayerInventoryMainSlotCount) {
        return nullptr;
    }

    InventoryGetItemFn getItem = ResolveInventoryGetItem(inventory);
    if (getItem == nullptr) {
        return nullptr;
    }

    __try {
        return getItem(inventory, slot);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool TryGetPlayerMainInventory(void* localPlayer, void*& inventory) {
    inventory = nullptr;
    if (localPlayer == nullptr) {
        return false;
    }

    auto tryAtOffset = [&](std::size_t playerInventoryOffset) -> bool {
        void* supplies = nullptr;
        void* candidateInventory = nullptr;
        if (!ReadOffset(localPlayer, playerInventoryOffset, supplies) ||
            !ReadOffset(supplies, kPlayerInventoryInventoryOffset, candidateInventory) ||
            !IsUsableInventory(candidateInventory)) {
            return false;
        }

        g_cachedPlayerInventoryOffset = playerInventoryOffset;
        inventory = candidateInventory;
        return true;
    };

    if (g_cachedPlayerInventoryOffset != 0 && tryAtOffset(g_cachedPlayerInventoryOffset)) {
        return true;
    }

    for (std::size_t playerInventoryOffset : kPlayerInventoryCandidates) {
        if (tryAtOffset(playerInventoryOffset)) {
            return true;
        }
    }

    return false;
}

bool TryGetSelectedSlot(void* localPlayer, int& selectedSlot);

bool ReadInventoryCounterStackState(void* itemStack, InventoryCounterTrackedStack& stack) {
    stack = InventoryCounterTrackedStack{};
    if (itemStack == nullptr) {
        return false;
    }

    std::uint8_t valid = 0;
    std::uint8_t count = 0;
    void* itemHolder = nullptr;
    if (!ReadOffset(itemStack, kItemValidOffset, valid) ||
        !ReadOffset(itemStack, kItemCountOffset, count) ||
        !ReadOffset(itemStack, kItemPointerOffset, itemHolder) ||
        valid == 0 ||
        count == 0 ||
        itemHolder == nullptr) {
        return false;
    }

    stack.itemStack = itemStack;
    stack.itemHolder = itemHolder;
    stack.count = count;
    stack.valid = true;
    ReadOffset(itemStack, kItemAuxValueOffset, stack.aux);
    return true;
}

InventoryCounterKind ClassifyInventoryCounterStack(const InventoryCounterTrackedStack& stack) {
    if (!stack.valid) {
        return InventoryCounterKind::None;
    }

    const std::string itemNameStorage = GetItemStackItemName(stack.itemStack);
    const std::string_view itemName = StripMinecraftNamespace(itemNameStorage);
    if (itemName == "arrow") {
        return InventoryCounterKind::Arrow;
    }

    if (itemName != "splash_potion") {
        return InventoryCounterKind::None;
    }

    return stack.aux == kHealingSplashPotionAux ? InventoryCounterKind::HealingSplash : InventoryCounterKind::None;
}

void AddInventoryCounterStackCount(
    const InventoryCounterTrackedStack& stack,
    int& arrowCount,
    int& healingSplashCount) {
    if (!stack.valid || stack.kind == InventoryCounterKind::None) {
        return;
    }

    const int stackCount = static_cast<int>(stack.count);
    if (stack.kind == InventoryCounterKind::Arrow) {
        arrowCount += stackCount;
    } else if (stack.kind == InventoryCounterKind::HealingSplash) {
        healingSplashCount += stackCount;
    }
}

bool RefreshInventoryCounterTrackedCounts(int& arrowCount, int& healingSplashCount, bool& needsFullScan) {
    arrowCount = 0;
    healingSplashCount = 0;
    needsFullScan = false;

    bool touchedTrackedStack = false;
    for (InventoryCounterTrackedStack& tracked : g_inventoryCounterTrackedStacks) {
        if (!tracked.valid || tracked.itemStack == nullptr || tracked.kind == InventoryCounterKind::None) {
            continue;
        }

        InventoryCounterTrackedStack current{};
        if (!ReadInventoryCounterStackState(tracked.itemStack, current)) {
            tracked = {};
            needsFullScan = true;
            continue;
        }

        if (current.itemHolder != tracked.itemHolder || current.aux != tracked.aux) {
            needsFullScan = true;
            return false;
        }

        current.kind = tracked.kind;
        tracked.count = current.count;
        AddInventoryCounterStackCount(tracked, arrowCount, healingSplashCount);
        touchedTrackedStack = true;
    }

    g_inventoryCounterFrameCache.hasTrackedStacks = touchedTrackedStack;
    return true;
}

bool RefreshInventoryCounterCountsFullScan(
    void* clientInstance,
    void* localPlayer,
    void* inventory,
    std::uint64_t frameId,
    int selectedSlot,
    int& arrowCount,
    int& healingSplashCount) {
    arrowCount = 0;
    healingSplashCount = 0;
    g_inventoryCounterTrackedStacks = {};

    bool hasTrackedStacks = false;
    std::size_t trackedIndex = 0;
    for (int slot = 0; slot < kPlayerInventoryMainSlotCount; ++slot) {
        InventoryCounterTrackedStack stack{};
        if (!ReadInventoryCounterStackState(GetInventoryItem(inventory, slot), stack)) {
            continue;
        }

        stack.kind = ClassifyInventoryCounterStack(stack);
        if (stack.kind == InventoryCounterKind::None) {
            continue;
        }

        AddInventoryCounterStackCount(stack, arrowCount, healingSplashCount);
        if (trackedIndex < g_inventoryCounterTrackedStacks.size()) {
            g_inventoryCounterTrackedStacks[trackedIndex++] = stack;
            hasTrackedStacks = true;
        }
    }

    arrowCount = std::clamp(arrowCount, 0, 9999);
    healingSplashCount = std::clamp(healingSplashCount, 0, 9999);
    g_inventoryCounterFrameCache.frameId = frameId;
    g_inventoryCounterFrameCache.clientInstance = clientInstance;
    g_inventoryCounterFrameCache.localPlayer = localPlayer;
    g_inventoryCounterFrameCache.inventory = inventory;
    g_inventoryCounterFrameCache.lastFullScanMs = GetTickCount64();
    g_inventoryCounterFrameCache.selectedSlot = selectedSlot;
    g_inventoryCounterFrameCache.valid = true;
    g_inventoryCounterFrameCache.hasTrackedStacks = hasTrackedStacks;
    g_inventoryCounterFrameCache.arrowCount = arrowCount;
    g_inventoryCounterFrameCache.healingSplashCount = healingSplashCount;
    return true;
}

bool QueryInventoryCounterCountsForFrame(void* clientInstance, int& arrowCount, int& healingSplashCount) {
    arrowCount = 0;
    healingSplashCount = 0;
    const std::uint64_t frameId = g_uiFrameId.load(std::memory_order_relaxed);
    if (g_inventoryCounterFrameCache.valid &&
        g_inventoryCounterFrameCache.frameId == frameId &&
        g_inventoryCounterFrameCache.clientInstance == clientInstance) {
        arrowCount = g_inventoryCounterFrameCache.arrowCount;
        healingSplashCount = g_inventoryCounterFrameCache.healingSplashCount;
        return true;
    }

    if (clientInstance == nullptr) {
        g_inventoryCounterFrameCache.valid = false;
        return false;
    }

    void* localPlayer = CallClientInstanceGetLocalPlayer(clientInstance);
    void* inventory = nullptr;
    if (localPlayer == nullptr || !TryGetPlayerMainInventory(localPlayer, inventory)) {
        g_inventoryCounterFrameCache.valid = false;
        return false;
    }

    int selectedSlot = -1;
    TryGetSelectedSlot(localPlayer, selectedSlot);
    const DWORD64 now = GetTickCount64();
    const bool needsFullScan =
        !g_inventoryCounterFrameCache.valid ||
        g_inventoryCounterFrameCache.clientInstance != clientInstance ||
        g_inventoryCounterFrameCache.localPlayer != localPlayer ||
        g_inventoryCounterFrameCache.inventory != inventory ||
        g_inventoryCounterFrameCache.selectedSlot != selectedSlot ||
        now - g_inventoryCounterFrameCache.lastFullScanMs >= kInventoryCounterFullScanRefreshMs;
    if (needsFullScan) {
        return RefreshInventoryCounterCountsFullScan(
            clientInstance,
            localPlayer,
            inventory,
            frameId,
            selectedSlot,
            arrowCount,
            healingSplashCount);
    }

    bool trackedNeedsFullScan = false;
    if (!RefreshInventoryCounterTrackedCounts(arrowCount, healingSplashCount, trackedNeedsFullScan) || trackedNeedsFullScan) {
        return RefreshInventoryCounterCountsFullScan(
            clientInstance,
            localPlayer,
            inventory,
            frameId,
            selectedSlot,
            arrowCount,
            healingSplashCount);
    }

    arrowCount = std::clamp(arrowCount, 0, 9999);
    healingSplashCount = std::clamp(healingSplashCount, 0, 9999);
    g_inventoryCounterFrameCache.frameId = frameId;
    g_inventoryCounterFrameCache.clientInstance = clientInstance;
    g_inventoryCounterFrameCache.localPlayer = localPlayer;
    g_inventoryCounterFrameCache.inventory = inventory;
    g_inventoryCounterFrameCache.valid = true;
    g_inventoryCounterFrameCache.arrowCount = arrowCount;
    g_inventoryCounterFrameCache.healingSplashCount = healingSplashCount;
    return true;
}

bool QueryInventoryCounterCountsAndRenderEntriesForFrame(
    void* clientInstance,
    int& arrowCount,
    int& healingSplashCount,
    void* arrowRenderEntry,
    void* healingSplashRenderEntry,
    std::size_t renderEntrySize) {
    arrowCount = 0;
    healingSplashCount = 0;
    if (clientInstance == nullptr || renderEntrySize < kItemRenderEntrySize) {
        g_inventoryCounterFrameCache.valid = false;
        return false;
    }

    void* localPlayer = CallClientInstanceGetLocalPlayer(clientInstance);
    void* inventory = nullptr;
    if (localPlayer == nullptr || !TryGetPlayerMainInventory(localPlayer, inventory)) {
        g_inventoryCounterFrameCache.valid = false;
        return false;
    }

    bool hasArrowRenderEntry = false;
    bool hasHealingSplashRenderEntry = false;
    std::array<std::uint8_t, kItemRenderEntrySize> entry{};
    for (int slot = 0; slot < kPlayerInventoryMainSlotCount; ++slot) {
        void* itemStack = GetInventoryItem(inventory, slot);
        if (itemStack == nullptr) {
            continue;
        }

        InventoryCounterTrackedStack stack{};
        if (!ReadInventoryCounterStackState(itemStack, stack)) {
            continue;
        }

        stack.kind = ClassifyInventoryCounterStack(stack);
        if (stack.kind == InventoryCounterKind::Arrow) {
            arrowCount += static_cast<int>(stack.count);
            if (!hasArrowRenderEntry && arrowRenderEntry != nullptr && BuildRenderEntry(itemStack, entry)) {
                entry[kItemCountOffset] = 1;
                std::memcpy(arrowRenderEntry, entry.data(), entry.size());
                hasArrowRenderEntry = true;
            }
            continue;
        }

        if (stack.kind != InventoryCounterKind::HealingSplash) {
            continue;
        }

        healingSplashCount += static_cast<int>(stack.count);
        if (!hasHealingSplashRenderEntry && healingSplashRenderEntry != nullptr && BuildRenderEntry(itemStack, entry)) {
            entry[kItemCountOffset] = 1;
            std::memcpy(healingSplashRenderEntry, entry.data(), entry.size());
            hasHealingSplashRenderEntry = true;
        }
    }

    arrowCount = std::clamp(arrowCount, 0, 9999);
    healingSplashCount = std::clamp(healingSplashCount, 0, 9999);
    const std::uint64_t frameId = g_uiFrameId.load(std::memory_order_relaxed);
    g_inventoryCounterFrameCache.frameId = frameId;
    g_inventoryCounterFrameCache.clientInstance = clientInstance;
    g_inventoryCounterFrameCache.localPlayer = localPlayer;
    g_inventoryCounterFrameCache.inventory = inventory;
    g_inventoryCounterFrameCache.valid = true;
    g_inventoryCounterFrameCache.arrowCount = arrowCount;
    g_inventoryCounterFrameCache.healingSplashCount = healingSplashCount;
    return true;
}

bool ReadSelectedSlotAtInventoryOffset(void* localPlayer, std::size_t playerInventoryOffset, int& selectedSlot) {
    void* supplies = nullptr;
    void* inventory = nullptr;
    if (!ReadOffset(localPlayer, playerInventoryOffset, supplies) ||
        !ReadOffset(supplies, kPlayerInventorySelectedSlotOffset, selectedSlot) ||
        !ReadOffset(supplies, kPlayerInventoryInventoryOffset, inventory)) {
        return false;
    }

    return selectedSlot >= 0 && selectedSlot <= 36 && IsUsableInventory(inventory);
}

bool TryGetSelectedSlot(void* localPlayer, int& selectedSlot) {
    if (localPlayer == nullptr) {
        return false;
    }

    if (g_cachedPlayerInventoryOffset != 0 &&
        ReadSelectedSlotAtInventoryOffset(localPlayer, g_cachedPlayerInventoryOffset, selectedSlot)) {
        return true;
    }

    for (std::size_t playerInventoryOffset : kPlayerInventoryCandidates) {
        if (ReadSelectedSlotAtInventoryOffset(localPlayer, playerInventoryOffset, selectedSlot)) {
            g_cachedPlayerInventoryOffset = playerInventoryOffset;
            return true;
        }
    }

    return false;
}

bool DidSelectedSlotChange(void* localPlayer) {
    int selectedSlot = -1;
    return TryGetSelectedSlot(localPlayer, selectedSlot) && selectedSlot != g_cachedSelectedSlot;
}

void* GetMainHandInventoryItem(void* localPlayer) {
    if (g_cachedPlayerInventoryOffset != 0) {
        void* supplies = nullptr;
        void* inventory = nullptr;
        int selectedSlot = -1;
        if (ReadOffset(localPlayer, g_cachedPlayerInventoryOffset, supplies) &&
            ReadOffset(supplies, kPlayerInventorySelectedSlotOffset, selectedSlot) &&
            ReadOffset(supplies, kPlayerInventoryInventoryOffset, inventory)) {
            g_cachedSelectedSlot = selectedSlot;
            if (void* itemStack = GetInventoryItem(inventory, selectedSlot); IsRenderableItemStack(itemStack)) {
                return itemStack;
            }
        }
    }

    for (std::size_t playerInventoryOffset : kPlayerInventoryCandidates) {
        void* supplies = nullptr;
        void* inventory = nullptr;
        int selectedSlot = -1;
        if (!ReadOffset(localPlayer, playerInventoryOffset, supplies) ||
            !ReadOffset(supplies, kPlayerInventorySelectedSlotOffset, selectedSlot) ||
            !ReadOffset(supplies, kPlayerInventoryInventoryOffset, inventory)) {
            continue;
        }

        g_cachedPlayerInventoryOffset = playerInventoryOffset;
        g_cachedSelectedSlot = selectedSlot;
        if (void* itemStack = GetInventoryItem(inventory, selectedSlot); IsRenderableItemStack(itemStack)) {
            return itemStack;
        }
    }

    return nullptr;
}

void* GetInventoryItemStackTemplate(void* localPlayer) {
    if (localPlayer == nullptr) {
        return nullptr;
    }

    auto findTemplateAtOffset = [](void* player, std::size_t playerInventoryOffset) -> void* {
        void* supplies = nullptr;
        void* inventory = nullptr;
        int selectedSlot = -1;
        if (!ReadOffset(player, playerInventoryOffset, supplies) ||
            !ReadOffset(supplies, kPlayerInventorySelectedSlotOffset, selectedSlot) ||
            !ReadOffset(supplies, kPlayerInventoryInventoryOffset, inventory)) {
            return nullptr;
        }

        const int firstSlot = std::clamp(selectedSlot, 0, 36);
        if (void* itemStack = GetInventoryItem(inventory, firstSlot);
            IsLikelyItemStack(itemStack) && IsReadableAddress(itemStack, kItemRenderEntrySize)) {
            return itemStack;
        }

        for (int slot = 0; slot <= 36; ++slot) {
            if (slot == firstSlot) {
                continue;
            }
            if (void* itemStack = GetInventoryItem(inventory, slot);
                IsLikelyItemStack(itemStack) && IsReadableAddress(itemStack, kItemRenderEntrySize)) {
                return itemStack;
            }
        }

        return nullptr;
    };

    if (g_cachedPlayerInventoryOffset != 0) {
        if (void* itemStack = findTemplateAtOffset(localPlayer, g_cachedPlayerInventoryOffset)) {
            return itemStack;
        }
    }

    for (std::size_t playerInventoryOffset : kPlayerInventoryCandidates) {
        if (void* itemStack = findTemplateAtOffset(localPlayer, playerInventoryOffset)) {
            g_cachedPlayerInventoryOffset = playerInventoryOffset;
            return itemStack;
        }
    }

    for (int slot = 0; slot < 4; ++slot) {
        if (void* itemStack = SafeGetArmorItemStack(localPlayer, slot);
            IsLikelyItemStack(itemStack) && IsReadableAddress(itemStack, kItemRenderEntrySize)) {
            return itemStack;
        }
    }

    if (void* carried = CallActorVfunc(localPlayer, kGetCarriedItemVtableOffset);
        IsLikelyItemStack(carried) && IsReadableAddress(carried, kItemRenderEntrySize)) {
        return carried;
    }

    return nullptr;
}

void* GetMainHandItem(void* localPlayer) {
    if (void* inventoryItem = GetMainHandInventoryItem(localPlayer); IsRenderableItemStack(inventoryItem)) {
        return inventoryItem;
    }

    if (void* carried = CallActorVfunc(localPlayer, kGetCarriedItemVtableOffset); IsRenderableItemStack(carried)) {
        return carried;
    }

    return nullptr;
}

bool DidMainHandItemStackChange(void* localPlayer) {
    if (localPlayer == nullptr) {
        return false;
    }

    void* currentMainHand = GetMainHandItem(localPlayer);
    const bool hasCachedMainHand =
        g_cachedHudCacheValid &&
        g_cachedHudSlotCount > 0 &&
        g_cachedHudSlots[0].hasItem &&
        g_cachedHudSlots[0].sourceItemStack != nullptr;

    if (currentMainHand == nullptr) {
        return hasCachedMainHand;
    }

    if (!hasCachedMainHand) {
        return true;
    }

    return currentMainHand != g_cachedHudSlots[0].sourceItemStack;
}

std::array<void*, kItemSlotCount> CollectLiveItemStacks(void* localPlayer) {
    std::array<void*, kItemSlotCount> itemStacks{};
    if (localPlayer == nullptr) {
        return itemStacks;
    }

    itemStacks[0] = GetMainHandItem(localPlayer);
    if (g_getArmorItemStack != nullptr) {
        for (int slot = 0; slot < 4; ++slot) {
            itemStacks[static_cast<std::size_t>(slot + 1)] = SafeGetArmorItemStack(localPlayer, slot);
        }
    }
    return itemStacks;
}

bool BuildEditorPreviewRenderSlots(void* localPlayer, std::array<HudRenderSlot, kItemSlotCount>& slots, std::size_t& slotCount) {
    void* itemStackTemplate = GetInventoryItemStackTemplate(localPlayer);
    if (itemStackTemplate == nullptr) {
        return false;
    }

    ResolveEditorPreviewItems(localPlayer);
    bool hasAnySlot = false;
    for (std::size_t itemIndex = 0; itemIndex < kEditorPreviewItemNames.size(); ++itemIndex) {
        void* item = g_editorPreviewItems[itemIndex];
        void* itemHolder = g_editorPreviewItemHolders[itemIndex];
        if (item == nullptr || itemHolder == nullptr || !IsReadableAddress(item, sizeof(void*))) {
            continue;
        }

        HudRenderSlot& slot = slots[itemIndex];
        std::memcpy(slot.renderEntry.data(), itemStackTemplate, slot.renderEntry.size());
        std::memcpy(slot.renderEntry.data() + kItemPointerOffset, &itemHolder, sizeof(itemHolder));
        slot.renderEntry[kItemCountOffset] = 1;
        slot.renderEntry[kItemValidOffset] = 1;
        slot.renderEntry[kItemRenderEntryGlintFlagOffset] = 0;
        ItemStackRenderState liveState{};
        if (!ReadItemStackRenderState(slot.renderEntry.data(), liveState) || !IsRenderableItemStackState(liveState)) {
            continue;
        }

        slot.sourceItemStack = slot.renderEntry.data();
        slot.liveState = liveState;
        slot.liveStateValid = true;
        PopulateHudSlotText(slot, slot.renderEntry.data());
        if (slot.durabilityText.empty()) {
            const int current = 1561 - static_cast<int>(itemIndex) * 217;
            slot.currentDurability = std::max(1, current);
            slot.maxDurability = 1561;
            slot.durabilityText = std::to_string(std::max(1, current)) + "/1561";
            slot.durabilityTextColor = GetDurabilityTextColor(std::max(1, current), 1561);
        }
        slot.hasItem = true;
        hasAnySlot = true;
    }

    slotCount = hasAnySlot ? GetFixedItemHudSlotCount() : 0;
    return hasAnySlot;
}
