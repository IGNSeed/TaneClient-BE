bool IsEffectHudIconPath(const wchar_t* path) {
    return path != nullptr &&
        (wcsstr(path, L"\\TaneClient\\images\\effect\\") != nullptr ||
            wcsstr(path, L"\\TaneClient\\Images\\effect\\") != nullptr ||
            wcsstr(path, L"\\TaneClient\\Images\\Effect\\") != nullptr);
}

void UpscalePixelsNearest(std::vector<std::uint8_t>& pixels, UINT& width, UINT& height, UINT factor) {
    if (factor <= 1 || pixels.empty() || width == 0 || height == 0) {
        return;
    }

    const UINT nextWidth = width * factor;
    const UINT nextHeight = height * factor;
    std::vector<std::uint8_t> upscaled(static_cast<std::size_t>(nextWidth) * static_cast<std::size_t>(nextHeight) * 4);
    for (UINT y = 0; y < nextHeight; ++y) {
        const UINT sourceY = y / factor;
        for (UINT x = 0; x < nextWidth; ++x) {
            const UINT sourceX = x / factor;
            const std::size_t src = (static_cast<std::size_t>(sourceY) * width + sourceX) * 4;
            const std::size_t dst = (static_cast<std::size_t>(y) * nextWidth + x) * 4;
            upscaled[dst + 0] = pixels[src + 0];
            upscaled[dst + 1] = pixels[src + 1];
            upscaled[dst + 2] = pixels[src + 2];
            upscaled[dst + 3] = pixels[src + 3];
        }
    }
    pixels.swap(upscaled);
    width = nextWidth;
    height = nextHeight;
}

bool CreateExternalPngTextureFromFile(int index, const wchar_t* path) {
    std::vector<std::uint8_t> pixels;
    UINT width = 0;
    UINT height = 0;
    if (!DecodePngFile(path, pixels, width, height)) {
        return false;
    }
    if (IsEffectHudIconPath(path) && width <= 24 && height <= 24) {
        UpscalePixelsNearest(pixels, width, height, 4);
    }

    if (g_backend == RenderBackend::D3D11) {
        return CreateExternalPngD3D11Texture(index, pixels, width, height);
    }
    if (g_backend == RenderBackend::D3D12) {
        return CreateExternalPngD3D12Texture(index, pixels, width, height);
    }

    return false;
}

int FindExternalPngTextureSlot(const wchar_t* path) {
    int freeIndex = -1;
    for (int index = 0; index < kExternalPngTextureCount; ++index) {
        ExternalPngTexture& texture = g_externalPngTextures[index];
        if (texture.path[0] != L'\0' && _wcsicmp(texture.path, path) == 0) {
            return index;
        }
        if (freeIndex < 0 && texture.path[0] == L'\0') {
            freeIndex = index;
        }
    }
    return freeIndex;
}

bool EnsureExternalPngTexture(const wchar_t* path, int& index) {
    index = -1;
    if (path == nullptr || path[0] == L'\0') {
        return false;
    }

    index = FindExternalPngTextureSlot(path);
    if (index < 0) {
        return false;
    }

    ExternalPngTexture& texture = g_externalPngTextures[index];
    if (texture.path[0] == L'\0') {
        wcsncpy_s(texture.path, path, _TRUNCATE);
    }

    if (texture.d3d11View != nullptr || texture.d3d12Texture != nullptr) {
        return true;
    }
    if (texture.attempted) {
        return false;
    }

    texture.attempted = true;
    return CreateExternalPngTextureFromFile(index, texture.path);
}

bool CreateMenuLogoTextureFromResource() {
    std::vector<std::uint8_t> pixels;
    UINT width = 0;
    UINT height = 0;
    if (!DecodeMenuLogoPng(pixels, width, height)) {
        return false;
    }

    if (g_backend == RenderBackend::D3D11) {
        return CreateMenuLogoD3D11Texture(pixels, width, height);
    }
    if (g_backend == RenderBackend::D3D12) {
        return CreateMenuLogoD3D12Texture(pixels, width, height);
    }

    return false;
}

bool EnsureMenuLogoTexture() {
    if (g_menuLogoTextureView != nullptr || g_menuLogoDx12Texture != nullptr) {
        return true;
    }
    if (g_menuLogoTextureAttempted) {
        return false;
    }

    g_menuLogoTextureAttempted = true;
    return CreateMenuLogoTextureFromResource();
}

