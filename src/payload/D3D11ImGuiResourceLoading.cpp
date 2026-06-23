void ReleaseRenderTarget() {
    if (g_renderTarget != nullptr) {
        g_renderTarget->Release();
        g_renderTarget = nullptr;
    }
}

void UnbindD3D11RenderTargetForResize() {
    if (g_context == nullptr) {
        return;
    }

    ID3D11RenderTargetView* nullRenderTarget = nullptr;
    g_context->OMSetRenderTargets(1, &nullRenderTarget, nullptr);
    g_context->Flush();
}

void InvalidateImGuiDeviceObjectsForResize() {
    if (!g_imguiReady.load(std::memory_order_acquire) || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    if (g_backend == RenderBackend::D3D11) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    } else if (g_backend == RenderBackend::D3D12) {
        ImGui_ImplDX12_InvalidateDeviceObjects();
    }
}

void ReleaseMenuLogoTexture() {
    if (g_menuLogoTextureView != nullptr) {
        g_menuLogoTextureView->Release();
        g_menuLogoTextureView = nullptr;
    }
    if (g_menuLogoDx12Texture != nullptr) {
        g_menuLogoDx12Texture->Release();
        g_menuLogoDx12Texture = nullptr;
    }

    g_menuLogoDx12CpuHandle = {};
    g_menuLogoDx12GpuHandle = {};
    g_menuLogoTextureSize = ImVec2(0.0f, 0.0f);
    g_menuLogoTextureAttempted = false;
}

void ReleaseItemHudPreviewTextures() {
    for (int i = 0; i < kItemHudPreviewTextureCount; ++i) {
        if (g_itemHudPreviewTextureViews[i] != nullptr) {
            g_itemHudPreviewTextureViews[i]->Release();
            g_itemHudPreviewTextureViews[i] = nullptr;
        }
        if (g_itemHudPreviewDx12Textures[i] != nullptr) {
            g_itemHudPreviewDx12Textures[i]->Release();
            g_itemHudPreviewDx12Textures[i] = nullptr;
        }
        g_itemHudPreviewDx12CpuHandles[i] = {};
        g_itemHudPreviewDx12GpuHandles[i] = {};
        g_itemHudPreviewTextureSizes[i] = ImVec2(0.0f, 0.0f);
    }
    g_itemHudPreviewTexturesAttempted = false;
}

