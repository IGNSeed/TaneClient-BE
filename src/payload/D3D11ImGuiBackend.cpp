HWND GetSwapChainWindow(IDXGISwapChain* swapChain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (SUCCEEDED(swapChain->GetDesc(&desc)) && desc.OutputWindow != nullptr) {
        return desc.OutputWindow;
    }

    return FindCurrentProcessWindow();
}

bool ShouldRecordFpsForPresent(IDXGISwapChain* swapChain) {
    if (swapChain == nullptr) {
        return false;
    }

    LARGE_INTEGER counter{};
    if (!QueryPerformanceCounter(&counter)) {
        return swapChain == g_overlaySwapChain;
    }

    AcquireSRWLockExclusive(&g_fpsPresentLock);

    if (g_fpsPresentCounterFrequency <= 0) {
        LARGE_INTEGER frequency{};
        if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
            g_fpsPresentCounterFrequency = frequency.QuadPart;
        }
    }

    const ULONGLONG now = GetTickCount64();
    HWND window = GetSwapChainWindow(swapChain);
    const HWND gameWindow = g_gameWindow;
    const bool belongsToGameWindow =
        gameWindow == nullptr ||
        window == nullptr ||
        window == gameWindow;
    if (!belongsToGameWindow) {
        ReleaseSRWLockExclusive(&g_fpsPresentLock);
        return false;
    }

    FpsPresentCandidate* candidate = nullptr;
    FpsPresentCandidate* staleSlot = nullptr;
    FpsPresentCandidate* emptySlot = nullptr;
    for (FpsPresentCandidate& slot : g_fpsPresentCandidates) {
        if (slot.swapChain == swapChain) {
            candidate = &slot;
            break;
        }
        if (slot.swapChain == nullptr && emptySlot == nullptr) {
            emptySlot = &slot;
        } else if (slot.swapChain != nullptr &&
            now - slot.lastSeenTick > kFpsPresentCandidateStaleMs &&
            staleSlot == nullptr) {
            staleSlot = &slot;
        }
    }

    if (candidate == nullptr) {
        candidate = emptySlot != nullptr ? emptySlot : staleSlot;
        if (candidate == nullptr) {
            candidate = &g_fpsPresentCandidates[0];
        }
        *candidate = FpsPresentCandidate{};
        candidate->swapChain = swapChain;
        candidate->window = window;
    }

    candidate->lastSeenTick = now;
    candidate->window = window;
    if (g_fpsPresentCounterFrequency > 0 && candidate->lastCounter > 0 && counter.QuadPart > candidate->lastCounter) {
        const double deltaSeconds =
            static_cast<double>(counter.QuadPart - candidate->lastCounter) /
            static_cast<double>(g_fpsPresentCounterFrequency);
        if (deltaSeconds > 0.0 && deltaSeconds < 1.0) {
            candidate->accumulatedSeconds += deltaSeconds;
            ++candidate->accumulatedFrames;
            if (candidate->accumulatedSeconds >= kFpsPresentSelectionWindowSeconds) {
                candidate->measuredFps =
                    static_cast<double>(candidate->accumulatedFrames) /
                    candidate->accumulatedSeconds;
                candidate->accumulatedSeconds = 0.0;
                candidate->accumulatedFrames = 0;
            }
        }
    }
    candidate->lastCounter = counter.QuadPart;

    FpsPresentCandidate* selected = nullptr;
    for (FpsPresentCandidate& slot : g_fpsPresentCandidates) {
        if (slot.swapChain == nullptr ||
            now - slot.lastSeenTick > kFpsPresentCandidateStaleMs ||
            slot.measuredFps <= 0.0) {
            continue;
        }

        if (selected == nullptr || slot.measuredFps > selected->measuredFps * kFpsPresentSwitchMargin) {
            selected = &slot;
        }
    }

    if (selected != nullptr) {
        g_fpsPresentSourceSwapChain = selected->swapChain;
    } else if (g_fpsPresentSourceSwapChain == nullptr) {
        g_fpsPresentSourceSwapChain = g_overlaySwapChain != nullptr ? g_overlaySwapChain : swapChain;
    }

    const bool shouldRecord = swapChain == g_fpsPresentSourceSwapChain;
    ReleaseSRWLockExclusive(&g_fpsPresentLock);
    return shouldRecord;
}

