bool HasTexture(ImTextureRef texture) {
    return texture._TexData != nullptr || texture._TexID != ImTextureID_Invalid;
}

bool TryGetFileSize(const wchar_t* path, unsigned long long& size) {
    size = 0;
    if (path == nullptr || path[0] == L'\0') {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data) ||
        (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }

    size = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    return true;
}

bool FileExists(const wchar_t* path) {
    unsigned long long size = 0;
    return TryGetFileSize(path, size) && size > 0;
}

bool DirectoryExists(const wchar_t* path) {
    if (path == nullptr || path[0] == L'\0') {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool BuildEffectHudTempIconDirectory(wchar_t* outDir, DWORD outDirCount) {
    if (outDir == nullptr || outDirCount == 0) {
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

    wchar_t imagesPath[MAX_PATH]{};
    if (swprintf_s(imagesPath, L"%s\\images", basePath) < 0) {
        return false;
    }
    CreateDirectoryW(imagesPath, nullptr);

    if (swprintf_s(outDir, outDirCount, L"%s\\effect", imagesPath) < 0) {
        return false;
    }
    CreateDirectoryW(outDir, nullptr);
    return DirectoryExists(outDir);
}

void NormalizeEffectIconFileName(const char* iconPath, wchar_t* fileName, DWORD fileNameCount) {
    if (fileName == nullptr || fileNameCount == 0) {
        return;
    }
    fileName[0] = L'\0';
    if (iconPath == nullptr || iconPath[0] == '\0') {
        return;
    }

    wchar_t widePath[MAX_PATH]{};
    if (MultiByteToWideChar(CP_UTF8, 0, iconPath, -1, widePath, MAX_PATH) <= 0) {
        return;
    }

    for (wchar_t* cursor = widePath; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'/') {
            *cursor = L'\\';
        }
    }

    const wchar_t* baseName = wcsrchr(widePath, L'\\');
    baseName = baseName != nullptr ? baseName + 1 : widePath;
    const bool hasPng = wcslen(baseName) >= 4 && _wcsicmp(baseName + wcslen(baseName) - 4, L".png") == 0;
    if (hasPng) {
        wcsncpy_s(fileName, fileNameCount, baseName, _TRUNCATE);
    } else {
        swprintf_s(fileName, fileNameCount, L"%s.png", baseName);
    }
}

bool TryBuildPath(const wchar_t* root, const wchar_t* fileName, wchar_t* outPath, DWORD outPathCount) {
    if (root == nullptr || root[0] == L'\0' || fileName == nullptr || fileName[0] == L'\0') {
        return false;
    }
    if (swprintf_s(outPath, outPathCount, L"%s\\%s", root, fileName) < 0) {
        return false;
    }
    return FileExists(outPath);
}

HMODULE GetThisModuleHandle() {
    if (g_effectHudPayloadModule != nullptr) {
        return g_effectHudPayloadModule;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(&GetThisModuleHandle), &info, sizeof(info)) != 0 &&
        info.AllocationBase != nullptr) {
        return static_cast<HMODULE>(info.AllocationBase);
    }

    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetThisModuleHandle),
            &module)) {
        return nullptr;
    }
    return module;
}

bool WriteResourceToFileIfMissing(HMODULE module, const EmbeddedEffectIcon& icon, const wchar_t* targetRoot) {
    if (module == nullptr || icon.fileName == nullptr || targetRoot == nullptr || targetRoot[0] == L'\0') {
        return false;
    }

    wchar_t targetPath[MAX_PATH]{};
    if (swprintf_s(targetPath, L"%s\\%s", targetRoot, icon.fileName) < 0) {
        return false;
    }

    unsigned long long existingSize = 0;
    if (TryGetFileSize(targetPath, existingSize) && existingSize > 0) {
        return true;
    }

    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(icon.resourceId), RT_RCDATA);
    if (resource == nullptr) {
        return false;
    }

    const DWORD size = SizeofResource(module, resource);
    HGLOBAL loaded = LoadResource(module, resource);
    void* bytes = loaded != nullptr ? LockResource(loaded) : nullptr;
    if (size == 0 || bytes == nullptr) {
        return false;
    }

    HANDLE file = CreateFileW(targetPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesWritten = 0;
    const bool ok = WriteFile(file, bytes, size, &bytesWritten, nullptr) != FALSE && bytesWritten == size;
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(targetPath);
        return false;
    }

    return true;
}

bool ExtractEmbeddedEffectImagesToTemp() {
    HMODULE module = GetThisModuleHandle();
    if (module == nullptr) {
        return false;
    }

    wchar_t targetRoot[MAX_PATH]{};
    if (!BuildEffectHudTempIconDirectory(targetRoot, MAX_PATH)) {
        return false;
    }

    int failed = 0;
    for (const EmbeddedEffectIcon& icon : kEmbeddedEffectIcons) {
        if (!WriteResourceToFileIfMissing(module, icon, targetRoot)) {
            ++failed;
        }
    }

    return failed == 0;
}