void GetDx12SrvDescriptorHandles(
    UINT descriptorIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle) {
    if (g_d3d12SrvHeap == nullptr || g_d3d12SrvDescriptorSize == 0 || descriptorIndex >= kDx12SrvDescriptorCount) {
        if (cpuHandle != nullptr) {
            *cpuHandle = {};
        }
        if (gpuHandle != nullptr) {
            *gpuHandle = {};
        }
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = g_d3d12SrvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = g_d3d12SrvHeap->GetGPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(g_d3d12SrvDescriptorSize) * descriptorIndex;
    gpu.ptr += static_cast<UINT64>(g_d3d12SrvDescriptorSize) * descriptorIndex;

    if (cpuHandle != nullptr) {
        *cpuHandle = cpu;
    }
    if (gpuHandle != nullptr) {
        *gpuHandle = gpu;
    }
}

void ResetDx12SrvDescriptorPool() {
    std::memset(g_d3d12SrvDescriptorUsed, 0, sizeof(g_d3d12SrvDescriptorUsed));
    g_d3d12SrvDescriptorUsed[kMenuLogoSrvDescriptorIndex] = true;
    for (UINT descriptorIndex = 0; descriptorIndex < static_cast<UINT>(kItemHudPreviewTextureCount); ++descriptorIndex) {
        const UINT reservedIndex = kItemHudPreviewSrvDescriptorStart + descriptorIndex;
        if (reservedIndex < kDx12SrvDescriptorCount) {
            g_d3d12SrvDescriptorUsed[reservedIndex] = true;
        }
    }
}

bool IsReservedDx12SrvDescriptor(UINT descriptorIndex) {
    return descriptorIndex == kMenuLogoSrvDescriptorIndex ||
        (descriptorIndex >= kItemHudPreviewSrvDescriptorStart &&
            descriptorIndex < kItemHudPreviewSrvDescriptorStart + static_cast<UINT>(kItemHudPreviewTextureCount));
}

void AllocateDx12SrvDescriptor(
    ImGui_ImplDX12_InitInfo*,
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
    if (outCpuHandle != nullptr) {
        *outCpuHandle = {};
    }
    if (outGpuHandle != nullptr) {
        *outGpuHandle = {};
    }

    for (UINT descriptorIndex = 0; descriptorIndex < kDx12SrvDescriptorCount; ++descriptorIndex) {
        if (g_d3d12SrvDescriptorUsed[descriptorIndex]) {
            continue;
        }

        g_d3d12SrvDescriptorUsed[descriptorIndex] = true;
        GetDx12SrvDescriptorHandles(descriptorIndex, outCpuHandle, outGpuHandle);
        return;
    }
}

void FreeDx12SrvDescriptor(
    ImGui_ImplDX12_InitInfo*,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE) {
    if (g_d3d12SrvHeap == nullptr || g_d3d12SrvDescriptorSize == 0 || cpuHandle.ptr == 0) {
        return;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE start = g_d3d12SrvHeap->GetCPUDescriptorHandleForHeapStart();
    if (cpuHandle.ptr < start.ptr) {
        return;
    }

    const SIZE_T offset = cpuHandle.ptr - start.ptr;
    if ((offset % g_d3d12SrvDescriptorSize) != 0) {
        return;
    }

    const UINT descriptorIndex = static_cast<UINT>(offset / g_d3d12SrvDescriptorSize);
    if (descriptorIndex >= kDx12SrvDescriptorCount || IsReservedDx12SrvDescriptor(descriptorIndex)) {
        return;
    }

    g_d3d12SrvDescriptorUsed[descriptorIndex] = false;
}

void FreeDx12SrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) {
    FreeDx12SrvDescriptor(nullptr, cpuHandle, {});
}

void ReleaseExternalPngTextures() {
    for (ExternalPngTexture& texture : g_externalPngTextures) {
        if (texture.d3d11View != nullptr) {
            texture.d3d11View->Release();
        }
        if (texture.d3d12Texture != nullptr) {
            texture.d3d12Texture->Release();
        }
        FreeDx12SrvDescriptor(texture.d3d12CpuHandle);
        texture = {};
    }
}

void ReleaseMenuFonts() {
    g_menuRegularFont = nullptr;
    g_menuBoldFont = nullptr;
    g_itemHudFont = nullptr;
    g_menuRegularFontBytes.clear();
    g_menuBoldFontBytes.clear();
    g_itemHudFontBytes.clear();
}

bool CopyResourceBytes(int resourceId, const wchar_t* resourceType, std::vector<std::uint8_t>& output) {
    output.clear();

    HMODULE module = g_module != nullptr ? g_module : GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), resourceType);
    if (resource == nullptr) {
        return false;
    }

    HGLOBAL loadedResource = LoadResource(module, resource);
    if (loadedResource == nullptr) {
        return false;
    }

    const DWORD resourceSize = SizeofResource(module, resource);
    const void* resourceData = LockResource(loadedResource);
    if (resourceData == nullptr || resourceSize == 0) {
        return false;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(resourceData);
    output.assign(bytes, bytes + resourceSize);
    return true;
}

ImFont* LoadMenuFontResource(
    int resourceId,
    float sizePixels,
    std::vector<std::uint8_t>& fontBytes,
    bool optimizeSmallText = false) {
    if (!CopyResourceBytes(resourceId, RT_RCDATA, fontBytes) || fontBytes.empty() || ImGui::GetCurrentContext() == nullptr) {
        return nullptr;
    }

    ImFontConfig fontConfig{};
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.PixelSnapH = true;
    fontConfig.OversampleH = optimizeSmallText ? 4 : 3;
    fontConfig.OversampleV = optimizeSmallText ? 4 : 2;
    fontConfig.RasterizerMultiply = optimizeSmallText ? 1.14f : 1.08f;

    ImGuiIO& io = ImGui::GetIO();
    return io.Fonts->AddFontFromMemoryTTF(
        fontBytes.data(),
        static_cast<int>(fontBytes.size()),
        sizePixels,
        &fontConfig,
        io.Fonts->GetGlyphRangesDefault());
}