bool InstallWindowSubclass(HWND hwnd) {
    if (hwnd == nullptr) {
        return false;
    }

    if (g_gameWindow == hwnd && g_originalWndProc != nullptr) {
        return true;
    }

    if (g_gameWindow != nullptr && g_originalWndProc != nullptr) {
        SetWindowLongPtrW(g_gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_gameWindow = nullptr;
        g_originalWndProc = nullptr;
    }

    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HookWndProc));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        return false;
    }

    g_gameWindow = hwnd;
    g_originalWndProc = reinterpret_cast<WNDPROC>(previous);
    return true;
}

void RemoveWindowSubclass() {
    UpdateMenuInputOwnership(g_gameWindow, false);

    if (g_gameWindow != nullptr && g_originalWndProc != nullptr) {
        SetWindowLongPtrW(g_gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
    }

    g_gameWindow = nullptr;
    g_originalWndProc = nullptr;
}

bool InitializePlatformBackend(IDXGISwapChain* swapChain) {
    if (g_platformReady.load()) {
        return true;
    }

    HWND hwnd = GetSwapChainWindow(swapChain);
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return false;
    }

    if (!InstallWindowSubclass(hwnd)) {
        return false;
    }

    if (!ImGui_ImplWin32_Init(hwnd)) {
        RemoveWindowSubclass();
        return false;
    }

    g_platformReady.store(true);
    return true;
}

void ReleaseDx12RenderTargets() {
    if (g_d3d12Frames == nullptr) {
        return;
    }

    for (UINT i = 0; i < g_d3d12BufferCount; ++i) {
        g_d3d12Frames[i].fenceValue = 0;
        if (g_d3d12Frames[i].renderTarget != nullptr) {
            g_d3d12Frames[i].renderTarget->Release();
            g_d3d12Frames[i].renderTarget = nullptr;
        }
    }
}

void WaitForDx12Queue() {
    ID3D12CommandQueue* commandQueue = g_d3d12CommandQueue.load();
    if (commandQueue == nullptr || g_d3d12Fence == nullptr || g_d3d12FenceEvent == nullptr) {
        return;
    }

    const UINT64 fenceValue = ++g_d3d12FenceValue;
    if (FAILED(commandQueue->Signal(g_d3d12Fence, fenceValue))) {
        return;
    }

    if (g_d3d12Fence->GetCompletedValue() < fenceValue) {
        if (SUCCEEDED(g_d3d12Fence->SetEventOnCompletion(fenceValue, g_d3d12FenceEvent))) {
            WaitForSingleObject(g_d3d12FenceEvent, 1000);
        }
    }
}

void WaitForDx12Frame(Dx12FrameContext& frame) {
    if (frame.fenceValue == 0 || g_d3d12Fence == nullptr || g_d3d12FenceEvent == nullptr) {
        return;
    }

    if (g_d3d12Fence->GetCompletedValue() < frame.fenceValue) {
        if (SUCCEEDED(g_d3d12Fence->SetEventOnCompletion(frame.fenceValue, g_d3d12FenceEvent))) {
            WaitForSingleObject(g_d3d12FenceEvent, 1000);
        }
    }

    frame.fenceValue = 0;
}

bool CreateRenderTarget(IDXGISwapChain* swapChain) {
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || backBuffer == nullptr) {
        return false;
    }

    hr = g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
    backBuffer->Release();

    if (FAILED(hr) || g_renderTarget == nullptr) {
        return false;
    }

    return true;
}

