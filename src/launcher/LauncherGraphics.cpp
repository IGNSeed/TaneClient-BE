void CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_renderTargetView);
    }
}

void CleanupRenderTarget() {
    g_renderTargetView.Reset();
}

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = kWindowWidth;
    swapChainDesc.BufferDesc.Height = kWindowHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        featureLevels,
        static_cast<UINT>(_countof(featureLevels)),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &g_swapChain,
        &g_device,
        &featureLevel,
        &g_deviceContext);

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            featureLevels,
            static_cast<UINT>(_countof(featureLevels)),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &g_swapChain,
            &g_device,
            &featureLevel,
            &g_deviceContext);
    }

    if (FAILED(hr)) {
        return false;
    }

    CreateRenderTarget();
    return g_renderTargetView != nullptr;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    g_swapChain.Reset();
    g_deviceContext.Reset();
    g_device.Reset();
}

bool CreateLogoTextureFromResource() {
    std::vector<std::uint8_t> pngBytes;
    if (!CopyResourceBytes(kResourceLogoPng, RT_RCDATA, pngBytes)) {
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        return false;
    }

    hr = stream->InitializeFromMemory(pngBytes.data(), static_cast<DWORD>(pngBytes.size()));
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);
    if (width == 0 || height == 0) {
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return false;
    }

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return false;
    }

    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource{};
    subResource.pSysMem = pixels.data();
    subResource.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> texture;
    hr = g_device->CreateTexture2D(&desc, &subResource, &texture);
    if (FAILED(hr)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = g_device->CreateShaderResourceView(texture.Get(), &srvDesc, &g_logoTextureView);
    return SUCCEEDED(hr);
}

void LoadLauncherFont() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    if (!CopyResourceBytes(kResourceFont, RT_RCDATA, g_fontBytes) || g_fontBytes.empty()) {
        return;
    }

    ImFontConfig fontConfig{};
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.PixelSnapH = false;
    fontConfig.OversampleH = 3;
    fontConfig.OversampleV = 2;
    fontConfig.RasterizerMultiply = 1.06f;
    g_launcherFont = io.Fonts->AddFontFromMemoryTTF(
        g_fontBytes.data(),
        static_cast<int>(g_fontBytes.size()),
        18.0f,
        &fontConfig,
        io.Fonts->GetGlyphRangesDefault());

    ImFontConfig compactFontConfig = fontConfig;
    compactFontConfig.RasterizerMultiply = 1.16f;
    g_compactFont = io.Fonts->AddFontFromMemoryTTF(
        g_fontBytes.data(),
        static_cast<int>(g_fontBytes.size()),
        13.0f,
        &compactFontConfig,
        io.Fonts->GetGlyphRangesDefault());

    ImFontConfig titleFontConfig = fontConfig;
    titleFontConfig.OversampleH = 1;
    titleFontConfig.OversampleV = 1;
    titleFontConfig.RasterizerMultiply = 1.0f;
    g_titleFont = io.Fonts->AddFontFromMemoryTTF(
        g_fontBytes.data(),
        static_cast<int>(g_fontBytes.size()),
        54.0f,
        &titleFontConfig,
        io.Fonts->GetGlyphRangesDefault());

    if (g_launcherFont != nullptr) {
        io.FontDefault = g_launcherFont;
    }
}

void ApplyRoundedCorners(HWND hwnd) {
    constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    enum DwmWindowCornerPreference {
        DwmWindowCornerPreferenceRound = 2
    };

    DwmWindowCornerPreference preference = DwmWindowCornerPreferenceRound;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
}

void ApplyImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.WindowRounding = kCornerRadius;
    style.FrameRounding = 13.0f;
    style.FramePadding = ImVec2(0.0f, 0.0f);
    style.ItemSpacing = ImVec2(0.0f, 0.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(25.0f / 255.0f, 25.0f / 255.0f, 25.0f / 255.0f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(238.0f / 255.0f, 238.0f / 255.0f, 238.0f / 255.0f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(218.0f / 255.0f, 218.0f / 255.0f, 218.0f / 255.0f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}
