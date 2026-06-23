#include <Windows.h>
#include <MinHook.h>

#include <cstddef>
#include <cstdio>
#include <cwchar>

namespace tane::payload {

bool InstallD3D11Hook(HMODULE module);
void RemoveD3D11Hook();

}  // namespace tane::payload

namespace tane::patch {

bool InstallForceCloseOreUiHooks();

}  // namespace tane::patch

namespace {

HANDLE g_singleInstanceEvent = nullptr;

void BuildSingleInstanceEventName(wchar_t* buffer, std::size_t bufferCount) {
    if (buffer == nullptr || bufferCount == 0) {
        return;
    }

    std::swprintf(buffer, bufferCount, L"Local\\TaneClientPayload_%lu", GetCurrentProcessId());
}

bool AcquireSingleInstance() {
    wchar_t eventName[96]{};
    BuildSingleInstanceEventName(eventName, sizeof(eventName) / sizeof(eventName[0]));

    SECURITY_DESCRIPTOR securityDescriptor{};
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.lpSecurityDescriptor = &securityDescriptor;

    if (!InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(&securityDescriptor, TRUE, nullptr, FALSE)) {
        securityAttributes.lpSecurityDescriptor = nullptr;
    }

    g_singleInstanceEvent = CreateEventW(&securityAttributes, TRUE, TRUE, eventName);
    if (g_singleInstanceEvent == nullptr) {
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_singleInstanceEvent);
        g_singleInstanceEvent = nullptr;
        return false;
    }

    return true;
}

void ReleaseSingleInstance() {
    if (g_singleInstanceEvent != nullptr) {
        CloseHandle(g_singleInstanceEvent);
        g_singleInstanceEvent = nullptr;
    }
}

DWORD WINAPI MainThread(LPVOID param) {
    auto module = static_cast<HMODULE>(param);

    const MH_STATUS initializeStatus = MH_Initialize();
    if (initializeStatus == MH_OK || initializeStatus == MH_ERROR_ALREADY_INITIALIZED) {
        tane::patch::InstallForceCloseOreUiHooks();
    }

    tane::payload::InstallD3D11Hook(module);
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (!AcquireSingleInstance()) {
            return TRUE;
        }

        HANDLE thread = CreateThread(nullptr, 0, MainThread, module, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        } else {
            ReleaseSingleInstance();
            return FALSE;
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_singleInstanceEvent != nullptr) {
            tane::payload::RemoveD3D11Hook();
            ReleaseSingleInstance();
        }
    }

    return TRUE;
}