bool InitializeImGui(IDXGISwapChain* swapChain) {
    if (g_imguiReady.load()) {
        return true;
    }

    ImGuiExclusiveLock imguiLock(true);

    HRESULT hr = swapChain->GetDevice(IID_PPV_ARGS(&g_device));
    if (FAILED(hr) || g_device == nullptr) {
        return false;
    }

    g_device->GetImmediateContext(&g_context);
    if (g_context == nullptr) {
        g_device->Release();
        g_device = nullptr;
        return false;
    }

    if (!CreateRenderTarget(swapChain)) {
        g_context->Release();
        g_context = nullptr;
        g_device->Release();
        g_device = nullptr;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    LoadMenuFonts();

    ImGui::StyleColorsDark();

    if (!InitializePlatformBackend(swapChain)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX11_Init(g_device, g_context)) {
        ImGui_ImplWin32_Shutdown();
        g_platformReady.store(false);
        RemoveWindowSubclass();
        ImGui::DestroyContext();
        ReleaseRenderTarget();
        g_context->Release();
        g_context = nullptr;
        g_device->Release();
        g_device = nullptr;
        return false;
    }

    g_imguiReady.store(true);
    g_overlaySwapChain = swapChain;
    g_backend = RenderBackend::D3D11;
    return true;
}

bool CreateDx12RenderTargets(IDXGISwapChain* swapChain, DXGI_FORMAT format) {
    IDXGISwapChain3* swapChain3 = nullptr;
    HRESULT hr = swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3));
    if (FAILED(hr) || swapChain3 == nullptr) {
        return false;
    }

    const UINT rtvDescriptorSize = g_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_d3d12RtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < g_d3d12BufferCount; ++i) {
        if (g_d3d12Frames[i].renderTarget != nullptr) {
            g_d3d12Frames[i].renderTarget->Release();
            g_d3d12Frames[i].renderTarget = nullptr;
        }

        hr = swapChain3->GetBuffer(i, IID_PPV_ARGS(&g_d3d12Frames[i].renderTarget));
        if (FAILED(hr) || g_d3d12Frames[i].renderTarget == nullptr) {
            swapChain3->Release();
            return false;
        }

        g_d3d12Frames[i].rtvHandle = rtvHandle;
        g_d3d12Device->CreateRenderTargetView(g_d3d12Frames[i].renderTarget, nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
    }

    swapChain3->Release();
    (void)format;
    return true;
}

bool InitializeImGuiDx12(IDXGISwapChain* swapChain) {
    if (g_imguiReady.load()) {
        return g_backend == RenderBackend::D3D12;
    }

    ImGuiExclusiveLock imguiLock(true);

    ID3D12CommandQueue* commandQueue = g_d3d12CommandQueue.load();
    if (commandQueue == nullptr) {
        return false;
    }

    HRESULT hr = swapChain->GetDevice(IID_PPV_ARGS(&g_d3d12Device));
    if (FAILED(hr) || g_d3d12Device == nullptr) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    hr = swapChain->GetDesc(&desc);
    if (FAILED(hr)) {
        return false;
    }

    g_d3d12BufferCount = desc.BufferCount > 0 ? desc.BufferCount : 2;
    DXGI_FORMAT format = desc.BufferDesc.Format != DXGI_FORMAT_UNKNOWN ? desc.BufferDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = g_d3d12BufferCount;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = g_d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_d3d12RtvHeap));
    if (FAILED(hr) || g_d3d12RtvHeap == nullptr) {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = kDx12SrvDescriptorCount;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = g_d3d12Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_d3d12SrvHeap));
    if (FAILED(hr) || g_d3d12SrvHeap == nullptr) {
        return false;
    }
    g_d3d12SrvDescriptorSize = g_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ResetDx12SrvDescriptorPool();

    g_d3d12Frames = new Dx12FrameContext[g_d3d12BufferCount]{};
    for (UINT i = 0; i < g_d3d12BufferCount; ++i) {
        hr = g_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_d3d12Frames[i].commandAllocator));
        if (FAILED(hr) || g_d3d12Frames[i].commandAllocator == nullptr) {
            return false;
        }
    }

    hr = g_d3d12Device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_d3d12Frames[0].commandAllocator,
        nullptr,
        IID_PPV_ARGS(&g_d3d12CommandList));
    if (FAILED(hr) || g_d3d12CommandList == nullptr) {
        return false;
    }
    g_d3d12CommandList->Close();

    hr = g_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_d3d12Fence));
    if (FAILED(hr) || g_d3d12Fence == nullptr) {
        return false;
    }

    g_d3d12FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (g_d3d12FenceEvent == nullptr) {
        return false;
    }

    if (!CreateDx12RenderTargets(swapChain, format)) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    LoadMenuFonts();
    ImGui::StyleColorsDark();

    if (!InitializePlatformBackend(swapChain)) {
        ImGui::DestroyContext();
        return false;
    }

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = g_d3d12Device;
    initInfo.CommandQueue = commandQueue;
    initInfo.NumFramesInFlight = static_cast<int>(g_d3d12BufferCount);
    initInfo.RTVFormat = format;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = g_d3d12SrvHeap;
    initInfo.SrvDescriptorAllocFn = &AllocateDx12SrvDescriptor;
    initInfo.SrvDescriptorFreeFn = &FreeDx12SrvDescriptor;
    initInfo.LegacySingleSrvCpuDescriptor = g_d3d12SrvHeap->GetCPUDescriptorHandleForHeapStart();
    initInfo.LegacySingleSrvGpuDescriptor = g_d3d12SrvHeap->GetGPUDescriptorHandleForHeapStart();

    if (!ImGui_ImplDX12_Init(&initInfo)) {
        ImGui_ImplWin32_Shutdown();
        g_platformReady.store(false);
        RemoveWindowSubclass();
        ImGui::DestroyContext();
        return false;
    }

    g_backend = RenderBackend::D3D12;
    g_overlaySwapChain = swapChain;
    g_imguiReady.store(true);
    return true;
}

