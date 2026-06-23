bool IsPlausibleGuiSize(const Vec2& size) {
    if (size.x < 240.0f || size.y < 135.0f || size.x > 10000.0f || size.y > 10000.0f) {
        return false;
    }

    const float aspect = size.x / std::max(1.0f, size.y);
    return aspect >= 1.2f && aspect <= 2.4f;
}

bool IsCompatibleGuiSizePair(const Vec2& screenSize, const Vec2& scaledSize) {
    if (!IsPlausibleGuiSize(screenSize) || !IsPlausibleGuiSize(scaledSize)) {
        return false;
    }

    const float screenAspect = screenSize.x / std::max(1.0f, screenSize.y);
    const float scaledAspect = scaledSize.x / std::max(1.0f, scaledSize.y);
    const float aspectDelta = std::fabs(screenAspect - scaledAspect);
    if (aspectDelta > 0.08f) {
        return false;
    }

    const float ratioX = scaledSize.x / std::max(1.0f, screenSize.x);
    const float ratioY = scaledSize.y / std::max(1.0f, screenSize.y);
    return ratioX > 0.05f && ratioY > 0.05f && ratioX <= 1.05f && ratioY <= 1.05f;
}

struct GuiSizeOffsetPair {
    std::size_t screenOffset;
    std::size_t scaledOffset;
};

constexpr GuiSizeOffsetPair kGuiSizeOffsetPairs[] = {
    {kGuiDataScreenSizeOffset, kGuiDataScaledSizeOffset},
    {0x40, 0x50},
    {0x28, 0x38},
    {0x38, 0x48},
};

bool TryReadGuiScreenMetricsFromGuiData(void* guiData, GuiScreenMetrics& metrics) {
    if (!IsReadableAddress(guiData, sizeof(void*))) {
        return false;
    }

    for (const GuiSizeOffsetPair& pair : kGuiSizeOffsetPairs) {
        Vec2 screenSize{};
        Vec2 scaledSize{};
        if (!ReadOffset(guiData, pair.screenOffset, screenSize) ||
            !ReadOffset(guiData, pair.scaledOffset, scaledSize) ||
            !IsCompatibleGuiSizePair(screenSize, scaledSize)) {
            continue;
        }

        metrics = GuiScreenMetrics{screenSize, scaledSize, true};
        return true;
    }

    Vec2 scaledSize{};
    if (ReadOffset(guiData, kGuiDataScaledSizeOffset, scaledSize) && IsPlausibleGuiSize(scaledSize)) {
        metrics = GuiScreenMetrics{scaledSize, scaledSize, true};
        return true;
    }

    return false;
}

bool TryReadGuiScreenMetricsAt(void* clientInstance, std::size_t guiDataOffset, GuiScreenMetrics& metrics) {
    void* guiData = nullptr;
    return ReadOffset(clientInstance, guiDataOffset, guiData) &&
        TryReadGuiScreenMetricsFromGuiData(guiData, metrics);
}

bool ScanClientInstanceForGuiScreenMetrics(void* clientInstance, GuiScreenMetrics& metrics) {
    for (std::size_t guiDataOffset = 0x400; guiDataOffset <= 0x700; guiDataOffset += sizeof(void*)) {
        if (TryReadGuiScreenMetricsAt(clientInstance, guiDataOffset, metrics)) {
            return true;
        }
    }

    return false;
}

GuiScreenMetrics ResolveGuiScreenMetrics(void* clientInstance) {
    GuiScreenMetrics metrics{Vec2{854.0f, 480.0f}, Vec2{854.0f, 480.0f}, false};

    if (TryReadGuiScreenMetricsAt(clientInstance, kClientInstanceGuiDataOffset, metrics)) {
        return metrics;
    }

    for (const std::size_t guiDataOffset : kClientInstanceGuiDataOffsetCandidates) {
        if (guiDataOffset == kClientInstanceGuiDataOffset) {
            continue;
        }

        if (TryReadGuiScreenMetricsAt(clientInstance, guiDataOffset, metrics)) {
            return metrics;
        }
    }

    if (ScanClientInstanceForGuiScreenMetrics(clientInstance, metrics)) {
        return metrics;
    }

    return metrics;
}

void CacheGuiScreenMetrics(void* clientInstance) {
    const DWORD64 now = GetTickCount64();
    if (clientInstance == g_lastHudScreenMetricsClientInstance &&
        now - g_lastHudScreenMetricsAttemptMs < kHudScreenMetricsRetryMs) {
        return;
    }

    g_lastHudScreenMetricsClientInstance = clientInstance;
    g_lastHudScreenMetricsAttemptMs = now;

    const GuiScreenMetrics metrics = ResolveGuiScreenMetrics(clientInstance);
    g_cachedHudScreenMetricsValid = metrics.valid;
    if (metrics.valid) {
        g_cachedHudPixelScreenSize = metrics.screenSize;
        g_cachedHudScreenSize = metrics.scaledSize;
    }
}

