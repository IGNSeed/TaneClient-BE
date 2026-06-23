using LoadLibraryAFn = HMODULE(WINAPI*)(LPCSTR);
using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
using RtlAddFunctionTableFn = BOOLEAN(WINAPI*)(PRUNTIME_FUNCTION, DWORD, DWORD64);
using DllMainFn = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);

struct ManualMapContext {
    std::uint8_t* imageBase = nullptr;
    LoadLibraryAFn loadLibraryA = nullptr;
    GetProcAddressFn getProcAddress = nullptr;
    RtlAddFunctionTableFn rtlAddFunctionTable = nullptr;
    HMODULE module = nullptr;
    DWORD error = 0;
};

#pragma code_seg(push, ".mmap")
#pragma optimize("", off)
__declspec(noinline) DWORD WINAPI ManualMapLoader(void* parameter) {
    auto* context = reinterpret_cast<ManualMapContext*>(parameter);
    if (context == nullptr || context->imageBase == nullptr || context->loadLibraryA == nullptr ||
        context->getProcAddress == nullptr) {
        return 1;
    }

    std::uint8_t* imageBase = context->imageBase;
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(imageBase);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(imageBase + dosHeader->e_lfanew);
    const ULONGLONG relocationDelta =
        reinterpret_cast<ULONGLONG>(imageBase) - ntHeaders->OptionalHeader.ImageBase;

    if (relocationDelta != 0) {
        const IMAGE_DATA_DIRECTORY relocationDirectory =
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (relocationDirectory.VirtualAddress != 0 && relocationDirectory.Size != 0) {
            auto* relocation = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                imageBase + relocationDirectory.VirtualAddress);
            DWORD processed = 0;
            while (processed < relocationDirectory.Size && relocation->SizeOfBlock != 0) {
                const DWORD entryCount =
                    (relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                auto* entries = reinterpret_cast<WORD*>(relocation + 1);
                for (DWORD i = 0; i < entryCount; ++i) {
                    const WORD type = entries[i] >> 12;
                    const WORD offset = entries[i] & 0x0FFF;
                    if (type == IMAGE_REL_BASED_DIR64) {
                        auto* patch = reinterpret_cast<ULONGLONG*>(
                            imageBase + relocation->VirtualAddress + offset);
                        *patch += relocationDelta;
                    }
                }

                processed += relocation->SizeOfBlock;
                relocation = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                    reinterpret_cast<std::uint8_t*>(relocation) + relocation->SizeOfBlock);
            }
        }
    }

    const IMAGE_DATA_DIRECTORY importDirectory =
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDirectory.VirtualAddress != 0 && importDirectory.Size != 0) {
        auto* importDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            imageBase + importDirectory.VirtualAddress);
        while (importDescriptor->Name != 0) {
            HMODULE importedModule = context->loadLibraryA(
                reinterpret_cast<LPCSTR>(imageBase + importDescriptor->Name));
            if (importedModule == nullptr) {
                context->error = 2;
                return 2;
            }

            auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(
                imageBase + importDescriptor->FirstThunk);
            auto* originalThunk = importDescriptor->OriginalFirstThunk != 0
                ? reinterpret_cast<IMAGE_THUNK_DATA64*>(imageBase + importDescriptor->OriginalFirstThunk)
                : thunk;

            while (originalThunk->u1.AddressOfData != 0) {
                FARPROC proc = nullptr;
                if ((originalThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) != 0) {
                    proc = context->getProcAddress(
                        importedModule,
                        reinterpret_cast<LPCSTR>(originalThunk->u1.Ordinal & 0xFFFF));
                } else {
                    auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        imageBase + originalThunk->u1.AddressOfData);
                    proc = context->getProcAddress(importedModule, importByName->Name);
                }

                if (proc == nullptr) {
                    context->error = 3;
                    return 3;
                }

                thunk->u1.Function = reinterpret_cast<ULONGLONG>(proc);
                ++originalThunk;
                ++thunk;
            }

            ++importDescriptor;
        }
    }

    const IMAGE_DATA_DIRECTORY exceptionDirectory =
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (exceptionDirectory.VirtualAddress != 0 && exceptionDirectory.Size != 0 &&
        context->rtlAddFunctionTable != nullptr) {
        context->rtlAddFunctionTable(
            reinterpret_cast<PRUNTIME_FUNCTION>(imageBase + exceptionDirectory.VirtualAddress),
            exceptionDirectory.Size / sizeof(RUNTIME_FUNCTION),
            reinterpret_cast<DWORD64>(imageBase));
    }

    const IMAGE_DATA_DIRECTORY tlsDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tlsDirectory.VirtualAddress != 0 && tlsDirectory.Size != 0) {
        auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY64*>(imageBase + tlsDirectory.VirtualAddress);
        auto* callbacks = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(tls->AddressOfCallBacks);
        if (callbacks != nullptr) {
            while (*callbacks != nullptr) {
                (*callbacks)(imageBase, DLL_PROCESS_ATTACH, nullptr);
                ++callbacks;
            }
        }
    }

    if (ntHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
        auto dllMain = reinterpret_cast<DllMainFn>(
            imageBase + ntHeaders->OptionalHeader.AddressOfEntryPoint);
        if (!dllMain(
                reinterpret_cast<HINSTANCE>(imageBase),
                DLL_PROCESS_ATTACH,
                nullptr)) {
            context->error = 4;
            return 4;
        }
    }

    context->module = reinterpret_cast<HMODULE>(imageBase);
    context->error = 0;
    return 0;
}