bool EnsureImGuiInitialized(IDXGISwapChain* swapChain) {
    if (g_imguiReady.load()) {
        return true;
    }

    bool expected = false;
    if (!g_imguiInitializing.compare_exchange_strong(expected, true)) {
        return false;
    }

    const bool initialized = InitializeImGui(swapChain) || InitializeImGuiDx12(swapChain);
    g_imguiInitializing.store(false);
    return initialized;
}

void RenderOverlay(IDXGISwapChain* swapChain) {
    if (!EnsureImGuiInitialized(swapChain)) {
        return;
    }

    if (!g_gameInputMethodsHookInstalled.load(std::memory_order_acquire)) {
        InstallGameInputHooks();
    }
    if (g_originalXInputGetState == nullptr || g_originalXInputGetCapabilities == nullptr) {
        InstallXInputHooks();
    }

    ImGuiExclusiveLock imguiLock(true);

    if (g_backend == RenderBackend::D3D11) {
        if (g_renderTarget == nullptr && !CreateRenderTarget(swapChain)) {
            return;
        }
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX11_NewFrame();
    } else if (g_backend == RenderBackend::D3D12) {
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX12_NewFrame();
    } else {
        return;
    }

    DrainPendingInputMessagesToImGui();
    ImGui::NewFrame();
    tane::gui::RenderOverlay();
    UpdateMenuInputOwnership(g_gameWindow, tane::gui::IsMenuOpen());

    ImGui::Render();

    if (g_backend == RenderBackend::D3D11) {
        ID3D11RenderTargetView* previousRenderTarget = nullptr;
        ID3D11DepthStencilView* previousDepthStencil = nullptr;
        g_context->OMGetRenderTargets(1, &previousRenderTarget, &previousDepthStencil);
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_context->OMSetRenderTargets(1, &previousRenderTarget, previousDepthStencil);

        if (previousRenderTarget != nullptr) {
            previousRenderTarget->Release();
        }
        if (previousDepthStencil != nullptr) {
            previousDepthStencil->Release();
        }
    } else if (g_backend == RenderBackend::D3D12) {
        IDXGISwapChain3* swapChain3 = nullptr;
        if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))) || swapChain3 == nullptr) {
            return;
        }

        const UINT frameIndex = swapChain3->GetCurrentBackBufferIndex();
        swapChain3->Release();
        if (frameIndex >= g_d3d12BufferCount) {
            return;
        }

        if (g_d3d12Frames[frameIndex].renderTarget == nullptr) {
            DXGI_SWAP_CHAIN_DESC currentDesc{};
            if (FAILED(swapChain->GetDesc(&currentDesc))) {
                return;
            }

            UINT currentBufferCount = currentDesc.BufferCount > 0 ? currentDesc.BufferCount : g_d3d12BufferCount;
            if (currentBufferCount != g_d3d12BufferCount) {
                return;
            }

            DXGI_FORMAT currentFormat = currentDesc.BufferDesc.Format != DXGI_FORMAT_UNKNOWN
                ? currentDesc.BufferDesc.Format
                : DXGI_FORMAT_R8G8B8A8_UNORM;
            if (!CreateDx12RenderTargets(swapChain, currentFormat)) {
                return;
            }
        }

        Dx12FrameContext& frame = g_d3d12Frames[frameIndex];
        WaitForDx12Frame(frame);

        if (FAILED(frame.commandAllocator->Reset())) {
            return;
        }
        if (FAILED(g_d3d12CommandList->Reset(frame.commandAllocator, nullptr))) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrierToRender{};
        barrierToRender.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierToRender.Transition.pResource = frame.renderTarget;
        barrierToRender.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrierToRender.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrierToRender.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_d3d12CommandList->ResourceBarrier(1, &barrierToRender);

        g_d3d12CommandList->OMSetRenderTargets(1, &frame.rtvHandle, FALSE, nullptr);
        ID3D12DescriptorHeap* descriptorHeaps[] = {g_d3d12SrvHeap};
        g_d3d12CommandList->SetDescriptorHeaps(1, descriptorHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_d3d12CommandList);

        D3D12_RESOURCE_BARRIER barrierToPresent = barrierToRender;
        barrierToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrierToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_d3d12CommandList->ResourceBarrier(1, &barrierToPresent);

        if (SUCCEEDED(g_d3d12CommandList->Close())) {
            ID3D12CommandList* commandLists[] = {g_d3d12CommandList};
            ID3D12CommandQueue* commandQueue = g_d3d12CommandQueue.load();
            if (commandQueue != nullptr) {
                if (g_originalExecuteCommandLists != nullptr) {
                    g_originalExecuteCommandLists(commandQueue, 1, commandLists);
                } else {
                    commandQueue->ExecuteCommandLists(1, commandLists);
                }
                if (g_d3d12Fence != nullptr) {
                    const UINT64 fenceValue = ++g_d3d12FenceValue;
                    if (SUCCEEDED(commandQueue->Signal(g_d3d12Fence, fenceValue))) {
                        frame.fenceValue = fenceValue;
                    }
                }
            }
        }
    }
}

HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    RenderOverlay(swapChain);
    tane::imgui_menu::TickInputBlock(nullptr);
    if (ShouldRecordFpsForPresent(swapChain)) {
        tane::gui::RecordFpsFrame();
    }
    return g_originalPresent(swapChain, syncInterval, flags);
}

HRESULT STDMETHODCALLTYPE HookResizeBuffers(
    IDXGISwapChain* swapChain,
    UINT bufferCount,
    UINT width,
    UINT height,
    DXGI_FORMAT newFormat,
    UINT swapChainFlags) {
    ImGuiExclusiveLock imguiLock(true);

    if (g_backend == RenderBackend::D3D12) {
        WaitForDx12Queue();
    } else if (g_backend == RenderBackend::D3D11) {
        UnbindD3D11RenderTargetForResize();
    }
    InvalidateImGuiDeviceObjectsForResize();
    ReleaseRenderTarget();
    ReleaseDx12RenderTargets();
    return g_originalResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
}

void STDMETHODCALLTYPE HookExecuteCommandLists(
    ID3D12CommandQueue* commandQueue,
    UINT numCommandLists,
    ID3D12CommandList* const* commandLists) {
    if (commandQueue != nullptr && g_d3d12CommandQueue.load() == nullptr) {
        D3D12_COMMAND_QUEUE_DESC desc = commandQueue->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            ID3D12CommandQueue* expected = nullptr;
            commandQueue->AddRef();
            if (g_d3d12CommandQueue.compare_exchange_strong(expected, commandQueue)) {
            } else {
                commandQueue->Release();
            }
        }
    }

    g_originalExecuteCommandLists(commandQueue, numCommandLists, commandLists);
}

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HWND CreateDummyWindow(HINSTANCE instance) {
    constexpr wchar_t className[] = L"TaneClientDummySwapChainWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return nullptr;
    }

    return CreateWindowExW(
        0,
        className,
        L"TaneClient",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        instance,
        nullptr);
}