void ClearHudRenderCache() {
    g_cachedHudSlotCount = 0;
    g_cachedHudTextOverlayEntryCount = 0;
    g_cachedHudDurabilityBarOverlayEntryCount = 0;
    g_cachedHudScreenMetricsValid = false;
    g_cachedHudCacheValid = false;
    g_cachedHudEditorPreview = false;
    g_cachedHudClientInstance = nullptr;
    g_cachedHudLocalPlayer = nullptr;
    g_cachedHudScreenView = nullptr;
    g_lastHudCacheRefreshMs = 0;
    g_lastHudLiveStateRefreshMs = 0;
    g_lastClientWorldValidationMs = 0;
    g_lastValidatedClientInstance = nullptr;
    g_lastClientWorldValidationResult = false;
    g_lastHudDrawFrameId = 0;
    g_emptyHudRefreshCount = 0;
    g_cachedPlayerInventoryOffset = 0;
    g_cachedSelectedSlot = -1;
    g_cachedInventoryVtable = nullptr;
    g_cachedInventoryGetItem = nullptr;
    g_cachedMinecraftGameClientInstance = nullptr;
    g_cachedMinecraftGame = nullptr;
    g_currentUiFrameLocalPlayerFrameId = 0;
    g_currentUiFrameLocalPlayerClientInstance = nullptr;
    g_currentUiFrameLocalPlayer = nullptr;
    g_currentUiFrameLocalPlayerResolved = false;
    g_inventoryCounterFrameCache = {};
    g_cachedHudTextItemPosition = Vec2{-1.0f, -1.0f};
    g_cachedHudTextDurabilityPosition = Vec2{-1.0f, -1.0f};
    g_cachedHudTextScale = -1.0f;
    g_cachedHudTextShowDurability = false;
    g_cachedHudTextSlotCountForLayout = 0;
    g_cachedHudTextLayout = -1;
    g_cachedHudTextDurabilityStyle = -1;
    for (HudRenderSlot& slot : g_cachedHudSlots) {
        slot.sourceItemStack = nullptr;
        slot.countText.clear();
        slot.durabilityText.clear();
        slot.currentDurability = 0;
        slot.maxDurability = 0;
        slot.hasItem = false;
    }
}

void AddItemHudTextOverlayEntry(
    const std::string& text,
    const MceColor& color,
    float nativeX,
    float nativeY,
    float nativeTextSize,
    bool alignRight) {
    if (text.empty() || g_cachedHudTextOverlayEntryCount >= g_cachedHudTextOverlayEntries.size()) {
        return;
    }

    HudTextOverlayEntry& entry = g_cachedHudTextOverlayEntries[g_cachedHudTextOverlayEntryCount++];
    entry.text = text;
    entry.color = color;
    entry.nativePosition = Vec2{nativeX, nativeY};
    entry.nativeTextSize = nativeTextSize;
    entry.alignRight = alignRight;
}

void AddItemHudDurabilityBarOverlayEntry(
    float nativeX,
    float nativeY,
    float nativeScale,
    int currentDurability,
    int maxDurability) {
    if (maxDurability <= 0 || currentDurability < 0 || currentDurability >= maxDurability ||
        g_cachedHudDurabilityBarOverlayEntryCount >= g_cachedHudDurabilityBarOverlayEntries.size()) {
        return;
    }

    HudDurabilityBarOverlayEntry& entry =
        g_cachedHudDurabilityBarOverlayEntries[g_cachedHudDurabilityBarOverlayEntryCount++];
    entry.nativePosition = Vec2{nativeX, nativeY};
    entry.nativeScale = nativeScale;
    entry.currentDurability = currentDurability;
    entry.maxDurability = maxDurability;
}