void LoadMenuFonts() {
    ImGuiIO& io = ImGui::GetIO();
    g_menuRegularFont = LoadMenuFontResource(kResourceMenuRegularFont, 18.0f, g_menuRegularFontBytes);
    g_menuBoldFont = LoadMenuFontResource(kResourceMenuBoldFont, 18.0f, g_menuBoldFontBytes);
    g_itemHudFont = LoadMenuFontResource(kResourceItemHudFont, 32.0f, g_itemHudFontBytes, true);

    if (g_menuRegularFont != nullptr) {
        io.FontDefault = g_menuRegularFont;
    } else {
        io.Fonts->AddFontDefault();
    }
}

bool DecodePngResource(int resourceId, std::vector<std::uint8_t>& pixels, UINT& width, UINT& height) {
    pixels.clear();
    width = 0;
    height = 0;

    std::vector<std::uint8_t> pngBytes;
    if (!CopyResourceBytes(resourceId, RT_RCDATA, pngBytes) || pngBytes.empty()) {
        return false;
    }

    const HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitializeCom = coInit == S_OK || coInit == S_FALSE;

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || factory == nullptr) {
        if (shouldUninitializeCom) {
            CoUninitialize();
        }
        return false;
    }

    IWICStream* stream = nullptr;
    hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory(pngBytes.data(), static_cast<DWORD>(pngBytes.size()));
    }

    IWICBitmapDecoder* decoder = nullptr;
    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    }

    IWICBitmapFrameDecode* frame = nullptr;
    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }

    if (SUCCEEDED(hr)) {
        hr = frame->GetSize(&width, &height);
        if (SUCCEEDED(hr) && (width == 0 || height == 0)) {
            hr = E_FAIL;
        }
    }

    IWICFormatConverter* converter = nullptr;
    if (SUCCEEDED(hr)) {
        hr = factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
    }

    if (SUCCEEDED(hr)) {
        pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    }

    if (converter != nullptr) {
        converter->Release();
    }
    if (frame != nullptr) {
        frame->Release();
    }
    if (decoder != nullptr) {
        decoder->Release();
    }
    if (stream != nullptr) {
        stream->Release();
    }
    factory->Release();
    if (shouldUninitializeCom) {
        CoUninitialize();
    }

    if (FAILED(hr) || pixels.empty()) {
        pixels.clear();
        width = 0;
        height = 0;
        return false;
    }

    return true;
}

bool DecodeMenuLogoPng(std::vector<std::uint8_t>& pixels, UINT& width, UINT& height) {
    return DecodePngResource(kResourceMenuLogoPng, pixels, width, height);
}

bool DecodePngFile(const wchar_t* path, std::vector<std::uint8_t>& pixels, UINT& width, UINT& height) {
    pixels.clear();
    width = 0;
    height = 0;
    if (path == nullptr || path[0] == L'\0') {
        return false;
    }

    const HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitializeCom = coInit == S_OK || coInit == S_FALSE;

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || factory == nullptr) {
        if (shouldUninitializeCom) {
            CoUninitialize();
        }
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

    IWICBitmapFrameDecode* frame = nullptr;
    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }

    if (SUCCEEDED(hr)) {
        hr = frame->GetSize(&width, &height);
        if (SUCCEEDED(hr) && (width == 0 || height == 0)) {
            hr = E_FAIL;
        }
    }

    IWICFormatConverter* converter = nullptr;
    if (SUCCEEDED(hr)) {
        hr = factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
    }

    if (SUCCEEDED(hr)) {
        pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    }

    if (converter != nullptr) {
        converter->Release();
    }
    if (frame != nullptr) {
        frame->Release();
    }
    if (decoder != nullptr) {
        decoder->Release();
    }
    factory->Release();
    if (shouldUninitializeCom) {
        CoUninitialize();
    }

    if (FAILED(hr) || pixels.empty()) {
        pixels.clear();
        width = 0;
        height = 0;
        return false;
    }

    return true;
}