BOOL CALLBACK FindProcessWindowProc(HWND hwnd, LPARAM param) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        *reinterpret_cast<HWND*>(param) = hwnd;
        return FALSE;
    }

    return TRUE;
}

HWND FindCurrentProcessWindow() {
    HWND hwnd = nullptr;
    EnumWindows(FindProcessWindowProc, reinterpret_cast<LPARAM>(&hwnd));
    return hwnd;
}

bool GetSwapChainMethods(void** present, void** resizeBuffers) {
    HINSTANCE instance = g_module != nullptr ? g_module : GetModuleHandleW(nullptr);
    HWND hwnd = CreateDummyWindow(instance);
    bool ownsWindow = hwnd != nullptr;
    if (hwnd == nullptr) {
        hwnd = FindCurrentProcessWindow();
        if (hwnd == nullptr) {
            return false;
        }
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Width = 100;
    desc.BufferDesc.Height = 100;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    IDXGISwapChain* swapChain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &desc,
        &swapChain,
        &device,
        &featureLevel,
        &context);

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &desc,
            &swapChain,
            &device,
            &featureLevel,
            &context);
    }

    if (FAILED(hr) || swapChain == nullptr) {
        if (ownsWindow) {
            DestroyWindow(hwnd);
            UnregisterClassW(L"TaneClientDummySwapChainWindow", instance);
        }
        return false;
    }

    auto vtable = *reinterpret_cast<void***>(swapChain);
    *present = vtable[8];
    *resizeBuffers = vtable[13];

    swapChain->Release();
    context->Release();
    device->Release();
    if (ownsWindow) {
        DestroyWindow(hwnd);
        UnregisterClassW(L"TaneClientDummySwapChainWindow", instance);
    }

    return *present != nullptr && *resizeBuffers != nullptr;
}

bool GetCommandQueueMethods(void** executeCommandLists) {
    ID3D12Device* device = nullptr;
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr) || device == nullptr) {
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    ID3D12CommandQueue* commandQueue = nullptr;
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr) || commandQueue == nullptr) {
        device->Release();
        return false;
    }

    auto vtable = *reinterpret_cast<void***>(commandQueue);
    *executeCommandLists = vtable[10];

    commandQueue->Release();
    device->Release();

    return *executeCommandLists != nullptr;
}

void InstallGameInputHooks() {
    constexpr const wchar_t* kGameInputDlls[] = {
        L"GameInput.dll",
        L"GameInputRedist.dll",
    };

    for (const wchar_t* dllName : kGameInputDlls) {
        HMODULE module = GetModuleHandleW(dllName);
        if (module == nullptr) {
            module = LoadLibraryW(dllName);
        }
        if (module == nullptr) {
            continue;
        }

        void* proc = ResolveExport(module, "GameInputCreate");
        if (proc == nullptr) {
            continue;
        }

        InstallGameInputCreateHook(proc);

        IGameInput* gameInput = nullptr;
        GameInputCreateFn create = g_originalGameInputCreate != nullptr
            ? g_originalGameInputCreate
            : reinterpret_cast<GameInputCreateFn>(proc);
        if (create != nullptr && SUCCEEDED(create(&gameInput)) && gameInput != nullptr) {
            InstallGameInputMethodHooks(gameInput);
            gameInput->Release();
        }

        if (g_gameInputMethodsHookInstalled.load(std::memory_order_acquire)) {
            return;
        }
    }
}

void InstallXInputHooks() {
    constexpr const wchar_t* kXInputDlls[] = {
        L"xinput1_4.dll",
        L"xinput1_3.dll",
        L"xinput9_1_0.dll",
    };

    for (const wchar_t* dllName : kXInputDlls) {
        HMODULE module = GetModuleHandleW(dllName);
        if (module == nullptr) {
            module = LoadLibraryW(dllName);
        }
        if (module == nullptr) {
            continue;
        }

        if (g_originalXInputGetState == nullptr) {
            void* getState = ResolveExport(module, "XInputGetState");
            if (getState != nullptr) {
                CreateAndEnableHook(getState, reinterpret_cast<void*>(&HookXInputGetState), reinterpret_cast<void**>(&g_originalXInputGetState));
            }
        }

        if (g_originalXInputGetCapabilities == nullptr) {
            void* getCapabilities = ResolveExport(module, "XInputGetCapabilities");
            if (getCapabilities != nullptr) {
                CreateAndEnableHook(
                    getCapabilities,
                    reinterpret_cast<void*>(&HookXInputGetCapabilities),
                    reinterpret_cast<void**>(&g_originalXInputGetCapabilities));
            }
        }

        if (g_originalXInputGetState != nullptr && g_originalXInputGetCapabilities != nullptr) {
            return;
        }
    }
}