void CacheItemHudTextOverlayEntries(
    const Vec2& itemPosition,
    const Vec2& durabilityPosition,
    float hudScale,
    bool showDurability) {
    g_cachedHudTextOverlayEntryCount = 0;
    g_cachedHudDurabilityBarOverlayEntryCount = 0;
    for (std::size_t i = 0; i < g_cachedHudSlotCount; ++i) {
        const HudRenderSlot& slot = g_cachedHudSlots[i];
        if (!slot.hasItem) {
            continue;
        }

        const float itemX = itemPosition.x + (IsItemHudHorizontalLayout() ? GetItemHudSlotOffset(i, hudScale) : 0.0f);
        const float itemY = itemPosition.y + (IsItemHudHorizontalLayout() ? 0.0f : GetItemHudSlotOffset(i, hudScale));
        AddItemHudTextOverlayEntry(
            slot.countText,
            MceColor{1.0f, 1.0f, 1.0f, 1.0f},
            itemX + (kItemSize + 1.0f) * hudScale,
            itemY + (kItemSize * 0.50f) * hudScale,
            12.5f * hudScale,
            true);
        if (showDurability && ShouldUseItemHudDurabilityBar()) {
            AddItemHudDurabilityBarOverlayEntry(
                itemX,
                itemY,
                hudScale,
                slot.currentDurability,
                slot.maxDurability);
        } else if (showDurability) {
            const float durabilityX = durabilityPosition.x +
                0.0f;
            const float durabilityY = durabilityPosition.y +
                GetItemHudDurabilitySlotOffset(i, hudScale);
            AddItemHudTextOverlayEntry(
                slot.durabilityText,
                slot.durabilityTextColor,
                durabilityX,
                durabilityY,
                10.5f * hudScale,
                false);
        }
    }

    g_cachedHudTextItemPosition = itemPosition;
    g_cachedHudTextDurabilityPosition = durabilityPosition;
    g_cachedHudTextScale = hudScale;
    g_cachedHudTextShowDurability = showDurability;
    g_cachedHudTextSlotCountForLayout = g_cachedHudSlotCount;
    g_cachedHudTextLayout = g_itemHudLayout.load(std::memory_order_relaxed);
    g_cachedHudTextDurabilityStyle = g_itemHudDurabilityStyle.load(std::memory_order_relaxed);
}

bool ShouldRebuildItemHudTextOverlayEntries(
    const Vec2& itemPosition,
    const Vec2& durabilityPosition,
    float hudScale,
    bool showDurability,
    bool refreshedCache) {
    return refreshedCache ||
        g_cachedHudTextSlotCountForLayout != g_cachedHudSlotCount ||
        g_cachedHudTextLayout != g_itemHudLayout.load(std::memory_order_relaxed) ||
        g_cachedHudTextDurabilityStyle != g_itemHudDurabilityStyle.load(std::memory_order_relaxed) ||
        g_cachedHudTextShowDurability != showDurability ||
        g_cachedHudTextScale != hudScale ||
        g_cachedHudTextItemPosition.x != itemPosition.x ||
        g_cachedHudTextItemPosition.y != itemPosition.y ||
        g_cachedHudTextDurabilityPosition.x != durabilityPosition.x ||
        g_cachedHudTextDurabilityPosition.y != durabilityPosition.y;
}

bool RefreshHudRenderCache(void* clientInstance, void* localPlayer) {
    const bool editorActive = tane::gui::IsGuiPositionEditorActive();
    std::array<HudRenderSlot, kItemSlotCount> slots{};
    std::size_t slotCount = 0;
    int selectedSlot = -1;
    if (TryGetSelectedSlot(localPlayer, selectedSlot)) {
        g_cachedSelectedSlot = selectedSlot;
    }

    if (editorActive) {
        BuildEditorPreviewRenderSlots(localPlayer, slots, slotCount);
    }

    if (slotCount == 0 && !editorActive) {
        bool hasAnySlot = false;
        const std::array<void*, kItemSlotCount> itemStacks = CollectLiveItemStacks(localPlayer);
        for (std::size_t slotIndex = 0; slotIndex < itemStacks.size(); ++slotIndex) {
            void* itemStack = itemStacks[slotIndex];
            if (!IsRenderableItemStack(itemStack)) {
                continue;
            }

            HudRenderSlot& slot = slots[slotIndex];
            if (!BuildRenderEntry(itemStack, slot.renderEntry)) {
                continue;
            }

            ItemStackRenderState liveState{};
            if (!ReadItemStackRenderState(itemStack, liveState) || !IsRenderableItemStackState(liveState)) {
                continue;
            }

            slot.sourceItemStack = itemStack;
            slot.liveState = liveState;
            slot.liveStateValid = true;
            PopulateHudSlotText(slot, itemStack);
            slot.hasItem = true;
            hasAnySlot = true;
        }
        slotCount = hasAnySlot ? GetFixedItemHudSlotCount() : 0;
    }

    g_cachedHudSlots = slots;
    g_cachedHudSlotCount = slotCount;
    if (slotCount == 0) {
        g_cachedHudTextOverlayEntryCount = 0;
        g_cachedHudDurabilityBarOverlayEntryCount = 0;
    }
    g_cachedHudEditorPreview = editorActive && slotCount > 0;
    g_emptyHudRefreshCount = 0;
    CacheGuiScreenMetrics(clientInstance);
    g_cachedHudClientInstance = clientInstance;
    g_cachedHudLocalPlayer = localPlayer;
    g_lastHudCacheRefreshMs = GetTickCount64();
    g_lastHudLiveStateRefreshMs = g_lastHudCacheRefreshMs;
    g_cachedHudCacheValid = true;
    return g_cachedHudSlotCount > 0;
}

void* GetClientInstanceFromMinecraftUiRenderContextUnchecked(void* minecraftUiRenderContext) {
    void* clientInstance = nullptr;
    return ReadOffsetUnchecked(minecraftUiRenderContext, kMinecraftUIRenderContextClientInstanceOffset, clientInstance) ? clientInstance : nullptr;
}

