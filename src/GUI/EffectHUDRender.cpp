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

void* GetImageAddress(std::uintptr_t imageBase, std::uintptr_t rva) {
    return imageBase != 0 && rva != 0 ? reinterpret_cast<void*>(imageBase + rva) : nullptr;
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

void* GetCachedExecutableVfunc(void* object, std::size_t vtableOffset, VfuncCache& cache) {
    void** vtable = nullptr;
    if (!ReadValue(object, vtable) || vtable == nullptr) {
        return nullptr;
    }

    if (cache.vtable == vtable && cache.offset == vtableOffset && cache.function != nullptr) {
        return cache.function;
    }

    void* function = nullptr;
    if (!ReadOffset(vtable, vtableOffset, function) || !IsExecutableAddress(function)) {
        cache = {};
        return nullptr;
    }

    cache.vtable = vtable;
    cache.offset = vtableOffset;
    cache.function = function;
    return function;
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

void* GetLocalPlayer(void* clientInstance) {
    void* function = GetCachedExecutableVfunc(clientInstance, kClientInstanceLocalPlayerVtableOffset, g_localPlayerVfunc);
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
        if (!ReadValue(slot, entryIndex) || entryIndex == UINT64_MAX) {
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

void* ResolveComponent(void* actor, std::uint32_t hash, std::size_t entrySize) {
    void* entityContext = nullptr;
    std::uint32_t entityId = 0;
    if (!ReadOffset(actor, kActorEntityContextOffset, entityContext) ||
        !ReadOffset(actor, kActorEntityIdOffset, entityId) ||
        entityContext == nullptr) {
        return nullptr;
    }

    void* storage = FindComponentStorage(entityContext, hash);
    return storage != nullptr ? ResolveComponentFromStorage(storage, entityId, entrySize) : nullptr;
}

bool ReadLiveEffects(void* clientInstance, std::array<EffectHudEntry, kMaxEffectHudEntries>& entries, int& count) {
    count = 0;
    void* localPlayer = GetLocalPlayer(clientInstance);
    void* component = ResolveComponent(localPlayer, kMobEffectsComponentHash, kMobEffectsComponentEntrySize);
    if (component == nullptr) {
        return false;
    }

    void* beginPtr = nullptr;
    void* endPtr = nullptr;
    if (!ReadOffset(component, kMobEffectsBeginOffset, beginPtr) ||
        !ReadOffset(component, kMobEffectsEndOffset, endPtr) ||
        beginPtr == nullptr ||
        endPtr == nullptr) {
        return true;
    }

    const auto begin = reinterpret_cast<std::uintptr_t>(beginPtr);
    const auto end = reinterpret_cast<std::uintptr_t>(endPtr);
    if (end < begin || (end - begin) > kMobEffectInstanceSize * 64) {
        return false;
    }

    const std::size_t rawCount = (end - begin) / kMobEffectInstanceSize;
    for (std::size_t index = 0; index < rawCount && count < kMaxEffectHudEntries; ++index) {
        const void* instance = reinterpret_cast<const std::uint8_t*>(beginPtr) + index * kMobEffectInstanceSize;
        int effectId = 0;
        int durationTicks = 0;
        int amplifier = 0;
        if (!ReadOffset(instance, kMobEffectIdOffset, effectId) ||
            !ReadOffset(instance, kMobEffectDurationOffset, durationTicks) ||
            !ReadOffset(instance, kMobEffectAmplifierOffset, amplifier)) {
            continue;
        }

        const EffectInfo* info = GetEffectInfo(effectId);
        if (info == nullptr || durationTicks == 0) {
            continue;
        }

        entries[count++] = EffectHudEntry{
            effectId,
            std::clamp(amplifier + 1, 1, 255),
            durationTicks,
            info,
        };
    }

    return true;
}

void FillPreviewEffects(std::array<EffectHudEntry, kMaxEffectHudEntries>& entries, int& count) {
    count = 0;
    const EffectInfo* speed = GetEffectInfo(1);
    const EffectInfo* resistance = GetEffectInfo(11);
    const EffectInfo* regen = GetEffectInfo(10);
    constexpr int kPreviewDurationTicks = 10 * 60 * 20;
    if (speed != nullptr) {
        entries[count++] = EffectHudEntry{1, 2, kPreviewDurationTicks, speed};
    }
    if (resistance != nullptr) {
        entries[count++] = EffectHudEntry{11, 5, kPreviewDurationTicks, resistance};
    }
    if (regen != nullptr) {
        entries[count++] = EffectHudEntry{10, 10, kPreviewDurationTicks, regen};
    }
}

int GetEstimatedRemainingTicks(EffectHudEntry& entry, DWORD64 now) {
    if (entry.durationTicks <= 0) {
        return entry.durationTicks;
    }

    EffectTimerState* freeState = nullptr;
    EffectTimerState* state = nullptr;
    for (EffectTimerState& candidate : g_effectTimers) {
        if (candidate.active && candidate.id == entry.id && candidate.level == entry.level) {
            state = &candidate;
            break;
        }
        if (!candidate.active && freeState == nullptr) {
            freeState = &candidate;
        }
    }

    if (state == nullptr) {
        state = freeState != nullptr ? freeState : &g_effectTimers[0];
        *state = EffectTimerState{
            entry.id,
            entry.level,
            entry.durationTicks,
            now,
            now,
            true,
        };
    }

    const DWORD64 elapsedTicks = (now > state->sourceTick) ? ((now - state->sourceTick) / 50) : 0;
    int estimatedTicks = std::max(0, state->sourceDurationTicks - static_cast<int>(std::min<DWORD64>(elapsedTicks, 0x7FFFFFFF)));

    const bool serverDurationRefreshed = entry.durationTicks > state->sourceDurationTicks + 20;
    const bool serverDurationIsLower = entry.durationTicks + 20 < estimatedTicks;
    if (serverDurationRefreshed || serverDurationIsLower || state->id != entry.id || state->level != entry.level) {
        state->id = entry.id;
        state->level = entry.level;
        state->sourceDurationTicks = entry.durationTicks;
        state->sourceTick = now;
        estimatedTicks = entry.durationTicks;
    }

    state->lastSeenTick = now;
    entry.durationTicks = estimatedTicks;
    return estimatedTicks;
}

bool IsEffectEntryPresent(
    const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries,
    int count,
    int id,
    int level) {
    for (int index = 0; index < count; ++index) {
        if (entries[index].id == id && entries[index].level == level) {
            return true;
        }
    }
    return false;
}

void ClearEffectTimer(int id, int level) {
    for (EffectTimerState& state : g_effectTimers) {
        if (state.active && state.id == id && state.level == level) {
            state = {};
            return;
        }
    }
}

void ClearMissingEffectTimers(const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries, int count) {
    if (count <= 0) {
        g_effectTimers = {};
        return;
    }

    for (EffectTimerState& state : g_effectTimers) {
        if (state.active && !IsEffectEntryPresent(entries, count, state.id, state.level)) {
            state = {};
        }
    }
}

void ApplyEffectCountdown(std::array<EffectHudEntry, kMaxEffectHudEntries>& entries, int& count) {
    const DWORD64 now = GetTickCount64();
    ClearMissingEffectTimers(entries, count);

    int writeIndex = 0;
    for (int index = 0; index < count; ++index) {
        const int remainingTicks = GetEstimatedRemainingTicks(entries[index], now);
        if (remainingTicks == 0) {
            ClearEffectTimer(entries[index].id, entries[index].level);
            continue;
        }
        if (writeIndex != index) {
            entries[writeIndex] = entries[index];
        }
        ++writeIndex;
    }
    for (int index = writeIndex; index < count; ++index) {
        entries[index] = {};
    }
    count = writeIndex;

    for (EffectTimerState& state : g_effectTimers) {
        if (state.active && now - state.lastSeenTick > 1000) {
            state = {};
        }
    }
}

bool RefreshEffectCache(void* clientInstance) {
    const DWORD64 now = GetTickCount64();
    if (clientInstance != g_cachedEffectClientInstance) {
        g_cachedEffectClientInstance = clientInstance;
        g_cachedEffectCount = 0;
        g_effectTimers = {};
        g_lastEffectRefreshTick = 0;
        g_lastEffectNativeDrawTick = 0;
    }

    if (now - g_lastEffectRefreshTick < kEffectHudRefreshMs) {
        return true;
    }

    std::array<EffectHudEntry, kMaxEffectHudEntries> live{};
    int liveCount = 0;
    if (ReadLiveEffects(clientInstance, live, liveCount)) {
        ApplyEffectCountdown(live, liveCount);
        g_cachedEffects = live;
        g_cachedEffectCount = std::clamp(liveCount, 0, kMaxEffectHudEntries);
    } else {
        g_cachedEffectCount = 0;
        g_effectTimers = {};
    }
    g_lastEffectRefreshTick = now;
    return true;
}

bool ResolveNativeScreen(void* clientInstance, ImVec2& nativeScreen) {
    float width = g_cachedNativeWidth;
    float height = g_cachedNativeHeight;
    if (GetNativeHudScreenSize(clientInstance, width, height)) {
        g_cachedNativeWidth = width;
        g_cachedNativeHeight = height;
    }

    nativeScreen = ImVec2(std::max(1.0f, width), std::max(1.0f, height));
    return nativeScreen.x > 1.0f && nativeScreen.y > 1.0f;
}

const char* ToRomanLevel(int level) {
    switch (level) {
    case 1: return "I";
    case 2: return "II";
    case 3: return "III";
    case 4: return "IV";
    case 5: return "V";
    case 6: return "VI";
    case 7: return "VII";
    case 8: return "VIII";
    case 9: return "IX";
    case 10: return "X";
    default: return nullptr;
    }
}

void FormatDuration(int durationTicks, char* text, std::size_t textSize) {
    if (text == nullptr || textSize == 0) {
        return;
    }

    if (durationTicks < 0 || durationTicks > 20 * 60 * 60 * 24) {
        std::snprintf(text, textSize, "--:--");
        return;
    }

    const int totalSeconds = std::max(0, durationTicks / 20);
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;
    if (hours > 0) {
        std::snprintf(text, textSize, "%d:%02d:%02d", hours, minutes, seconds);
    } else {
        std::snprintf(text, textSize, "%d:%02d", minutes, seconds);
    }
}

void FormatEffectTitle(const EffectHudEntry& entry, char* text, std::size_t textSize) {
    if (text == nullptr || textSize == 0) {
        return;
    }

    const char* levelText = ToRomanLevel(entry.level);
    char levelFallback[16]{};
    if (levelText == nullptr) {
        std::snprintf(levelFallback, sizeof(levelFallback), "%d", entry.level);
        levelText = levelFallback;
    }

    const char* name = entry.info != nullptr ? entry.info->name : "Effect";
    if (entry.level > 1) {
        std::snprintf(text, textSize, "%s %s", name, levelText);
    } else {
        std::snprintf(text, textSize, "%s", name);
    }
}

void FormatEffectText(const EffectHudEntry& entry, char* text, std::size_t textSize) {
    if (text == nullptr || textSize == 0) {
        return;
    }

    char title[80]{};
    char duration[24]{};
    FormatEffectTitle(entry, title, sizeof(title));
    FormatDuration(entry.durationTicks, duration, sizeof(duration));
    std::snprintf(text, textSize, "%s %s", title, duration);
}

ImU32 EffectIconColor(const EffectHudEntry& entry, int alpha = 230) {
    const EffectInfo* info = entry.info;
    const int r = static_cast<int>(std::clamp(info != nullptr ? info->r : 0.65f, 0.0f, 1.0f) * 255.0f);
    const int g = static_cast<int>(std::clamp(info != nullptr ? info->g : 0.65f, 0.0f, 1.0f) * 255.0f);
    const int b = static_cast<int>(std::clamp(info != nullptr ? info->b : 0.65f, 0.0f, 1.0f) * 255.0f);
    return IM_COL32(r, g, b, alpha);
}

float EstimateNativeTextWidth(const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries, int count, float scale) {
    float maxWidth = 55.0f * scale;
    for (int index = 0; index < count; ++index) {
        char title[80]{};
        char duration[24]{};
        FormatEffectTitle(entries[index], title, sizeof(title));
        FormatDuration(entries[index].durationTicks, duration, sizeof(duration));
        maxWidth = std::max(maxWidth, static_cast<float>(std::strlen(title)) * kEffectHudEstimatedTextWidth * scale);
        maxWidth = std::max(maxWidth, static_cast<float>(std::strlen(duration)) * kEffectHudEstimatedTextWidth * scale);
    }
    return maxWidth;
}

ImVec2 GetEffectHudNativeRectSize(float scale, const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries, int count) {
    const int visibleCount = std::max(1, count);
    const float rowHeight = kEffectHudRowHeight * scale;
    const float iconSize = kEffectHudIconSize * scale;
    return ImVec2(
        kEffectHudPaddingX * 2.0f * scale + kEffectHudIconSize * scale + kEffectHudGap * scale + EstimateNativeTextWidth(entries, visibleCount, scale),
        kEffectHudPaddingY * 2.0f * scale + iconSize + rowHeight * static_cast<float>(visibleCount - 1));
}

ImVec2 ClampEffectHudNativePosition(
    const ImVec2& position,
    const ImVec2& nativeScreen,
    const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries,
    int count) {
    const ImVec2 rectSize = GetEffectHudNativeRectSize(GetClampedEffectHudScale(), entries, count);
    return ImVec2(
        std::clamp(position.x, 0.0f, std::max(0.0f, nativeScreen.x - rectSize.x)),
        std::clamp(position.y, 0.0f, std::max(0.0f, nativeScreen.y - rectSize.y)));
}

ImVec2 GetDefaultEffectHudNativePosition(
    const ImVec2& nativeScreen,
    const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries,
    int count) {
    const ImVec2 rectSize = GetEffectHudNativeRectSize(GetClampedEffectHudScale(), entries, count);
    return ImVec2(kDefaultEffectHudX, std::max(0.0f, (nativeScreen.y - rectSize.y) * 0.5f));
}

ImVec2 GetEffectiveEffectHudNativePosition(
    const ImVec2& nativeScreen,
    const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries,
    int count) {
    if (!g_effectHudCustomPosition.load(std::memory_order_relaxed)) {
        return ClampEffectHudNativePosition(GetDefaultEffectHudNativePosition(nativeScreen, entries, count), nativeScreen, entries, count);
    }

    return ClampEffectHudNativePosition(
        ImVec2(
            g_effectHudPositionX.load(std::memory_order_relaxed),
            g_effectHudPositionY.load(std::memory_order_relaxed)),
        nativeScreen,
        entries,
        count);
}

float GetEffectHudDisplayFontSize(float nativeScale, float displayScale) {
    const float nativeFontSize = kEffectHudNativeTextSize * nativeScale * displayScale;
    const float imguiFontSize = ImGui::GetCurrentContext() != nullptr
        ? ImGui::GetFontSize() * nativeScale
        : nativeFontSize;
    return std::clamp(std::max(nativeFontSize, imguiFontSize), 10.0f, 34.0f);
}

ImFont* GetGuiTextFont() {
    ImFont* font = tane::payload::GetItemHudFont();
    return font != nullptr ? font : ImGui::GetFont();
}

void UpdateOverlayMetrics(
    const ImVec2& nativePosition,
    const ImVec2& nativeScreen,
    const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries,
    int count) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 displaySize = io.DisplaySize;
    if (displaySize.x <= 0.0f || displaySize.y <= 0.0f || nativeScreen.x <= 0.0f || nativeScreen.y <= 0.0f) {
        return;
    }

    const float scale = GetClampedEffectHudScale();
    const int clampedCount = std::clamp(count, 0, kMaxEffectHudEntries);
    const bool backgroundEnabled = g_effectHudBackgroundEnabled.load(std::memory_order_relaxed);
    bool entriesUnchanged = g_lastOverlayMetricsCount == clampedCount;
    for (int index = 0; entriesUnchanged && index < clampedCount; ++index) {
        const EffectHudEntry& previous = g_lastOverlayMetricsEntries[index];
        const EffectHudEntry& current = entries[index];
        entriesUnchanged =
            previous.id == current.id &&
            previous.level == current.level &&
            previous.durationTicks == current.durationTicks &&
            previous.info == current.info;
    }
    if (entriesUnchanged &&
        g_lastOverlayMetricsScale == scale &&
        g_lastOverlayMetricsBackgroundEnabled == backgroundEnabled &&
        g_lastOverlayMetricsNativePosition.x == nativePosition.x &&
        g_lastOverlayMetricsNativePosition.y == nativePosition.y &&
        g_lastOverlayMetricsNativeScreen.x == nativeScreen.x &&
        g_lastOverlayMetricsNativeScreen.y == nativeScreen.y &&
        g_lastOverlayMetricsDisplaySize.x == displaySize.x &&
        g_lastOverlayMetricsDisplaySize.y == displaySize.y) {
        return;
    }

    const float scaleX = displaySize.x / nativeScreen.x;
    const float scaleY = displaySize.y / nativeScreen.y;
    const float displayScale = std::min(scaleX, scaleY);
    const float paddingX = kEffectHudPaddingX * scale * scaleX;
    const float paddingY = kEffectHudPaddingY * scale * scaleY;
    const float iconSize = kEffectHudIconSize * scale * displayScale;
    const float gap = kEffectHudGap * scale * scaleX;
    const float rowHeight = kEffectHudRowHeight * scale * scaleY;
    const float fontSize = GetEffectHudDisplayFontSize(scale, displayScale);
    const ImVec2 rectMin(nativePosition.x * scaleX, nativePosition.y * scaleY);
    const ImVec2 nativeRectSize = GetEffectHudNativeRectSize(scale, entries, count);
    g_overlayBackgroundMin = rectMin;
    g_overlayBackgroundMax = ImVec2(
        rectMin.x + nativeRectSize.x * scaleX,
        rectMin.y + nativeRectSize.y * scaleY);
    g_overlayBackgroundVisible = backgroundEnabled;

    g_overlayLineCount = clampedCount;
    for (int index = 0; index < g_overlayLineCount; ++index) {
        OverlayLine& line = g_overlayLines[index];
        FormatEffectTitle(entries[index], line.title, sizeof(line.title));
        FormatDuration(entries[index].durationTicks, line.duration, sizeof(line.duration));
        line.iconPath[0] = '\0';
        if (entries[index].info != nullptr && entries[index].info->iconPath != nullptr) {
            strncpy_s(line.iconPath, entries[index].info->iconPath, _TRUNCATE);
        }
        const float rowY = rectMin.y + paddingY + rowHeight * static_cast<float>(index);
        line.iconMin = ImVec2(rectMin.x + paddingX, rowY);
        line.iconMax = ImVec2(line.iconMin.x + iconSize, line.iconMin.y + iconSize);
        const float twoLineHeight = fontSize * 2.0f + kEffectHudLineSpacing * scale * displayScale;
        const float line1Offset = std::max(0.0f, (iconSize - twoLineHeight) * 0.5f);
        const float line2Offset = line1Offset + fontSize + kEffectHudLineSpacing * scale * displayScale;
        line.titlePos = ImVec2(line.iconMax.x + gap, rowY + line1Offset);
        line.durationPos = ImVec2(line.iconMax.x + gap, rowY + line2Offset);
        line.fontSize = fontSize;
        line.shadowOffset = std::max(1.0f, scale * displayScale);
        line.iconColor = EffectIconColor(entries[index]);
    }

    g_lastOverlayMetricsEntries = entries;
    g_lastOverlayMetricsNativePosition = nativePosition;
    g_lastOverlayMetricsNativeScreen = nativeScreen;
    g_lastOverlayMetricsDisplaySize = displaySize;
    g_lastOverlayMetricsCount = clampedCount;
    g_lastOverlayMetricsScale = scale;
    g_lastOverlayMetricsBackgroundEnabled = backgroundEnabled;
}

void DrawOverlayLines() {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    ImFont* font = GetGuiTextFont();
    if (g_overlayBackgroundVisible) {
        drawList->AddRectFilled(
            g_overlayBackgroundMin,
            g_overlayBackgroundMax,
            IM_COL32(0, 0, 0, 116),
            7.0f);
        drawList->AddRect(
            g_overlayBackgroundMin,
            g_overlayBackgroundMax,
            IM_COL32(255, 255, 255, 34),
            7.0f,
            0,
            1.0f);
    }

    for (int index = 0; index < g_overlayLineCount; ++index) {
        const OverlayLine& line = g_overlayLines[index];
        ImTextureRef iconTexture{};
        ImVec2 iconTextureSize{};
        if (GetEffectIconTexture(line.iconPath, iconTexture, iconTextureSize)) {
            drawList->AddImage(iconTexture, line.iconMin, line.iconMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 245));
        } else {
            drawList->AddRect(line.iconMin, line.iconMax, IM_COL32(255, 255, 255, 85), 2.0f, 0, 1.0f);
            const char iconLetter[2] = {line.title[0] != '\0' ? line.title[0] : 'E', '\0'};
            const ImVec2 letterSize = font != nullptr
                ? font->CalcTextSizeA(line.fontSize * 0.78f, FLT_MAX, 0.0f, iconLetter)
                : ImGui::CalcTextSize(iconLetter);
            const ImVec2 letterPos(
                line.iconMin.x + ((line.iconMax.x - line.iconMin.x) - letterSize.x) * 0.5f,
                line.iconMin.y + ((line.iconMax.y - line.iconMin.y) - letterSize.y) * 0.5f);
            DrawGuiTextWithShadow(
                drawList,
                font,
                line.fontSize * 0.78f,
                letterPos,
                IM_COL32(255, 255, 255, 235),
                iconLetter,
                line.shadowOffset,
                IM_COL32(0, 0, 0, 140));
        }
        DrawGuiTextWithShadow(
            drawList,
            font,
            line.fontSize,
            line.titlePos,
            IM_COL32(255, 255, 255, 238),
            line.title,
            line.shadowOffset,
            IM_COL32(0, 0, 0, 170));
        DrawGuiTextWithShadow(
            drawList,
            font,
            line.fontSize,
            line.durationPos,
            IM_COL32(170, 170, 170, 238),
            line.duration,
            line.shadowOffset,
            IM_COL32(0, 0, 0, 150));
    }
}

void DrawEffectHudLinesAt(
    ImDrawList* drawList,
    const ImVec2& min,
    const std::array<EffectHudEntry, kMaxEffectHudEntries>& entries,
    int count) {
    if (drawList == nullptr || ImGui::GetCurrentContext() == nullptr || count <= 0) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    const ImVec2 nativeScreen(
        g_cachedNativeWidth > 1.0f ? g_cachedNativeWidth : std::max(1.0f, io.DisplaySize.x),
        g_cachedNativeHeight > 1.0f ? g_cachedNativeHeight : std::max(1.0f, io.DisplaySize.y));
    const float scaleX = io.DisplaySize.x / std::max(1.0f, nativeScreen.x);
    const float scaleY = io.DisplaySize.y / std::max(1.0f, nativeScreen.y);
    const float displayScale = std::min(scaleX, scaleY);
    const float scale = GetClampedEffectHudScale();
    const float paddingX = kEffectHudPaddingX * scale * scaleX;
    const float paddingY = kEffectHudPaddingY * scale * scaleY;
    const float iconSize = kEffectHudIconSize * scale * displayScale;
    const float gap = kEffectHudGap * scale * scaleX;
    const float rowHeight = kEffectHudRowHeight * scale * scaleY;
    const float fontSize = GetEffectHudDisplayFontSize(scale, displayScale);
    const ImVec2 nativeSize = GetEffectHudNativeRectSize(scale, entries, count);
    const ImVec2 max(min.x + nativeSize.x * scaleX, min.y + nativeSize.y * scaleY);

    if (g_effectHudBackgroundEnabled.load(std::memory_order_relaxed)) {
        drawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, 116), 7.0f);
        drawList->AddRect(min, max, IM_COL32(255, 255, 255, 34), 7.0f, 0, 1.0f);
    }

    ImFont* font = GetGuiTextFont();
    const float shadowOffset = std::max(1.0f, scale * displayScale);
    const int clampedCount = std::clamp(count, 0, kMaxEffectHudEntries);
    for (int index = 0; index < clampedCount; ++index) {
        char title[80]{};
        char duration[24]{};
        FormatEffectTitle(entries[index], title, sizeof(title));
        FormatDuration(entries[index].durationTicks, duration, sizeof(duration));

        const float rowY = min.y + paddingY + rowHeight * static_cast<float>(index);
        const ImVec2 iconMin(min.x + paddingX, rowY);
        const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
        ImTextureRef iconTexture{};
        ImVec2 iconTextureSize{};
        const char* iconPath = entries[index].info != nullptr ? entries[index].info->iconPath : nullptr;
        if (iconPath != nullptr && GetEffectIconTexture(iconPath, iconTexture, iconTextureSize)) {
            drawList->AddImage(iconTexture, iconMin, iconMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 245));
        } else {
            drawList->AddRect(iconMin, iconMax, IM_COL32(255, 255, 255, 85), 2.0f, 0, 1.0f);
            const char iconLetter[2] = {title[0] != '\0' ? title[0] : 'E', '\0'};
            const ImVec2 letterSize = font != nullptr
                ? font->CalcTextSizeA(fontSize * 0.78f, FLT_MAX, 0.0f, iconLetter)
                : ImGui::CalcTextSize(iconLetter);
            const ImVec2 letterPos(
                iconMin.x + (iconSize - letterSize.x) * 0.5f,
                iconMin.y + (iconSize - letterSize.y) * 0.5f);
            DrawGuiTextWithShadow(
                drawList,
                font,
                fontSize * 0.78f,
                letterPos,
                IM_COL32(255, 255, 255, 235),
                iconLetter,
                shadowOffset,
                IM_COL32(0, 0, 0, 140));
        }

        const float twoLineHeight = fontSize * 2.0f + kEffectHudLineSpacing * scale * displayScale;
        const float line1Offset = std::max(0.0f, (iconSize - twoLineHeight) * 0.5f);
        const ImVec2 titlePos(iconMax.x + gap, rowY + line1Offset);
        const ImVec2 durationPos(iconMax.x + gap, titlePos.y + fontSize + kEffectHudLineSpacing * scale * displayScale);
        DrawGuiTextWithShadow(
            drawList,
            font,
            fontSize,
            titlePos,
            IM_COL32(255, 255, 255, 238),
            title,
            shadowOffset,
            IM_COL32(0, 0, 0, 170));
        DrawGuiTextWithShadow(
            drawList,
            font,
            fontSize,
            durationPos,
            IM_COL32(170, 170, 170, 238),
            duration,
            shadowOffset,
            IM_COL32(0, 0, 0, 150));
    }
}

void __fastcall HookMobEffectsRendererRender(void* self, void* renderContext, void* a3, void* a4) {
    EnsureEffectHudConfigLoaded();
    if (g_effectHudEnabled.load(std::memory_order_relaxed) || IsGuiPositionEditorActive()) {
        return;
    }

    if (g_originalMobEffectsRendererRender != nullptr) {
        g_originalMobEffectsRendererRender(self, renderContext, a3, a4);
    }
}

} // namespace
