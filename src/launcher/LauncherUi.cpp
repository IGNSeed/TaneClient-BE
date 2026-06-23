void RenderLauncherUi() {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight)), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("TaneClientLauncher", nullptr, flags);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetWindowPos();
    ImVec2 size(static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight));
    const ImU32 background = IM_COL32(25, 25, 25, 255);
    const ImU32 white = IM_COL32(255, 255, 255, 255);

    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), background, kCornerRadius);

    ImVec2 closeMin(origin.x + 372.0f, origin.y + 14.0f);
    ImVec2 closeMax(origin.x + 406.0f, origin.y + 48.0f);
    ImGui::SetCursorScreenPos(closeMin);
    ImGui::InvisibleButton("##close", ImVec2(closeMax.x - closeMin.x, closeMax.y - closeMin.y));
    bool closeHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        PostMessageW(g_mainWindow, WM_CLOSE, 0, 0);
    }

    ImU32 closeColor = closeHovered ? white : IM_COL32(255, 255, 255, 210);
    drawList->AddLine(ImVec2(closeMin.x + 11.0f, closeMin.y + 11.0f), ImVec2(closeMax.x - 11.0f, closeMax.y - 11.0f), closeColor, 2.0f);
    drawList->AddLine(ImVec2(closeMax.x - 11.0f, closeMin.y + 11.0f), ImVec2(closeMin.x + 11.0f, closeMax.y - 11.0f), closeColor, 2.0f);

    const bool buttonEnabled = g_injectButtonEnabled.load();

    if (g_logoTextureView != nullptr) {
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 158.0f, origin.y + 44.0f));
        ImGui::Image(static_cast<void*>(g_logoTextureView.Get()), ImVec2(104.0f, 104.0f));
    }

    const char* title = "TaneClient";
    ImFont* titleFont = g_titleFont != nullptr
        ? g_titleFont
        : (g_launcherFont != nullptr ? g_launcherFont : ImGui::GetFont());
    const float titleFontSize = g_titleFont != nullptr ? 54.0f : (g_launcherFont != nullptr ? 54.0f : 46.0f);
    ImGui::PushFont(titleFont, titleFontSize);
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(
        titleFont,
        titleFontSize,
        ImVec2(std::floor(origin.x + (size.x - titleSize.x) * 0.5f), std::floor(origin.y + 162.0f)),
        white,
        title);
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 110.0f, origin.y + 252.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(25.0f / 255.0f, 25.0f / 255.0f, 25.0f / 255.0f, 1.0f));
    ImGui::PushFont(g_launcherFont, 18.0f);
    if (!buttonEnabled) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Inject", ImVec2(200.0f, 52.0f))) {
        StartInjection();
    }

    if (!buttonEnabled) {
        ImGui::EndDisabled();
    }
    ImGui::PopFont();
    ImGui::PopStyleColor();

    std::string status = WideToUtf8(GetStatus());
    ImGui::PushFont(g_compactFont != nullptr ? g_compactFont : g_launcherFont, 13.0f);
    ImVec2 statusSize = ImGui::CalcTextSize(status.c_str());
    drawList->AddText(
        ImGui::GetFont(),
        13.0f,
        ImVec2(std::floor(origin.x + (size.x - statusSize.x) * 0.5f), std::floor(origin.y + 321.0f)),
        IM_COL32(255, 255, 255, 210),
        status.c_str());
    ImGui::PopFont();

    ImGui::End();
}

void RenderFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderLauncherUi();

    ImGui::Render();
    const float clearColor[4] = {25.0f / 255.0f, 25.0f / 255.0f, 25.0f / 255.0f, 1.0f};
    g_deviceContext->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), nullptr);
    g_deviceContext->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swapChain->Present(1, 0);
}