bool IsClientWorldStateReady(void* clientInstance) {
    if (clientInstance == nullptr) {
        return false;
    }

    const DWORD64 now = GetTickCount64();
    if (g_lastValidatedClientInstance == clientInstance &&
        now - g_lastClientWorldValidationMs < 100) {
        return g_lastClientWorldValidationResult;
    }

    void* localPlayer = CallClientInstanceGetLocalPlayer(clientInstance);
    void* level = nullptr;
    const bool ready =
        localPlayer != nullptr &&
        ReadOffset(localPlayer, kActorLevelOffset, level) &&
        level != nullptr;
    g_lastValidatedClientInstance = clientInstance;
    g_lastClientWorldValidationMs = now;
    g_lastClientWorldValidationResult = ready;
    return ready;
}

struct ClientFrameTickState {
    bool ping = false;
    bool tab = false;
    bool arrowCounter = false;
    bool potCounter = false;
    bool effectHud = false;
    bool nameTags = false;
    bool fullbright = false;
    bool noFog = false;
    bool hitbox = false;
    bool tracer = false;
    bool autoSprint = false;
    bool zoom = false;
    bool freeLook = false;
};

ClientFrameTickState ResolveClientFrameTickState() {
    ClientFrameTickState state{};
    state.ping = tane::gui::IsPingEnabled();
    state.tab = tane::gui::IsTabEnabled();
    state.arrowCounter = tane::gui::IsArrowCounterEnabled();
    state.potCounter = tane::gui::IsPotCounterEnabled();
    state.effectHud = tane::gui::IsEffectHudEnabled();
    state.nameTags = tane::render::IsNameTagsEnabled();
    state.fullbright = tane::render::IsFullbrightEnabled();
    state.noFog = tane::render::IsNoFogEnabled();
    state.hitbox = tane::render::IsHitboxEnabled();
    state.tracer = tane::render::IsTracerEnabled();
    state.autoSprint = tane::movement::IsAutoSprintEnabled();
    state.zoom = tane::camera::IsZoomEnabled();
    state.freeLook = tane::camera::IsFreeLookEnabled();
    return state;
}

bool NeedsValidatedClientWorldState(const ClientFrameTickState& state) {
    return state.ping ||
        state.tab ||
        state.arrowCounter ||
        state.potCounter ||
        state.effectHud ||
        state.hitbox ||
        state.tracer ||
        state.autoSprint ||
        state.zoom ||
        state.freeLook;
}

void TickClientFrameFromScreenView(void* clientInstance, bool isHudScreen) {
    g_uiFrameId.fetch_add(1, std::memory_order_relaxed);
    tane::patch::TickForceCloseOreUi(clientInstance);
    if (!isHudScreen) {
        if (clientInstance == nullptr) {
            BlockGameplayModulesForClientTransition();
        }
        return;
    }

    const std::uint64_t fpsFrameId = tane::gui::GetFpsFrameId();
    if (fpsFrameId == g_lastClientTickFpsFrameId) {
        return;
    }
    g_lastClientTickFpsFrameId = fpsFrameId;

    const ClientFrameTickState tickState = ResolveClientFrameTickState();
    if (clientInstance != nullptr && NeedsValidatedClientWorldState(tickState) && !IsClientWorldStateReady(clientInstance)) {
        clientInstance = nullptr;
    }
    if (tickState.ping) {
        tane::gui::TickPing(clientInstance);
    }
    if (clientInstance == nullptr) {
        BlockGameplayModulesForClientTransition();
    }
    tane::imgui_menu::TickInputBlock(clientInstance);
    if (tickState.nameTags) {
        tane::render::TickNameTags(clientInstance);
    }
    if (tickState.fullbright) {
        tane::render::TickFullbright(clientInstance);
    }
    if (tickState.noFog) {
        tane::render::TickNoFog(clientInstance);
    }
    if (tickState.hitbox) {
        tane::render::TickHitbox(clientInstance);
    }
    if (tickState.tracer) {
        tane::render::TickTracer(clientInstance);
    }
    if (tickState.tab) {
        tane::gui::TickTab(clientInstance);
    }
    if (tickState.arrowCounter) {
        tane::gui::TickArrowCounter(clientInstance);
    }
    if (tickState.potCounter) {
        tane::gui::TickPotCounter(clientInstance);
    }
    if (tickState.effectHud) {
        tane::gui::TickEffectHud(clientInstance);
    }
    const bool canRunMovementModules = clientInstance != nullptr && tane::gui::CanRunGameplayModules();
    const bool canRunCameraModules = clientInstance != nullptr && tane::gui::CanRunGameplayModules();
    if (canRunMovementModules) {
        if (tickState.autoSprint) {
            tane::movement::TickAutoSprint(clientInstance);
        }
    } else if (tickState.autoSprint) {
        tane::movement::TickAutoSprint(nullptr);
    }

    if (canRunCameraModules) {
        if (tickState.zoom) {
            tane::camera::TickZoom(clientInstance);
        }
        if (tickState.freeLook) {
            tane::camera::TickFreeLook(clientInstance);
        }
    } else {
        if (tickState.zoom) {
            tane::camera::TickZoom(nullptr);
        }
        if (tickState.freeLook) {
            tane::camera::TickFreeLook(nullptr);
        }
    }
}

