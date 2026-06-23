bool CreateMenuLogoD3D11Texture(const std::vector<std::uint8_t>& pixels, UINT width, UINT height) {
    if (g_device == nullptr || pixels.empty() || width == 0 || height == 0) {
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

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = g_device->CreateTexture2D(&desc, &subResource, &texture);
    if (SUCCEEDED(hr) && texture != nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        hr = g_device->CreateShaderResourceView(texture, &srvDesc, &g_menuLogoTextureView);
        texture->Release();
    }

    if (FAILED(hr) || g_menuLogoTextureView == nullptr) {
        return false;
    }

    g_menuLogoTextureSize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    return true;
}

bool CreateItemHudPreviewD3D11Texture(int index, const std::vector<std::uint8_t>& pixels, UINT width, UINT height) {
    if (index < 0 || index >= kItemHudPreviewTextureCount ||
        g_device == nullptr || pixels.empty() || width == 0 || height == 0) {
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

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = g_device->CreateTexture2D(&desc, &subResource, &texture);
    if (SUCCEEDED(hr) && texture != nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        hr = g_device->CreateShaderResourceView(texture, &srvDesc, &g_itemHudPreviewTextureViews[index]);
        texture->Release();
    }

    if (FAILED(hr) || g_itemHudPreviewTextureViews[index] == nullptr) {
        return false;
    }

    g_itemHudPreviewTextureSizes[index] = ImVec2(static_cast<float>(width), static_cast<float>(height));
    return true;
}

bool WaitForDx12FenceValue(UINT64 fenceValue) {
    if (g_d3d12Fence == nullptr || g_d3d12FenceEvent == nullptr) {
        return false;
    }

    if (g_d3d12Fence->GetCompletedValue() >= fenceValue) {
        return true;
    }

    if (FAILED(g_d3d12Fence->SetEventOnCompletion(fenceValue, g_d3d12FenceEvent))) {
        return false;
    }

    return WaitForSingleObject(g_d3d12FenceEvent, 1000) == WAIT_OBJECT_0;
}

bool CreateMenuLogoD3D12Texture(const std::vector<std::uint8_t>& pixels, UINT width, UINT height) {
    ID3D12CommandQueue* commandQueue = g_d3d12CommandQueue.load();
    if (g_d3d12Device == nullptr || g_d3d12SrvHeap == nullptr || commandQueue == nullptr ||
        pixels.empty() || width == 0 || height == 0 || g_d3d12SrvDescriptorSize == 0) {
        return false;
    }

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ID3D12Resource* texture = nullptr;
    HRESULT hr = g_d3d12Device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture));
    if (FAILED(hr) || texture == nullptr) {
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadFootprint{};
    UINT uploadRowCount = 0;
    UINT64 uploadRowSize = 0;
    UINT64 uploadBufferSize = 0;
    g_d3d12Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &uploadFootprint, &uploadRowCount, &uploadRowSize, &uploadBufferSize);

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* uploadBuffer = nullptr;
    hr = g_d3d12Device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    if (FAILED(hr) || uploadBuffer == nullptr) {
        texture->Release();
        return false;
    }

    std::uint8_t* mappedUpload = nullptr;
    D3D12_RANGE readRange{};
    hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedUpload));
    if (SUCCEEDED(hr) && mappedUpload != nullptr) {
        const UINT sourcePitch = width * 4;
        std::uint8_t* uploadStart = mappedUpload + uploadFootprint.Offset;
        for (UINT row = 0; row < height; ++row) {
            std::memcpy(
                uploadStart + static_cast<std::size_t>(row) * uploadFootprint.Footprint.RowPitch,
                pixels.data() + static_cast<std::size_t>(row) * sourcePitch,
                sourcePitch);
        }
        uploadBuffer->Unmap(0, nullptr);
    }
    if (FAILED(hr)) {
        uploadBuffer->Release();
        texture->Release();
        return false;
    }

    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = g_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    if (SUCCEEDED(hr)) {
        hr = g_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&commandList));
    }
    if (FAILED(hr) || allocator == nullptr || commandList == nullptr) {
        if (commandList != nullptr) {
            commandList->Release();
        }
        if (allocator != nullptr) {
            allocator->Release();
        }
        uploadBuffer->Release();
        texture->Release();
        return false;
    }

    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = texture;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = uploadBuffer;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = uploadFootprint;
    commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    hr = commandList->Close();
    if (SUCCEEDED(hr)) {
        ID3D12CommandList* commandLists[] = {commandList};
        commandQueue->ExecuteCommandLists(1, commandLists);
        const UINT64 fenceValue = ++g_d3d12FenceValue;
        hr = commandQueue->Signal(g_d3d12Fence, fenceValue);
        if (SUCCEEDED(hr) && !WaitForDx12FenceValue(fenceValue)) {
            hr = E_FAIL;
        }
    }

    commandList->Release();
    allocator->Release();
    uploadBuffer->Release();
    if (FAILED(hr)) {
        texture->Release();
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
    GetDx12SrvDescriptorHandles(kMenuLogoSrvDescriptorIndex, &cpuHandle, &gpuHandle);
    if (cpuHandle.ptr == 0 || gpuHandle.ptr == 0) {
        texture->Release();
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    g_d3d12Device->CreateShaderResourceView(texture, &srvDesc, cpuHandle);

    g_menuLogoDx12Texture = texture;
    g_menuLogoDx12CpuHandle = cpuHandle;
    g_menuLogoDx12GpuHandle = gpuHandle;
    g_menuLogoTextureSize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    return true;
}

bool CreateItemHudPreviewD3D12Texture(int index, const std::vector<std::uint8_t>& pixels, UINT width, UINT height) {
    ID3D12CommandQueue* commandQueue = g_d3d12CommandQueue.load();
    if (index < 0 || index >= kItemHudPreviewTextureCount ||
        g_d3d12Device == nullptr || g_d3d12SrvHeap == nullptr || commandQueue == nullptr ||
        pixels.empty() || width == 0 || height == 0 || g_d3d12SrvDescriptorSize == 0) {
        return false;
    }

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ID3D12Resource* texture = nullptr;
    HRESULT hr = g_d3d12Device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture));
    if (FAILED(hr) || texture == nullptr) {
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadFootprint{};
    UINT uploadRowCount = 0;
    UINT64 uploadRowSize = 0;
    UINT64 uploadBufferSize = 0;
    g_d3d12Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &uploadFootprint, &uploadRowCount, &uploadRowSize, &uploadBufferSize);

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* uploadBuffer = nullptr;
    hr = g_d3d12Device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    if (FAILED(hr) || uploadBuffer == nullptr) {
        texture->Release();
        return false;
    }

    std::uint8_t* mappedUpload = nullptr;
    D3D12_RANGE readRange{};
    hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedUpload));
    if (SUCCEEDED(hr) && mappedUpload != nullptr) {
        const UINT sourcePitch = width * 4;
        std::uint8_t* uploadStart = mappedUpload + uploadFootprint.Offset;
        for (UINT row = 0; row < height; ++row) {
            std::memcpy(
                uploadStart + static_cast<std::size_t>(row) * uploadFootprint.Footprint.RowPitch,
                pixels.data() + static_cast<std::size_t>(row) * sourcePitch,
                sourcePitch);
        }
        uploadBuffer->Unmap(0, nullptr);
    }
    if (FAILED(hr)) {
        uploadBuffer->Release();
        texture->Release();
        return false;
    }

    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = g_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    if (SUCCEEDED(hr)) {
        hr = g_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&commandList));
    }
    if (FAILED(hr) || allocator == nullptr || commandList == nullptr) {
        if (commandList != nullptr) {
            commandList->Release();
        }
        if (allocator != nullptr) {
            allocator->Release();
        }
        uploadBuffer->Release();
        texture->Release();
        return false;
    }

    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = texture;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = uploadBuffer;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = uploadFootprint;
    commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    hr = commandList->Close();
    if (SUCCEEDED(hr)) {
        ID3D12CommandList* commandLists[] = {commandList};
        commandQueue->ExecuteCommandLists(1, commandLists);
        const UINT64 fenceValue = ++g_d3d12FenceValue;
        hr = commandQueue->Signal(g_d3d12Fence, fenceValue);
        if (SUCCEEDED(hr) && !WaitForDx12FenceValue(fenceValue)) {
            hr = E_FAIL;
        }
    }

    commandList->Release();
    allocator->Release();
    uploadBuffer->Release();
    if (FAILED(hr)) {
        texture->Release();
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
    GetDx12SrvDescriptorHandles(kItemHudPreviewSrvDescriptorStart + static_cast<UINT>(index), &cpuHandle, &gpuHandle);
    if (cpuHandle.ptr == 0 || gpuHandle.ptr == 0) {
        texture->Release();
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    g_d3d12Device->CreateShaderResourceView(texture, &srvDesc, cpuHandle);

    g_itemHudPreviewDx12Textures[index] = texture;
    g_itemHudPreviewDx12CpuHandles[index] = cpuHandle;
    g_itemHudPreviewDx12GpuHandles[index] = gpuHandle;
    g_itemHudPreviewTextureSizes[index] = ImVec2(static_cast<float>(width), static_cast<float>(height));
    return true;
}

bool CreateExternalPngD3D11Texture(int index, const std::vector<std::uint8_t>& pixels, UINT width, UINT height) {
    if (index < 0 || index >= kExternalPngTextureCount ||
        g_device == nullptr || pixels.empty() || width == 0 || height == 0) {
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

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = g_device->CreateTexture2D(&desc, &subResource, &texture);
    if (SUCCEEDED(hr) && texture != nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        hr = g_device->CreateShaderResourceView(texture, &srvDesc, &g_externalPngTextures[index].d3d11View);
        texture->Release();
    }

    if (FAILED(hr) || g_externalPngTextures[index].d3d11View == nullptr) {
        return false;
    }

    g_externalPngTextures[index].size = ImVec2(static_cast<float>(width), static_cast<float>(height));
    return true;
}

bool CreateExternalPngD3D12Texture(int index, const std::vector<std::uint8_t>& pixels, UINT width, UINT height) {
    ID3D12CommandQueue* commandQueue = g_d3d12CommandQueue.load();
    if (index < 0 || index >= kExternalPngTextureCount ||
        g_d3d12Device == nullptr || g_d3d12SrvHeap == nullptr || commandQueue == nullptr ||
        pixels.empty() || width == 0 || height == 0 || g_d3d12SrvDescriptorSize == 0) {
        return false;
    }

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ID3D12Resource* texture = nullptr;
    HRESULT hr = g_d3d12Device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture));
    if (FAILED(hr) || texture == nullptr) {
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadFootprint{};
    UINT uploadRowCount = 0;
    UINT64 uploadRowSize = 0;
    UINT64 uploadBufferSize = 0;
    g_d3d12Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &uploadFootprint, &uploadRowCount, &uploadRowSize, &uploadBufferSize);

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* uploadBuffer = nullptr;
    hr = g_d3d12Device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    if (FAILED(hr) || uploadBuffer == nullptr) {
        texture->Release();
        return false;
    }

    std::uint8_t* mappedUpload = nullptr;
    D3D12_RANGE readRange{};
    hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedUpload));
    if (SUCCEEDED(hr) && mappedUpload != nullptr) {
        const UINT sourcePitch = width * 4;
        std::uint8_t* uploadStart = mappedUpload + uploadFootprint.Offset;
        for (UINT row = 0; row < height; ++row) {
            std::memcpy(
                uploadStart + static_cast<std::size_t>(row) * uploadFootprint.Footprint.RowPitch,
                pixels.data() + static_cast<std::size_t>(row) * sourcePitch,
                sourcePitch);
        }
        uploadBuffer->Unmap(0, nullptr);
    }
    if (FAILED(hr)) {
        uploadBuffer->Release();
        texture->Release();
        return false;
    }

    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = g_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    if (SUCCEEDED(hr)) {
        hr = g_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&commandList));
    }
    if (FAILED(hr) || allocator == nullptr || commandList == nullptr) {
        if (commandList != nullptr) {
            commandList->Release();
        }
        if (allocator != nullptr) {
            allocator->Release();
        }
        uploadBuffer->Release();
        texture->Release();
        return false;
    }

    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = texture;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = uploadBuffer;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = uploadFootprint;
    commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    hr = commandList->Close();
    if (SUCCEEDED(hr)) {
        ID3D12CommandList* commandLists[] = {commandList};
        commandQueue->ExecuteCommandLists(1, commandLists);
        const UINT64 fenceValue = ++g_d3d12FenceValue;
        hr = commandQueue->Signal(g_d3d12Fence, fenceValue);
        if (SUCCEEDED(hr) && !WaitForDx12FenceValue(fenceValue)) {
            hr = E_FAIL;
        }
    }

    commandList->Release();
    allocator->Release();
    uploadBuffer->Release();
    if (FAILED(hr)) {
        texture->Release();
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
    AllocateDx12SrvDescriptor(nullptr, &cpuHandle, &gpuHandle);
    if (cpuHandle.ptr == 0 || gpuHandle.ptr == 0) {
        texture->Release();
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    g_d3d12Device->CreateShaderResourceView(texture, &srvDesc, cpuHandle);

    g_externalPngTextures[index].d3d12Texture = texture;
    g_externalPngTextures[index].d3d12CpuHandle = cpuHandle;
    g_externalPngTextures[index].d3d12GpuHandle = gpuHandle;
    g_externalPngTextures[index].size = ImVec2(static_cast<float>(width), static_cast<float>(height));
    return true;
}