void ResetEffectIconCachesForReextract() {
    g_iconPathCache = {};
    g_effectHudIconPreloadIndex.store(0, std::memory_order_relaxed);
    g_effectHudIconPreloadComplete.store(false, std::memory_order_release);
}

bool EnsureEffectHudImagesInjected() {
    if (g_effectHudImagesInjected.load(std::memory_order_acquire)) {
        return g_effectHudImagesInjectSucceeded.load(std::memory_order_relaxed);
    }

    const bool extracted = ExtractEmbeddedEffectImagesToTemp();
    ResetEffectIconCachesForReextract();
    g_effectHudImagesInjectSucceeded.store(extracted, std::memory_order_relaxed);
    g_effectHudImagesInjected.store(true, std::memory_order_release);
    return extracted;
}

bool ResolveEffectIconFile(const char* iconPath, wchar_t* outPath, DWORD outPathCount) {
    if (outPath == nullptr || outPathCount == 0) {
        return false;
    }
    outPath[0] = L'\0';

    wchar_t fileName[MAX_PATH]{};
    NormalizeEffectIconFileName(iconPath, fileName, MAX_PATH);
    if (fileName[0] == L'\0') {
        return false;
    }

    EnsureEffectHudImagesInjected();
    wchar_t tempRoot[MAX_PATH]{};
    if (BuildEffectHudTempIconDirectory(tempRoot, MAX_PATH) &&
        TryBuildPath(tempRoot, fileName, outPath, outPathCount)) {
        return true;
    }

    g_effectHudImagesInjected.store(false, std::memory_order_release);
    if (EnsureEffectHudImagesInjected() &&
        BuildEffectHudTempIconDirectory(tempRoot, MAX_PATH) &&
        TryBuildPath(tempRoot, fileName, outPath, outPathCount)) {
        return true;
    }

    return false;
}

bool GetEffectIconTexture(const char* iconPath, ImTextureRef& texture, ImVec2& size) {
    texture = ImTextureRef();
    size = ImVec2(0.0f, 0.0f);
    if (iconPath == nullptr || iconPath[0] == '\0') {
        return false;
    }

    IconPathCache* freeSlot = nullptr;
    IconPathCache* cache = nullptr;
    for (IconPathCache& candidate : g_iconPathCache) {
        if (candidate.iconPath[0] != '\0' && std::strcmp(candidate.iconPath, iconPath) == 0) {
            cache = &candidate;
            break;
        }
        if (candidate.iconPath[0] == '\0' && freeSlot == nullptr) {
            freeSlot = &candidate;
        }
    }

    if (cache == nullptr) {
        cache = freeSlot != nullptr ? freeSlot : &g_iconPathCache[0];
        *cache = {};
        strncpy_s(cache->iconPath, iconPath, _TRUNCATE);
    }

    const DWORD64 now = GetTickCount64();
    if (cache->textureReady && HasTexture(cache->texture)) {
        texture = cache->texture;
        size = cache->textureSize;
        return true;
    }

    if (cache->found &&
        (cache->lastFileCheckTick == 0 || now - cache->lastFileCheckTick >= kEffectIconFileRecheckMs)) {
        cache->lastFileCheckTick = now;
        if (!FileExists(cache->filePath)) {
            cache->textureReady = false;
            cache->texture = ImTextureRef();
            cache->textureSize = ImVec2(0.0f, 0.0f);
            cache->lastTextureAttemptTick = 0;
            cache->attempted = false;
            cache->found = false;
            cache->filePath[0] = L'\0';
            g_effectHudImagesInjected.store(false, std::memory_order_release);
        }
    }

    if (cache->found && !cache->textureReady &&
        cache->lastTextureAttemptTick != 0 &&
        now - cache->lastTextureAttemptTick < kEffectIconTextureRetryMs) {
        return false;
    }

    if (cache->found && cache->filePath[0] == L'\0') {
        cache->attempted = false;
        cache->found = false;
        cache->textureReady = false;
        g_effectHudImagesInjected.store(false, std::memory_order_release);
    }

    if (!cache->attempted || !cache->found) {
        wchar_t resolvedPath[MAX_PATH]{};
        const bool resolved = ResolveEffectIconFile(iconPath, resolvedPath, MAX_PATH);

        cache->attempted = true;
        cache->found = resolved;
        cache->lastFileCheckTick = now;
        if (resolved) {
            wcsncpy_s(cache->filePath, resolvedPath, _TRUNCATE);
        } else {
            cache->filePath[0] = L'\0';
        }
    }

    if (!cache->found) {
        return false;
    }

    cache->lastTextureAttemptTick = now;
    if (tane::payload::GetPngTextureFromFile(cache->filePath, texture, size) && HasTexture(texture)) {
        cache->texture = texture;
        cache->textureSize = size;
        cache->textureReady = true;
        return true;
    }
    return false;
}