float NativeTextMeasureFromOverlaySize(float overlayTextSize) {
    return std::clamp(overlayTextSize / 15.625f, 0.45f, 2.0f);
}

bool DrawNativeOverlayTextEntry(void* minecraftUiRenderContext, const HudTextOverlayEntry& entry) {
    if (entry.text.empty()) {
        return false;
    }

    constexpr float kDefaultTextWidth = 112.0f;
    constexpr float kDefaultTextHeight = 18.0f;
    const float textScale = NativeTextMeasureFromOverlaySize(entry.nativeTextSize);
    const float width = std::max(24.0f, kDefaultTextWidth * textScale);
    const RectangleArea rect{
        entry.alignRight ? entry.nativePosition.x - width : entry.nativePosition.x,
        entry.alignRight ? entry.nativePosition.x : entry.nativePosition.x + width,
        entry.nativePosition.y,
        entry.nativePosition.y + kDefaultTextHeight * textScale,
    };
    return SafeDrawDebugText(
        minecraftUiRenderContext,
        rect,
        entry.text,
        entry.color,
        entry.alignRight ? TextAlignment::Right : TextAlignment::Left,
        textScale);
}

void DrawNativeDurabilityBarEntry(void* minecraftUiRenderContext, const HudDurabilityBarOverlayEntry& entry) {
    if (entry.maxDurability <= 0) {
        return;
    }

    constexpr float kVanillaDurabilityBarX = 2.0f;
    constexpr float kVanillaDurabilityBarY = 13.0f;
    constexpr float kVanillaDurabilityBarWidth = 13.0f;
    constexpr float kVanillaDurabilityBarHeight = 2.0f;
    constexpr float kVanillaDurabilityFillHeight = 1.0f;
    const float nativeScale = std::max(0.01f, entry.nativeScale);
    const float ratio = std::clamp(
        static_cast<float>(entry.currentDurability) / static_cast<float>(entry.maxDurability),
        0.0f,
        1.0f);
    const float barX = entry.nativePosition.x + kVanillaDurabilityBarX * nativeScale;
    const float barY = entry.nativePosition.y + kVanillaDurabilityBarY * nativeScale;
    const float barWidth = kVanillaDurabilityBarWidth * nativeScale;
    const float barHeight = kVanillaDurabilityBarHeight * nativeScale;
    SafeFillRectangle(
        minecraftUiRenderContext,
        RectangleArea{barX, barX + barWidth, barY, barY + barHeight},
        MceColor{0.0f, 0.0f, 0.0f, 0.86f},
        0.86f);

    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    HsvToRgb(ratio / 3.0f, 1.0f, 1.0f, red, green, blue);
    const float fillWidth = std::max(nativeScale, barWidth * ratio);
    SafeFillRectangle(
        minecraftUiRenderContext,
        RectangleArea{barX, barX + fillWidth, barY, barY + kVanillaDurabilityFillHeight * nativeScale},
        MceColor{red, green, blue, 1.0f},
        1.0f);
}

void DrawItemHudNativeOverlays(void* minecraftUiRenderContext) {
    if (minecraftUiRenderContext == nullptr ||
        (g_cachedHudTextOverlayEntryCount == 0 && g_cachedHudDurabilityBarOverlayEntryCount == 0)) {
        return;
    }

    if (ResolveNativeTextFunctions(minecraftUiRenderContext)) {
        bool drewText = false;
        for (std::size_t i = 0; i < g_cachedHudTextOverlayEntryCount; ++i) {
            drewText = DrawNativeOverlayTextEntry(minecraftUiRenderContext, g_cachedHudTextOverlayEntries[i]) || drewText;
        }
        if (drewText) {
            SafeFlushText(minecraftUiRenderContext);
        }
    }

    for (std::size_t i = 0; i < g_cachedHudDurabilityBarOverlayEntryCount; ++i) {
        DrawNativeDurabilityBarEntry(minecraftUiRenderContext, g_cachedHudDurabilityBarOverlayEntries[i]);
    }
}