__declspec(noinline) void ManualMapLoaderEnd() {}
#pragma optimize("", on)
#pragma code_seg(pop)

bool IsCloseButtonPoint(POINT point) {
    return point.x >= 372 && point.x <= 406 && point.y >= 14 && point.y <= 48;
}

std::wstring FormatWin32Error(const wchar_t* prefix, DWORD error) {
    wchar_t buffer[512]{};
    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        buffer,
        static_cast<DWORD>(_countof(buffer)),
        nullptr);

    std::wstring result = prefix;
    result += L" (";
    result += std::to_wstring(error);
    result += L") ";
    result += buffer;
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) {
        return {};
    }

    std::string result(static_cast<size_t>(required - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), required, nullptr, nullptr);
    return result;
}

void SetStatus(const std::wstring& text) {
    std::lock_guard<std::mutex> lock(g_statusMutex);
    g_status = text;
}

std::wstring GetStatus() {
    std::lock_guard<std::mutex> lock(g_statusMutex);
    return g_status;
}

void SetButtonEnabled(bool enabled) {
    g_injectButtonEnabled.store(enabled);
}

bool CopyResourceBytes(int resourceId, const wchar_t* type, std::vector<std::uint8_t>& out) {
    HRSRC resource = FindResourceW(g_instance, MAKEINTRESOURCEW(resourceId), type);
    if (resource == nullptr) {
        return false;
    }

    HGLOBAL loaded = LoadResource(g_instance, resource);
    if (loaded == nullptr) {
        return false;
    }

    auto* data = static_cast<const std::uint8_t*>(LockResource(loaded));
    DWORD size = SizeofResource(g_instance, resource);
    if (data == nullptr || size == 0) {
        return false;
    }

    out.assign(data, data + size);
    return true;
}

bool EnableDebugPrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &privileges.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }

    AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr);
    const bool ok = GetLastError() == ERROR_SUCCESS;
    CloseHandle(token);
    return ok;
}

DWORD FindProcessId(const wchar_t* processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processId;
}

std::vector<DWORD> FindProcessIds(const wchar_t* processName) {
    std::vector<DWORD> processIds;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return processIds;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                processIds.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processIds;
}

DWORD WaitForProcess(const wchar_t* processName, DWORD timeoutMs) {
    const ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < timeoutMs) {
        DWORD processId = FindProcessId(processName);
        if (processId != 0) {
            return processId;
        }
        Sleep(500);
    }
    return 0;
}

std::wstring GetPayloadInstanceEventName(DWORD processId) {
    std::wstring name = L"Local\\TaneClientPayload_";
    name += std::to_wstring(processId);
    return name;
}

bool IsPayloadAlreadyInjected(DWORD processId) {
    const std::wstring eventName = GetPayloadInstanceEventName(processId);
    HANDLE eventHandle = OpenEventW(SYNCHRONIZE, FALSE, eventName.c_str());
    if (eventHandle == nullptr) {
        return false;
    }

    CloseHandle(eventHandle);
    return true;
}

bool LaunchMinecraft(std::wstring& error) {
    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        L"shell:AppsFolder\\Microsoft.MinecraftUWP_8wekyb3d8bbwe!Game",
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32) {
        return true;
    }

    error = L"Official Minecraft for Windows could not be launched. Install it or start it manually.";
    return false;
}

DWORD FindInjectableProcessId() {
    for (DWORD processId : FindProcessIds(kTargetProcess)) {
        if (!IsPayloadAlreadyInjected(processId)) {
            return processId;
        }
    }

    return 0;
}

