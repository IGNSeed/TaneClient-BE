bool IsReasonableName(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        return false;
    }
    for (unsigned char c : name) {
        if (c < 0x20 && c != '\t') {
            return false;
        }
    }
    return true;
}

std::string StripMinecraftFormatting(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if (c == 0xC2 && i + 2 < input.size() && static_cast<unsigned char>(input[i + 1]) == 0xA7) {
            i += 2;
            continue;
        }
        if (input[i] == '\xA7' && i + 1 < input.size()) {
            ++i;
            continue;
        }
        out.push_back(input[i]);
    }
    return out;
}

bool ApplyCachedSkinHead(const MceUuid& uuid, TabPlayerEntry& out) {
    auto cached = g_cachedSkinHeads.find(uuid);
    if (cached == g_cachedSkinHeads.end() || cached->second.pixels.empty()) {
        return false;
    }

    out.textureKey = cached->second.textureKey;
    out.cachedHead = &cached->second;
    out.headWidth = 64;
    out.headHeight = 64;
    out.hasHeadPixels = true;
    return true;
}

std::uint64_t HashBytes(const unsigned char* bytes, std::size_t size) {
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<std::uint64_t>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

bool ApplyCachedSkinHeadAfterFailure(
    const PlayerListEntryCompat& entry,
    TabPlayerEntry& out,
    const char*,
    PlayerSkinPrefix*,
    const SkinImage*) {
    return ApplyCachedSkinHead(entry.uuid, out);
}

bool CopySkinHeadPixels(
    const SkinImage& image,
    int headX,
    int headY,
    int overlayX,
    int overlayY,
    int headSize,
    std::vector<unsigned char>& output) {
    constexpr int kOutputSize = 64;
    output.assign(kOutputSize * kOutputSize * 4, 0);
    __try {
        for (int y = 0; y < kOutputSize; ++y) {
            const int sourceY = headY + (y * headSize) / kOutputSize;
            for (int x = 0; x < kOutputSize; ++x) {
                const int sourceX = headX + (x * headSize) / kOutputSize;
                const std::size_t source = (static_cast<std::size_t>(sourceY) * image.width + sourceX) * 4u;
                const std::size_t destination = (static_cast<std::size_t>(y) * kOutputSize + x) * 4u;
                output[destination + 0] = image.bytes.data[source + 0];
                output[destination + 1] = image.bytes.data[source + 1];
                output[destination + 2] = image.bytes.data[source + 2];
                output[destination + 3] = image.bytes.data[source + 3];
            }
        }
        for (int y = 0; y < kOutputSize; ++y) {
            const int sourceY = overlayY + (y * headSize) / kOutputSize;
            for (int x = 0; x < kOutputSize; ++x) {
                const int sourceX = overlayX + (x * headSize) / kOutputSize;
                const std::size_t source = (static_cast<std::size_t>(sourceY) * image.width + sourceX) * 4u;
                const std::size_t destination = (static_cast<std::size_t>(y) * kOutputSize + x) * 4u;
                const unsigned alpha = image.bytes.data[source + 3];
                if (alpha == 0) {
                    continue;
                }
                const unsigned inverse = 255u - alpha;
                output[destination + 0] = static_cast<unsigned char>((image.bytes.data[source + 0] * alpha + output[destination + 0] * inverse + 127u) / 255u);
                output[destination + 1] = static_cast<unsigned char>((image.bytes.data[source + 1] * alpha + output[destination + 1] * inverse + 127u) / 255u);
                output[destination + 2] = static_cast<unsigned char>((image.bytes.data[source + 2] * alpha + output[destination + 2] * inverse + 127u) / 255u);
                output[destination + 3] = static_cast<unsigned char>(std::max<unsigned>(output[destination + 3], alpha));
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        output.clear();
        return false;
    }
}

bool CopySkinHead(const PlayerListEntryCompat& entry, TabPlayerEntry& out) {
    PlayerSkinPrefix* skin = entry.skin.ptr;
    if (!IsReadableAddress(skin, sizeof(PlayerSkinPrefix))) {
        return ApplyCachedSkinHeadAfterFailure(entry, out, "skin_unreadable", skin, nullptr);
    }

    SkinImage image{};
    if (!ReadOffset(skin, kPlayerSkinSkinImageOffset, image)) {
        return ApplyCachedSkinHeadAfterFailure(entry, out, "image_read_failed", skin, nullptr);
    }

    const bool rgbaFormat = image.imageFormat == 3 || image.imageFormat == 4;
    if (!rgbaFormat ||
        image.width < 64 ||
        image.height < 32 ||
        image.width > 512 ||
        image.height > 512 ||
        image.bytes.data == nullptr) {
        return ApplyCachedSkinHeadAfterFailure(entry, out, "bad_header", skin, &image);
    }

    const std::size_t requiredBytes = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4u;
    if (image.bytes.size < requiredBytes || image.bytes.size > 512u * 512u * 4u ||
        !IsReadableAddress(image.bytes.data, requiredBytes)) {
        return ApplyCachedSkinHeadAfterFailure(entry, out, "blob_unreadable", skin, &image);
    }

    const auto cached = g_cachedSkinHeads.find(entry.uuid);
    if (cached != g_cachedSkinHeads.end() &&
        cached->second.data == image.bytes.data &&
        cached->second.size == image.bytes.size &&
        cached->second.width == image.width &&
        cached->second.height == image.height &&
        !cached->second.pixels.empty()) {
        return ApplyCachedSkinHead(entry.uuid, out);
    }

    const int headSize = static_cast<int>(std::max<std::uint32_t>(8, image.width / 8));
    const int headX = static_cast<int>(image.width / 8);
    const int headY = headX;
    const int overlayX = static_cast<int>((image.width * 5u) / 8u);
    const int overlayY = headY;
    if (headX + headSize > static_cast<int>(image.width) ||
        headY + headSize > static_cast<int>(image.height) ||
        overlayX + headSize > static_cast<int>(image.width) ||
        overlayY + headSize > static_cast<int>(image.height)) {
        return ApplyCachedSkinHeadAfterFailure(entry, out, "crop_oob", skin, &image);
    }

    constexpr int kOutputSize = 64;
    if (!CopySkinHeadPixels(image, headX, headY, overlayX, overlayY, headSize, out.headPixels)) {
        return ApplyCachedSkinHeadAfterFailure(entry, out, "copy_exception", skin, &image);
    }

    const std::uint64_t headHash = HashBytes(out.headPixels.data(), out.headPixels.size());
    char key[192]{};
    std::snprintf(
        key,
        sizeof(key),
        "tab:%016llx%016llx:%016llx:64x64",
        static_cast<unsigned long long>(entry.uuid.mostSig),
        static_cast<unsigned long long>(entry.uuid.leastSig),
        static_cast<unsigned long long>(headHash));
    out.textureKey = key;
    out.headWidth = kOutputSize;
    out.headHeight = kOutputSize;
    out.hasHeadPixels = true;

    CachedSkinHead& cachedHead = g_cachedSkinHeads[entry.uuid];
    cachedHead.data = image.bytes.data;
    cachedHead.size = image.bytes.size;
    cachedHead.width = image.width;
    cachedHead.height = image.height;
    cachedHead.textureKey = out.textureKey;
    cachedHead.pixels = std::move(out.headPixels);
    out.cachedHead = &cachedHead;
    return true;
}

bool TryGetPlayerMapSize(PlayerMap* playerMap, std::size_t& size) {
    size = 0;
    __try {
        size = playerMap != nullptr ? playerMap->size() : 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        size = 0;
        return false;
    }
}

bool RefreshPlayerCache(void* clientInstance, bool force) {
    const ULONGLONG now = GetTickCount64();
    if (!force &&
        clientInstance == g_cachedPlayerClientInstance &&
        g_lastPlayerRefreshTick != 0 &&
        now - g_lastPlayerRefreshTick < kPlayerRefreshMs) {
        return !g_cachedPlayers.empty();
    }

    void* previousClientInstance = g_cachedPlayerClientInstance;
    const bool clientChanged = clientInstance != previousClientInstance;
    g_cachedPlayerClientInstance = clientInstance;
    g_lastPlayerRefreshTick = now;
    if (clientChanged) {
        g_cachedPlayers.clear();
        g_cachedSkinHeads.clear();
    }

    void* localPlayer = GetLocalPlayer(clientInstance);
    void* level = GetLevelFromLocalPlayer(localPlayer);
    PlayerMap* playerMap = GetPlayerMap(level);
    if (playerMap == nullptr) {
        return !g_cachedPlayers.empty();
    }

    std::size_t playerMapSize = 0;
    if (!TryGetPlayerMapSize(playerMap, playerMapSize) || playerMapSize == 0 || playerMapSize > kMaxPlayers) {
        return !g_cachedPlayers.empty();
    }

    std::vector<TabPlayerEntry> nextPlayers;
    nextPlayers.reserve(std::min<std::size_t>(playerMapSize, kMaxDisplayedPlayers));
    g_cachedSkinHeads.reserve(std::min<std::size_t>(playerMapSize, kMaxDisplayedPlayers));
    for (const auto& pair : *playerMap) {
        if (nextPlayers.size() >= kMaxDisplayedPlayers) {
            break;
        }

        TabPlayerEntry entry{};
        entry.name = StripMinecraftFormatting(pair.second.name);
        if (!IsReasonableName(entry.name)) {
            continue;
        }
        CopySkinHead(pair.second, entry);
        nextPlayers.push_back(std::move(entry));
    }

    std::sort(nextPlayers.begin(), nextPlayers.end(), [](const TabPlayerEntry& a, const TabPlayerEntry& b) {
        return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    g_cachedPlayers = std::move(nextPlayers);
    return !g_cachedPlayers.empty();
}

ImFont* GetTabFont() {
    ImFont* font = tane::payload::GetItemHudFont();
    return font != nullptr ? font : ImGui::GetFont();
}

ImVec2 CalcText(ImFont* font, float size, const char* text) {
    if (font != nullptr) {
        return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    }
    return ImGui::CalcTextSize(text);
}

std::string FitTextToWidth(ImFont* font, float fontSize, const std::string& text, float maxWidth) {
    if (text.empty() || maxWidth <= 0.0f || CalcText(font, fontSize, text.c_str()).x <= maxWidth) {
        return text;
    }

    constexpr const char* kEllipsis = "...";
    const float ellipsisWidth = CalcText(font, fontSize, kEllipsis).x;
    if (ellipsisWidth >= maxWidth) {
        return std::string();
    }

    std::size_t low = 0;
    std::size_t high = text.size();
    while (low < high) {
        std::size_t mid = (low + high + 1) / 2;
        while (mid > 0 && mid < text.size() && (static_cast<unsigned char>(text[mid]) & 0xC0) == 0x80) {
            --mid;
        }
        if (mid == low && low + 1 < high) {
            mid = low + 1;
        }

        const std::string candidate = text.substr(0, mid);
        if (CalcText(font, fontSize, candidate.c_str()).x + ellipsisWidth <= maxWidth) {
            low = mid;
        } else {
            high = mid > 0 ? mid - 1 : 0;
        }
    }

    while (low > 0 && low < text.size() && (static_cast<unsigned char>(text[low]) & 0xC0) == 0x80) {
        --low;
    }

    if (low == 0) {
        return kEllipsis;
    }
    std::string result = text.substr(0, low);
    result += kEllipsis;
    return result;
}

void DrawFallbackHead(ImDrawList* drawList, ImFont* font, const ImVec2& min, float size, const std::string& name) {
    const ImVec2 max(min.x + size, min.y + size);
    const unsigned seed = name.empty() ? 0u : static_cast<unsigned>(name[0]) * 37u + static_cast<unsigned>(name.size()) * 17u;
    const ImU32 fill = IM_COL32(42 + (seed % 58), 58 + ((seed / 3) % 64), 72 + ((seed / 7) % 70), 245);
    drawList->AddRectFilled(min, max, fill, 4.0f);
    drawList->AddRect(min, max, IM_COL32(255, 255, 255, 55), 4.0f, 0, 1.0f);

    char letter[2] = {name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(name[0]))), '\0'};
    const float fontSize = size * 0.52f;
    const ImVec2 textSize = CalcText(font, fontSize, letter);
    DrawGuiTextWithShadow(
        drawList,
        font,
        fontSize,
        ImVec2(min.x + (size - textSize.x) * 0.5f, min.y + (size - textSize.y) * 0.46f),
        IM_COL32(255, 255, 255, 235),
        letter,
        1.0f,
        IM_COL32(0, 0, 0, 130));
}

void DrawPlayerHead(ImDrawList* drawList, const TabPlayerEntry& entry, ImFont* font, const ImVec2& min, float size) {
    const std::string* textureKey = &entry.textureKey;
    const std::vector<unsigned char>* pixels = &entry.headPixels;
    int headWidth = entry.headWidth;
    int headHeight = entry.headHeight;
    if (entry.cachedHead != nullptr) {
        textureKey = &entry.cachedHead->textureKey;
        pixels = &entry.cachedHead->pixels;
        headWidth = 64;
        headHeight = 64;
    }

    if (entry.hasHeadPixels && textureKey != nullptr && !textureKey->empty() && pixels != nullptr && !pixels->empty()) {
        ImTextureRef texture{};
        ImVec2 textureSize{};
        if (tane::payload::GetRgbaTextureFromMemory(
                textureKey->c_str(),
                pixels->data(),
                headWidth,
                headHeight,
                texture,
                textureSize)) {
            drawList->AddImage(texture, min, ImVec2(min.x + size, min.y + size));
            drawList->AddRect(min, ImVec2(min.x + size, min.y + size), IM_COL32(255, 255, 255, 44), 4.0f, 0, 1.0f);
            return;
        }
    }
    DrawFallbackHead(drawList, font, min, size, entry.name);
}

} // namespace