void DrawItemHud(void* minecraftUiRenderContext, void* hookClientInstance) {
    const bool positionEditorActive = tane::gui::IsGuiPositionEditorActive();
    if ((!g_itemHudEnabled.load(std::memory_order_relaxed) && !positionEditorActive) ||
        g_renderDecoratedGuiItem == nullptr) {
        ClearHudRenderCache();
        return;
    }

    void* clientInstance = hookClientInstance;
    if (clientInstance == nullptr) {
        return;
    }
    if (positionEditorActive || !g_cachedHudScreenMetricsValid) {
        CacheGuiScreenMetrics(clientInstance);
    }

    const DWORD64 now = GetTickCount64();
    void* localPlayer = g_cachedHudLocalPlayer;
    const bool needsTimedRefresh =
        !g_cachedHudCacheValid ||
        g_cachedHudEditorPreview != positionEditorActive ||
        g_cachedHudClientInstance != clientInstance ||
        now - g_lastHudCacheRefreshMs >= kHudCacheRefreshMs;
    if (!needsTimedRefresh && !positionEditorActive && localPlayer == nullptr) {
        localPlayer = CallClientInstanceGetLocalPlayer(clientInstance);
    }
    const bool selectedSlotChanged =
        !needsTimedRefresh &&
        !positionEditorActive &&
        localPlayer != nullptr &&
        DidSelectedSlotChange(localPlayer);
    const bool mainHandItemStackChanged =
        !needsTimedRefresh &&
        !selectedSlotChanged &&
        !positionEditorActive &&
        localPlayer != nullptr &&
        DidMainHandItemStackChange(localPlayer);
    bool refreshedCache = false;

    if (needsTimedRefresh || selectedSlotChanged || mainHandItemStackChanged) {
        if (needsTimedRefresh) {
            localPlayer = CallClientInstanceGetLocalPlayer(clientInstance);
        }
        if (localPlayer == nullptr) {
            g_localPlayerSeen.store(false, std::memory_order_relaxed);
            ClearHudRenderCache();
            return;
        }
        g_localPlayerSeen.store(true, std::memory_order_relaxed);

        if (!RefreshHudRenderCache(clientInstance, localPlayer)) {
            return;
        }
        refreshedCache = true;
    }
    if (g_cachedHudSlotCount == 0) {
        g_cachedHudTextOverlayEntryCount = 0;
        g_cachedHudDurabilityBarOverlayEntryCount = 0;
        return;
    }

    const bool showDurability =
        g_itemHudShowDurability.load(std::memory_order_relaxed) || positionEditorActive;
    bool liveStateChanged = false;
    if (!refreshedCache &&
        !positionEditorActive &&
        now - g_lastHudLiveStateRefreshMs >= kHudLiveStateRefreshMs) {
        if (!RefreshCachedHudSlotLiveState(showDurability, liveStateChanged)) {
            if (localPlayer == nullptr) {
                localPlayer = CallClientInstanceGetLocalPlayer(clientInstance);
            }
            if (localPlayer == nullptr || !RefreshHudRenderCache(clientInstance, localPlayer)) {
                return;
            }
            refreshedCache = true;
            if (g_cachedHudSlotCount == 0) {
                g_cachedHudTextOverlayEntryCount = 0;
                g_cachedHudDurabilityBarOverlayEntryCount = 0;
                return;
            }
        } else {
            g_lastHudLiveStateRefreshMs = now;
        }
    }

    const Vec2 screenSize = g_cachedHudScreenSize;
    const Vec2 hudPosition = GetEffectiveItemHudPosition(g_cachedHudSlotCount, screenSize);
    const float hudScale = GetClampedItemHudScale();
    const Vec2 durabilityPosition = GetEffectiveItemHudDurabilityPosition(g_cachedHudSlotCount, screenSize, hudPosition);
    if (ShouldRebuildItemHudTextOverlayEntries(hudPosition, durabilityPosition, hudScale, showDurability, refreshedCache || liveStateChanged)) {
        CacheItemHudTextOverlayEntries(hudPosition, durabilityPosition, hudScale, showDurability);
    }

    for (std::size_t i = 0; i < g_cachedHudSlotCount; ++i) {
        const HudRenderSlot& slot = g_cachedHudSlots[i];
        if (!slot.hasItem) {
            continue;
        }

        const float x = hudPosition.x + (IsItemHudHorizontalLayout() ? GetItemHudSlotOffset(i, hudScale) : 0.0f);
        const float y = hudPosition.y + (IsItemHudHorizontalLayout() ? 0.0f : GetItemHudSlotOffset(i, hudScale));

        SafeRenderDecoratedGuiItem(
            minecraftUiRenderContext,
            clientInstance,
            const_cast<std::uint8_t*>(slot.renderEntry.data()),
            x,
            y,
            1.0f,
            hudScale,
            17);
    }

}