bool HasMinecraftProcess() {
    return FindProcessId(kTargetProcess) != 0;
}

DWORD WaitForInjectableProcess(DWORD timeoutMs) {
    const ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < timeoutMs) {
        DWORD processId = FindInjectableProcessId();
        if (processId != 0) {
            return processId;
        }
        Sleep(500);
    }
    return 0;
}

bool GetPayloadResourceView(const std::uint8_t*& payloadData, DWORD& payloadSize, std::wstring& error) {
    HRSRC resource = FindResourceW(g_instance, MAKEINTRESOURCEW(kResourcePayload), RT_RCDATA);
    if (resource == nullptr) {
        error = FormatWin32Error(L"Payload resource was not found", GetLastError());
        return false;
    }

    HGLOBAL loadedResource = LoadResource(g_instance, resource);
    if (loadedResource == nullptr) {
        error = FormatWin32Error(L"Payload resource could not be loaded", GetLastError());
        return false;
    }

    payloadData = static_cast<const std::uint8_t*>(LockResource(loadedResource));
    payloadSize = SizeofResource(g_instance, resource);
    if (payloadData == nullptr || payloadSize == 0) {
        error = L"Payload resource is empty.";
        return false;
    }

    return true;
}

bool InjectPayloadFromMemory(DWORD processId, std::wstring& error) {
    const std::uint8_t* payloadData = nullptr;
    DWORD payloadSize = 0;
    if (!GetPayloadResourceView(payloadData, payloadSize, error)) {
        return false;
    }

    if (payloadSize < sizeof(IMAGE_DOS_HEADER)) {
        error = L"Payload image is too small.";
        return false;
    }

    auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(payloadData);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
        dosHeader->e_lfanew <= 0 ||
        static_cast<DWORD>(dosHeader->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > payloadSize) {
        error = L"Payload image has an invalid DOS header.";
        return false;
    }

    auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(payloadData + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
        ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        error = L"Payload image is not a valid x64 PE image.";
        return false;
    }

    const SIZE_T imageSize = ntHeaders->OptionalHeader.SizeOfImage;
    const SIZE_T headersSize = ntHeaders->OptionalHeader.SizeOfHeaders;
    if (imageSize == 0 || headersSize == 0 || headersSize > payloadSize) {
        error = L"Payload image has invalid PE sizes.";
        return false;
    }

    const auto loaderStart = reinterpret_cast<std::uintptr_t>(&ManualMapLoader);
    const auto loaderEnd = reinterpret_cast<std::uintptr_t>(&ManualMapLoaderEnd);
    if (loaderEnd <= loaderStart || loaderEnd - loaderStart > 0x4000) {
        error = L"Manual mapper stub size is invalid.";
        return false;
    }
    const SIZE_T loaderSize = loaderEnd - loaderStart;

    EnableDebugPrivilege();
    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        processId);
    if (process == nullptr) {
        error = FormatWin32Error(L"OpenProcess failed", GetLastError());
        return false;
    }

    void* preferredBase = reinterpret_cast<void*>(ntHeaders->OptionalHeader.ImageBase);
    void* remoteImage = VirtualAllocEx(
        process,
        preferredBase,
        imageSize,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    if (remoteImage == nullptr) {
        remoteImage = VirtualAllocEx(
            process,
            nullptr,
            imageSize,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_EXECUTE_READWRITE);
    }
    if (remoteImage == nullptr) {
        error = FormatWin32Error(L"Remote image allocation failed", GetLastError());
        CloseHandle(process);
        return false;
    }

    bool ok = WriteProcessMemory(process, remoteImage, payloadData, headersSize, nullptr) != FALSE;
    auto* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; ok && i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
        if (section->SizeOfRawData == 0) {
            continue;
        }

        const DWORD rawEnd = section->PointerToRawData + section->SizeOfRawData;
        if (section->PointerToRawData >= payloadSize || rawEnd > payloadSize) {
            ok = false;
            break;
        }

        void* remoteSection = static_cast<std::uint8_t*>(remoteImage) + section->VirtualAddress;
        ok = WriteProcessMemory(
            process,
            remoteSection,
            payloadData + section->PointerToRawData,
            section->SizeOfRawData,
            nullptr) != FALSE;
    }

    if (!ok) {
        error = FormatWin32Error(L"Payload image copy failed", GetLastError());
        VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    ManualMapContext context{};
    context.imageBase = static_cast<std::uint8_t*>(remoteImage);
    context.loadLibraryA = reinterpret_cast<LoadLibraryAFn>(GetProcAddress(kernel32, "LoadLibraryA"));
    context.getProcAddress = reinterpret_cast<GetProcAddressFn>(GetProcAddress(kernel32, "GetProcAddress"));
    context.rtlAddFunctionTable =
        reinterpret_cast<RtlAddFunctionTableFn>(GetProcAddress(ntdll, "RtlAddFunctionTable"));

    if (context.loadLibraryA == nullptr || context.getProcAddress == nullptr) {
        error = L"Manual mapper required API address was not found.";
        VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    void* remoteContext = VirtualAllocEx(
        process,
        nullptr,
        sizeof(context),
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);
    void* remoteLoader = VirtualAllocEx(
        process,
        nullptr,
        loaderSize,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    if (remoteContext == nullptr || remoteLoader == nullptr) {
        error = FormatWin32Error(L"Manual mapper allocation failed", GetLastError());
        if (remoteContext != nullptr) {
            VirtualFreeEx(process, remoteContext, 0, MEM_RELEASE);
        }
        if (remoteLoader != nullptr) {
            VirtualFreeEx(process, remoteLoader, 0, MEM_RELEASE);
        }
        VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    ok = WriteProcessMemory(process, remoteContext, &context, sizeof(context), nullptr) != FALSE &&
        WriteProcessMemory(process, remoteLoader, reinterpret_cast<void*>(loaderStart), loaderSize, nullptr) != FALSE;
    if (!ok) {
        error = FormatWin32Error(L"Manual mapper copy failed", GetLastError());
        VirtualFreeEx(process, remoteLoader, 0, MEM_RELEASE);
        VirtualFreeEx(process, remoteContext, 0, MEM_RELEASE);
        VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE thread = CreateRemoteThread(
        process,
        nullptr,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteLoader),
        remoteContext,
        0,
        nullptr);
    if (thread == nullptr) {
        error = FormatWin32Error(L"Manual mapper thread creation failed", GetLastError());
        VirtualFreeEx(process, remoteLoader, 0, MEM_RELEASE);
        VirtualFreeEx(process, remoteContext, 0, MEM_RELEASE);
        VirtualFreeEx(process, remoteImage, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    DWORD wait = WaitForSingleObject(thread, 15000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);

    ManualMapContext remoteResult{};
    ReadProcessMemory(process, remoteContext, &remoteResult, sizeof(remoteResult), nullptr);

    VirtualFreeEx(process, remoteLoader, 0, MEM_RELEASE);
    VirtualFreeEx(process, remoteContext, 0, MEM_RELEASE);
    CloseHandle(process);

    if (wait != WAIT_OBJECT_0 || exitCode != 0 || remoteResult.module == nullptr || remoteResult.error != 0) {
        error = L"Manual payload mapping failed.";
        return false;
    }

    return true;
}

DWORD WINAPI InjectWorker(LPVOID) {
    SetStatus(L"Preparing v26.21 payload...");
    std::wstring error;

    DWORD processId = FindInjectableProcessId();
    if (processId == 0) {
        if (HasMinecraftProcess()) {
            SetStatus(L"Already injected.");
            SetButtonEnabled(true);
            return 0;
        }

        SetStatus(L"Launching Minecraft...");
        if (!LaunchMinecraft(error)) {
            SetStatus(error);
            SetButtonEnabled(true);
            return 0;
        }

        SetStatus(L"Waiting for Minecraft.Windows.exe...");
        processId = WaitForInjectableProcess(60000);
        if (processId == 0) {
            SetStatus(L"Minecraft.Windows.exe was not found within 60 seconds.");
            SetButtonEnabled(true);
            return 0;
        }
    } else {
        SetStatus(L"Using running Minecraft.Windows.exe...");
    }

    if (IsPayloadAlreadyInjected(processId)) {
        SetStatus(L"Already injected.");
        SetButtonEnabled(true);
        return 0;
    }

    Sleep(3000);
    SetStatus(L"Injecting...");
    if (!InjectPayloadFromMemory(processId, error)) {
        SetStatus(error);
        SetButtonEnabled(true);
        return 0;
    }

    SetStatus(L"Injected.");
    SetButtonEnabled(true);
    return 0;
}

void StartInjection() {
    bool expected = true;
    if (!g_injectButtonEnabled.compare_exchange_strong(expected, false)) {
        return;
    }

    HANDLE thread = CreateThread(nullptr, 0, InjectWorker, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    } else {
        SetButtonEnabled(true);
    }
}