void InstallInputApiHooks() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr) {
        user32 = LoadLibraryW(L"user32.dll");
    }
    if (user32 == nullptr) {
        return;
    }

    if (g_originalPeekMessageW == nullptr) {
        void* peekMessage = reinterpret_cast<void*>(GetProcAddress(user32, "PeekMessageW"));
        if (peekMessage != nullptr &&
            MH_CreateHook(peekMessage, &HookPeekMessageW, reinterpret_cast<void**>(&g_originalPeekMessageW)) == MH_OK) {
            MH_EnableHook(peekMessage);
        }
    }

    if (g_originalGetMessageW == nullptr) {
        void* getMessage = reinterpret_cast<void*>(GetProcAddress(user32, "GetMessageW"));
        if (getMessage != nullptr &&
            MH_CreateHook(getMessage, &HookGetMessageW, reinterpret_cast<void**>(&g_originalGetMessageW)) == MH_OK) {
            MH_EnableHook(getMessage);
        }
    }

    if (g_originalGetRawInputData == nullptr) {
        void* getRawInputData = reinterpret_cast<void*>(GetProcAddress(user32, "GetRawInputData"));
        if (getRawInputData != nullptr &&
            MH_CreateHook(getRawInputData, &HookGetRawInputData, reinterpret_cast<void**>(&g_originalGetRawInputData)) == MH_OK) {
            MH_EnableHook(getRawInputData);
        }
    }

    if (g_originalGetRawInputBuffer == nullptr) {
        void* getRawInputBuffer = reinterpret_cast<void*>(GetProcAddress(user32, "GetRawInputBuffer"));
        if (getRawInputBuffer != nullptr &&
            MH_CreateHook(getRawInputBuffer, &HookGetRawInputBuffer, reinterpret_cast<void**>(&g_originalGetRawInputBuffer)) == MH_OK) {
            MH_EnableHook(getRawInputBuffer);
        }
    }

    if (g_originalGetAsyncKeyState == nullptr) {
        void* getAsyncKeyState = reinterpret_cast<void*>(GetProcAddress(user32, "GetAsyncKeyState"));
        if (getAsyncKeyState != nullptr &&
            MH_CreateHook(getAsyncKeyState, &HookGetAsyncKeyState, reinterpret_cast<void**>(&g_originalGetAsyncKeyState)) == MH_OK) {
            MH_EnableHook(getAsyncKeyState);
        }
    }

    if (g_originalGetKeyState == nullptr) {
        void* getKeyState = reinterpret_cast<void*>(GetProcAddress(user32, "GetKeyState"));
        if (getKeyState != nullptr &&
            MH_CreateHook(getKeyState, &HookGetKeyState, reinterpret_cast<void**>(&g_originalGetKeyState)) == MH_OK) {
            MH_EnableHook(getKeyState);
        }
    }

    if (g_originalSetCursorPos == nullptr) {
        void* setCursorPos = reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos"));
        if (setCursorPos != nullptr &&
            MH_CreateHook(setCursorPos, &HookSetCursorPos, reinterpret_cast<void**>(&g_originalSetCursorPos)) == MH_OK) {
            MH_EnableHook(setCursorPos);
        }
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 != nullptr && g_originalGetProcAddress == nullptr) {
        void* getProcAddress = reinterpret_cast<void*>(GetProcAddress(kernel32, "GetProcAddress"));
        if (getProcAddress != nullptr &&
            MH_CreateHook(getProcAddress, &HookGetProcAddress, reinterpret_cast<void**>(&g_originalGetProcAddress)) == MH_OK) {
            MH_EnableHook(getProcAddress);
        }
    }

    InstallGameInputHooks();
    InstallXInputHooks();
}

}  // namespace