void __fastcall HookScreenViewSetupAndRender(void* screenView, void* minecraftUiRenderContext) {
    const bool isHudScreen = UpdateGuiScreenVisibilityForScreenView(screenView);
    void* previousClientInstance = g_currentUiFrameClientInstance;
    void* frameClientInstance = GetClientInstanceFromMinecraftUiRenderContextUnchecked(minecraftUiRenderContext);
    g_currentUiFrameClientInstance = frameClientInstance;
    TickClientFrameFromScreenView(g_currentUiFrameClientInstance, isHudScreen);

    const bool positionEditorActive = tane::gui::IsGuiPositionEditorActive();
    const bool showGuiOverlay = positionEditorActive || tane::gui::ShouldShowGuiOverlay();
    const bool nativeHudAllowed = !positionEditorActive;
    const bool shouldDrawItemHud =
        nativeHudAllowed &&
        (g_itemHudEnabled.load(std::memory_order_relaxed) || positionEditorActive) &&
        showGuiOverlay &&
        (isHudScreen || positionEditorActive) &&
        ShouldDrawItemHudForFrame(screenView);
    g_originalScreenViewSetupAndRender(screenView, minecraftUiRenderContext);

    const bool shouldDrawNativeOverlay = shouldDrawItemHud;
    if (!shouldDrawNativeOverlay) {
        g_currentUiFrameClientInstance = previousClientInstance;
        return;
    }

    void* renderClientInstance = GetClientInstanceFromMinecraftUiRenderContextUnchecked(minecraftUiRenderContext);
    if (renderClientInstance == nullptr ||
        renderClientInstance != frameClientInstance ||
        !IsClientWorldStateReady(renderClientInstance)) {
        BlockGameplayModulesForClientTransition();
        g_currentUiFrameClientInstance = previousClientInstance;
        return;
    }
    g_currentUiFrameClientInstance = renderClientInstance;

    const bool shouldUseDirectGuiItemFrame = shouldDrawItemHud;
    if (shouldUseDirectGuiItemFrame) {
        BeginDirectGuiItemFrame(minecraftUiRenderContext, g_currentUiFrameClientInstance);
    }

    if (shouldDrawItemHud) {
        DrawItemHud(minecraftUiRenderContext, g_currentUiFrameClientInstance);
    }
    if (shouldUseDirectGuiItemFrame) {
        EndDirectGuiItemFrame();
    }
    g_currentUiFrameClientInstance = previousClientInstance;
}

} // namespace

bool GetNativeHudScreenSize(void* clientInstance, float& width, float& height) {
    if (clientInstance != nullptr && !g_cachedHudScreenMetricsValid) {
        CacheGuiScreenMetrics(clientInstance);
    }

    Vec2 screenSize = g_cachedHudScreenSize;
    if (screenSize.x <= 0.0f || screenSize.y <= 0.0f) {
        screenSize = Vec2{854.0f, 480.0f};
    }

    width = screenSize.x;
    height = screenSize.y;
    return width > 0.0f && height > 0.0f;
}

void RenderNativeDecoratedGuiItem(
    void* minecraftUiRenderContext,
    void* clientInstance,
    void* itemStack,
    float x,
    float y,
    float opacity,
    float scale,
    int renderVariant) {
    SafeRenderDecoratedGuiItem(minecraftUiRenderContext, clientInstance, itemStack, x, y, opacity, scale, renderVariant);
}

bool FillNativeRectangle(
    void* minecraftUiRenderContext,
    float x,
    float y,
    float width,
    float height,
    float r,
    float g,
    float b,
    float a) {
    if (minecraftUiRenderContext == nullptr || width <= 0.0f || height <= 0.0f) {
        return false;
    }

    const RectangleArea rect{x, x + width, y, y + height};
    const MceColor color{
        std::clamp(r, 0.0f, 1.0f),
        std::clamp(g, 0.0f, 1.0f),
        std::clamp(b, 0.0f, 1.0f),
        std::clamp(a, 0.0f, 1.0f),
    };
    return SafeFillRectangle(minecraftUiRenderContext, rect, color, color.a);
}

bool QueryInventoryCounterCounts(void* clientInstance, int& arrowCount, int& healingSplashCount) {
    return QueryInventoryCounterCountsForFrame(clientInstance, arrowCount, healingSplashCount);
}

bool QueryInventoryCounterCountsAndRenderEntries(
    void* clientInstance,
    int& arrowCount,
    int& healingSplashCount,
    void* arrowRenderEntry,
    void* healingSplashRenderEntry,
    std::size_t renderEntrySize) {
    return QueryInventoryCounterCountsAndRenderEntriesForFrame(
        clientInstance,
        arrowCount,
        healingSplashCount,
        arrowRenderEntry,
        healingSplashRenderEntry,
        renderEntrySize);
}

bool DrawNativeCounterText(
    void* minecraftUiRenderContext,
    const char* text,
    float x,
    float y,
    float width,
    float height,
    float textSize) {
    if (minecraftUiRenderContext == nullptr || text == nullptr || text[0] == '\0') {
        return false;
    }

    if (!ResolveNativeTextFunctions(minecraftUiRenderContext)) {
        return false;
    }

    const RectangleArea rect{x, x + std::max(1.0f, width), y, y + std::max(1.0f, height)};
    const bool drew = SafeDrawDebugText(
        minecraftUiRenderContext,
        rect,
        std::string(text),
        MceColor{1.0f, 1.0f, 1.0f, 1.0f},
        TextAlignment::Left,
        std::clamp(textSize, 0.45f, 2.0f));
    if (drew) {
        SafeFlushText(minecraftUiRenderContext);
    }
    return drew;
}