void PreloadEffectHudIconTextures(int budget) {
    if (budget <= 0 ||
        ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    if (g_effectHudIconPreloadComplete.load(std::memory_order_acquire) ||
        !EnsureEffectHudImagesInjected()) {
        return;
    }

    wchar_t tempRoot[MAX_PATH]{};
    if (!BuildEffectHudTempIconDirectory(tempRoot, MAX_PATH)) {
        return;
    }

    int index = g_effectHudIconPreloadIndex.load(std::memory_order_relaxed);
    const int iconCount = static_cast<int>(sizeof(kEmbeddedEffectIcons) / sizeof(kEmbeddedEffectIcons[0]));
    int processed = 0;
    while (index < iconCount && processed < budget) {
        const EmbeddedEffectIcon& icon = kEmbeddedEffectIcons[index];
        wchar_t path[MAX_PATH]{};
        if (icon.fileName != nullptr && swprintf_s(path, L"%s\\%s", tempRoot, icon.fileName) >= 0 && FileExists(path)) {
            ImTextureRef texture;
            ImVec2 size;
            tane::payload::GetPngTextureFromFile(path, texture, size);
        }
        ++index;
        ++processed;
    }

    g_effectHudIconPreloadIndex.store(index, std::memory_order_relaxed);
    if (index >= iconCount) {
        g_effectHudIconPreloadComplete.store(true, std::memory_order_release);
    }
}

bool BuildEffectHudConfigPath(wchar_t* path, DWORD pathCount) {
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

    return swprintf_s(path, pathCount, L"%s\\EffectHUD.json", guiPath) >= 0;
}

bool ParseFloatAfter(const char* section, const char* key, float& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    return found != nullptr && std::sscanf(found + 1, " %f", &value) == 1;
}

bool ParseIntAfter(const char* section, const char* key, int& value) {
    const char* found = section != nullptr ? std::strstr(section, key) : nullptr;
    if (found == nullptr) {
        return false;
    }

    found = std::strchr(found, ':');
    return found != nullptr && std::sscanf(found + 1, " %d", &value) == 1;
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

float GetClampedEffectHudScale() {
    return std::clamp(
        g_effectHudScale.load(std::memory_order_relaxed),
        kMinEffectHudScale,
        kMaxEffectHudScale);
}

void SaveEffectHudConfig() {
    wchar_t path[MAX_PATH]{};
    if (!BuildEffectHudConfigPath(path, MAX_PATH)) {
        return;
    }

    char json[384]{};
    std::snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"version\": %d,\n"
        "  \"enabled\": %s,\n"
        "  \"background\": %s,\n"
        "  \"custom\": %s,\n"
        "  \"x\": %.3f,\n"
        "  \"y\": %.3f,\n"
        "  \"scale\": %.3f\n"
        "}\n",
        kEffectHudConfigVersion,
        g_effectHudEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_effectHudBackgroundEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_effectHudCustomPosition.load(std::memory_order_relaxed) ? "true" : "false",
        g_effectHudPositionX.load(std::memory_order_relaxed),
        g_effectHudPositionY.load(std::memory_order_relaxed),
        g_effectHudScale.load(std::memory_order_relaxed));

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, json, static_cast<DWORD>(std::strlen(json)), &written, nullptr);
    CloseHandle(file);
    g_effectHudPositionDirty.store(false, std::memory_order_relaxed);
}

void EnsureEffectHudConfigLoaded() {
    bool expected = false;
    if (!g_effectHudConfigLoaded.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!BuildEffectHudConfigPath(path, MAX_PATH)) {
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
        int version = 0;
        bool enabled = false;
        bool background = true;
        bool custom = false;
        float x = kDefaultEffectHudX;
        float y = kDefaultEffectHudY;
        float scale = kDefaultEffectHudScale;

        ParseIntAfter(json, "\"version\"", version);
        ParseBoolAfter(json, "\"enabled\"", enabled);
        ParseBoolAfter(json, "\"background\"", background);
        ParseBoolAfter(json, "\"custom\"", custom);
        ParseFloatAfter(json, "\"x\"", x);
        ParseFloatAfter(json, "\"y\"", y);
        ParseFloatAfter(json, "\"scale\"", scale);
        if (version < kEffectHudConfigVersion) {
            custom = false;
            x = kDefaultEffectHudX;
            y = kDefaultEffectHudY;
            scale = kDefaultEffectHudScale;
        }

        g_effectHudEnabled.store(enabled, std::memory_order_relaxed);
        g_effectHudBackgroundEnabled.store(background, std::memory_order_relaxed);
        g_effectHudCustomPosition.store(custom, std::memory_order_relaxed);
        g_effectHudPositionX.store(std::max(0.0f, x), std::memory_order_relaxed);
        g_effectHudPositionY.store(std::max(0.0f, y), std::memory_order_relaxed);
        g_effectHudScale.store(std::clamp(scale, kMinEffectHudScale, kMaxEffectHudScale), std::memory_order_relaxed);
    }
    CloseHandle(file);
}