bool GenerateItemHudPreviewPixels(int index, std::vector<std::uint8_t>& pixels, UINT& width, UINT& height) {
    pixels.clear();
    width = 64;
    height = 64;
    if (index < 0 || index >= kItemHudPreviewTextureCount) {
        return false;
    }

    pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
    auto putPixel = [&](int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
        if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) {
            return;
        }
        const std::size_t offset = (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;
        pixels[offset + 0] = r;
        pixels[offset + 1] = g;
        pixels[offset + 2] = b;
        pixels[offset + 3] = a;
    };
    auto fillRect = [&](int left, int top, int right, int bottom, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                putPixel(x, y, r, g, b, a);
            }
        }
    };
    auto fillDiamond = [&](int centerX, int centerY, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (std::abs(x) + std::abs(y) <= radius) {
                    putPixel(centerX + x, centerY + y, r, g, b, a);
                }
            }
        }
    };

    fillRect(6, 6, 58, 58, 0, 0, 0, 72);
    fillRect(8, 8, 56, 56, 255, 255, 255, 22);
    constexpr std::uint8_t dr = 60;
    constexpr std::uint8_t dg = 214;
    constexpr std::uint8_t db = 224;
    constexpr std::uint8_t sr = 224;
    constexpr std::uint8_t sg = 252;
    constexpr std::uint8_t sb = 255;
    constexpr std::uint8_t or_ = 20;
    constexpr std::uint8_t og = 118;
    constexpr std::uint8_t ob = 132;

    switch (index) {
    case 0:
        fillRect(18, 18, 46, 28, dr, dg, db, 255);
        fillRect(14, 28, 22, 42, dr, dg, db, 255);
        fillRect(42, 28, 50, 42, dr, dg, db, 255);
        fillRect(22, 26, 42, 34, sr, sg, sb, 210);
        fillRect(18, 42, 46, 48, or_, og, ob, 255);
        break;
    case 1:
        fillRect(20, 15, 44, 50, dr, dg, db, 255);
        fillRect(14, 22, 22, 38, dr, dg, db, 255);
        fillRect(42, 22, 50, 38, dr, dg, db, 255);
        fillRect(24, 20, 40, 30, sr, sg, sb, 220);
        fillRect(24, 34, 40, 45, or_, og, ob, 255);
        break;
    case 2:
        fillRect(20, 14, 30, 34, dr, dg, db, 255);
        fillRect(34, 14, 44, 34, dr, dg, db, 255);
        fillRect(18, 34, 30, 52, dr, dg, db, 255);
        fillRect(34, 34, 46, 52, dr, dg, db, 255);
        fillRect(24, 18, 28, 30, sr, sg, sb, 220);
        fillRect(38, 18, 42, 30, sr, sg, sb, 220);
        break;
    case 3:
        fillRect(16, 24, 30, 48, dr, dg, db, 255);
        fillRect(34, 24, 48, 48, dr, dg, db, 255);
        fillRect(14, 48, 32, 54, or_, og, ob, 255);
        fillRect(32, 48, 50, 54, or_, og, ob, 255);
        fillRect(20, 28, 28, 38, sr, sg, sb, 220);
        fillRect(38, 28, 46, 38, sr, sg, sb, 220);
        break;
    default:
        fillRect(30, 8, 36, 48, dr, dg, db, 255);
        fillRect(24, 42, 42, 50, or_, og, ob, 255);
        fillDiamond(33, 15, 9, sr, sg, sb, 230);
        fillDiamond(33, 27, 7, dr, dg, db, 255);
        break;
    }

    return true;
}

bool CreateItemHudPreviewTexture(int index) {
    std::vector<std::uint8_t> pixels;
    UINT width = 0;
    UINT height = 0;
    if (index < 0 ||
        index >= kItemHudPreviewTextureCount ||
        !DecodePngResource(kResourceItemHudPreviewBase + index, pixels, width, height)) {
        return false;
    }

    if (g_backend == RenderBackend::D3D11) {
        return CreateItemHudPreviewD3D11Texture(index, pixels, width, height);
    }
    if (g_backend == RenderBackend::D3D12) {
        return CreateItemHudPreviewD3D12Texture(index, pixels, width, height);
    }

    return false;
}

bool EnsureItemHudPreviewTextures() {
    bool anyReady = false;
    for (int i = 0; i < kItemHudPreviewTextureCount; ++i) {
        anyReady = anyReady ||
            g_itemHudPreviewTextureViews[i] != nullptr ||
            g_itemHudPreviewDx12Textures[i] != nullptr;
    }
    if (anyReady) {
        return true;
    }
    if (g_itemHudPreviewTexturesAttempted) {
        return false;
    }

    g_itemHudPreviewTexturesAttempted = true;
    bool loadedAny = false;
    for (int i = 0; i < kItemHudPreviewTextureCount; ++i) {
        loadedAny = CreateItemHudPreviewTexture(i) || loadedAny;
    }

    return loadedAny;
}