void RenderItemHudTextOverlay() {
    if (ImGui::GetCurrentContext() == nullptr ||
        (g_cachedHudTextOverlayEntryCount == 0 && g_cachedHudDurabilityBarOverlayEntryCount == 0)) {
        return;
    }

    const bool positionEditorActive = IsGuiPositionEditorActive();
    if (positionEditorActive) {
        return;
    }
    if (!ShouldShowGuiOverlay()) {
        ClearHudRenderCache();
        return;
    }

    const Vec2 nativeScreen = g_cachedHudScreenSize;
    if (nativeScreen.x <= 0.0f || nativeScreen.y <= 0.0f) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float scaleX = io.DisplaySize.x / std::max(1.0f, nativeScreen.x);
    const float scaleY = io.DisplaySize.y / std::max(1.0f, nativeScreen.y);
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    if (drawList == nullptr || font == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < g_cachedHudTextOverlayEntryCount; ++i) {
        const HudTextOverlayEntry& entry = g_cachedHudTextOverlayEntries[i];
        if (entry.text.empty()) {
            continue;
        }

        const float fontSize = std::clamp(entry.nativeTextSize * scaleY, 8.0f, 28.0f);
        ImVec2 pos(entry.nativePosition.x * scaleX, entry.nativePosition.y * scaleY);
        if (entry.alignRight) {
            const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, entry.text.c_str());
            pos.x -= textSize.x;
        }

        const int red = std::clamp(static_cast<int>(entry.color.r * 255.0f), 0, 255);
        const int green = std::clamp(static_cast<int>(entry.color.g * 255.0f), 0, 255);
        const int blue = std::clamp(static_cast<int>(entry.color.b * 255.0f), 0, 255);
        const int alpha = std::clamp(static_cast<int>(entry.color.a * 255.0f), 0, 255);
        const float shadowOffset = std::max(1.0f, fontSize / 14.0f);
        DrawGuiTextWithShadow(
            drawList,
            font,
            fontSize,
            pos,
            IM_COL32(red, green, blue, alpha),
            entry.text.c_str(),
            shadowOffset,
            IM_COL32(0, 0, 0, alpha));
    }

    constexpr float kVanillaDurabilityBarX = 2.0f;
    constexpr float kVanillaDurabilityBarY = 13.0f;
    constexpr float kVanillaDurabilityBarWidth = 13.0f;
    constexpr float kVanillaDurabilityBarHeight = 2.0f;
    constexpr float kVanillaDurabilityFillHeight = 1.0f;
    for (std::size_t i = 0; i < g_cachedHudDurabilityBarOverlayEntryCount; ++i) {
        const HudDurabilityBarOverlayEntry& entry = g_cachedHudDurabilityBarOverlayEntries[i];
        if (entry.maxDurability <= 0) {
            continue;
        }

        const float ratio = std::clamp(
            static_cast<float>(entry.currentDurability) / static_cast<float>(entry.maxDurability),
            0.0f,
            1.0f);
        const float nativeScale = std::max(0.01f, entry.nativeScale);
        const ImVec2 barMin(
            (entry.nativePosition.x + kVanillaDurabilityBarX * nativeScale) * scaleX,
            (entry.nativePosition.y + kVanillaDurabilityBarY * nativeScale) * scaleY);
        const ImVec2 barMax(
            (entry.nativePosition.x + (kVanillaDurabilityBarX + kVanillaDurabilityBarWidth) * nativeScale) * scaleX,
            (entry.nativePosition.y + (kVanillaDurabilityBarY + kVanillaDurabilityBarHeight) * nativeScale) * scaleY);
        const float fillWidth = std::max(1.0f, (barMax.x - barMin.x) * ratio);
        const ImVec2 fillMax(
            barMin.x + fillWidth,
            (entry.nativePosition.y + (kVanillaDurabilityBarY + kVanillaDurabilityFillHeight) * nativeScale) * scaleY);

        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        HsvToRgb(ratio / 3.0f, 1.0f, 1.0f, red, green, blue);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 220), 0.0f);
        drawList->AddRectFilled(
            barMin,
            ImVec2(std::min(barMax.x, fillMax.x), std::max(barMin.y + 1.0f, fillMax.y)),
            IM_COL32(
                std::clamp(static_cast<int>(red * 255.0f), 0, 255),
                std::clamp(static_cast<int>(green * 255.0f), 0, 255),
                std::clamp(static_cast<int>(blue * 255.0f), 0, 255),
                255),
            0.0f);
    }
}
