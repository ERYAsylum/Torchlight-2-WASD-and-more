#include <windows.h>
#include <string.h>

namespace {

constexpr const char* kModAuthor = "ERY_Asylum";
constexpr bool kLoadKeyboardMoveDll = true;

HMODULE g_realMswsock = nullptr;
HMODULE g_modDll = nullptr;
HMODULE g_renderScaleDll = nullptr;
wchar_t g_proxyDir[MAX_PATH]{};
wchar_t g_logFile[MAX_PATH]{};
volatile LONG g_activeRangePatchApplied = 0;

constexpr DWORD kOriginalImageBase = 0x00400000;
constexpr SIZE_T kActiveRangePatchLength = 11;
const float kActiveRangeFixedOutdoor = 36.0f;
const float kActiveRangeFixedIndoor = 24.0f;

const BYTE kActiveRangeCharacterOutdoorExpected[kActiveRangePatchLength] = {
    0xE8, 0x30, 0x0A, 0x10, 0x00, 0xD9, 0x80, 0xE0, 0x00, 0x00, 0x00};
const BYTE kActiveRangeCharacterIndoorExpected[kActiveRangePatchLength] = {
    0xE8, 0x23, 0x0A, 0x10, 0x00, 0xD9, 0x80, 0xDC, 0x00, 0x00, 0x00};
const BYTE kActiveRangeItemOutdoorExpected[kActiveRangePatchLength] = {
    0xE8, 0x06, 0xA7, 0x09, 0x00, 0xD9, 0x80, 0xE0, 0x00, 0x00, 0x00};
const BYTE kActiveRangeItemIndoorExpected[kActiveRangePatchLength] = {
    0xE8, 0xF9, 0xA6, 0x09, 0x00, 0xD9, 0x80, 0xDC, 0x00, 0x00, 0x00};

struct ActiveRangePatchSite {
    const char* name;
    DWORD staticAddress;
    const BYTE* expected;
    const float* fixedValue;
    const char* fixedLabel;
};

const ActiveRangePatchSite kActiveRangePatchSites[] = {
    {"CCharacter outdoor active range consumer", 0x0054027B, kActiveRangeCharacterOutdoorExpected, &kActiveRangeFixedOutdoor, "36"},
    {"CCharacter indoor active range consumer", 0x00540288, kActiveRangeCharacterIndoorExpected, &kActiveRangeFixedIndoor, "24"},
    {"CItem outdoor active range consumer", 0x005A65A5, kActiveRangeItemOutdoorExpected, &kActiveRangeFixedOutdoor, "36"},
    {"CItem indoor active range consumer", 0x005A65B2, kActiveRangeItemIndoorExpected, &kActiveRangeFixedIndoor, "24"},
};

void BuildProxyDirectory(HMODULE module) {
    const DWORD written = GetModuleFileNameW(module, g_proxyDir, MAX_PATH);
    if (written == 0 || written >= MAX_PATH) {
        g_proxyDir[0] = L'\0';
        return;
    }

    for (DWORD i = written; i > 0; --i) {
        if (g_proxyDir[i - 1] == L'\\' || g_proxyDir[i - 1] == L'/') {
            g_proxyDir[i] = L'\0';
            return;
        }
    }

    g_proxyDir[0] = L'\0';
}

void BuildPath(wchar_t* out, const wchar_t* fileName) {
    wcscpy_s(out, MAX_PATH, g_proxyDir);
    wcscat_s(out, MAX_PATH, fileName);
}

void LogLine(const char* text) {
    HANDLE file = CreateFileW(g_logFile, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

DWORD StaticToRuntime(DWORD staticAddress) {
    const DWORD moduleBase = reinterpret_cast<DWORD>(GetModuleHandleW(nullptr));
    return moduleBase + (staticAddress - kOriginalImageBase);
}

bool ReadBytesSafe(const void* address, BYTE* valueOut, SIZE_T size) {
    __try {
        CopyMemory(valueOut, address, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ZeroMemory(valueOut, size);
        return false;
    }
}

void BuildActiveRangeReplacement(const ActiveRangePatchSite& site, BYTE* replacement) {
    replacement[0] = 0xD9;
    replacement[1] = 0x05;
    const DWORD fixedValueAddress = reinterpret_cast<DWORD>(site.fixedValue);
    CopyMemory(replacement + 2, &fixedValueAddress, sizeof(fixedValueAddress));
    for (SIZE_T i = 6; i < kActiveRangePatchLength; ++i) {
        replacement[i] = 0x90;
    }
}

bool WriteActiveRangeCodeBytes(void* address, const BYTE* bytes, SIZE_T length) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    CopyMemory(address, bytes, length);

    DWORD ignored = 0;
    VirtualProtect(address, length, oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), address, length);
    return true;
}

bool ValidateActiveRangePatchSite(const ActiveRangePatchSite& site) {
    BYTE actual[kActiveRangePatchLength]{};
    if (!ReadBytesSafe(reinterpret_cast<const void*>(StaticToRuntime(site.staticAddress)), actual, sizeof(actual))) {
        char line[256]{};
        wsprintfA(line, "MSWSOCK ActiveRangePatch: unable to read %s; patch refused.", site.name);
        LogLine(line);
        return false;
    }

    if (memcmp(actual, site.expected, sizeof(actual)) != 0) {
        char line[320]{};
        wsprintfA(
            line,
            "MSWSOCK ActiveRangePatch: unexpected bytes at 0x%08X for %s; patch refused.",
            site.staticAddress,
            site.name);
        LogLine(line);
        return false;
    }

    return true;
}

bool ApplyActiveRangePatchSite(const ActiveRangePatchSite& site) {
    BYTE replacement[kActiveRangePatchLength]{};
    BuildActiveRangeReplacement(site, replacement);
    if (!WriteActiveRangeCodeBytes(
            reinterpret_cast<void*>(StaticToRuntime(site.staticAddress)),
            replacement,
            sizeof(replacement))) {
        char line[256]{};
        wsprintfA(line, "MSWSOCK ActiveRangePatch: failed to write %s.", site.name);
        LogLine(line);
        return false;
    }

    char line[320]{};
    wsprintfA(
        line,
        "MSWSOCK ActiveRangePatch: %s now uses fixed range %s.",
        site.name,
        site.fixedLabel);
    LogLine(line);
    return true;
}

void RollbackActiveRangePatchSites(int patchedCount) {
    for (int i = patchedCount - 1; i >= 0; --i) {
        const ActiveRangePatchSite& site = kActiveRangePatchSites[i];
        WriteActiveRangeCodeBytes(
            reinterpret_cast<void*>(StaticToRuntime(site.staticAddress)),
            site.expected,
            kActiveRangePatchLength);
    }
}

bool ApplyActiveRangePatch() {
    if (InterlockedCompareExchange(&g_activeRangePatchApplied, 1, 1) != 0) {
        return true;
    }

    const int siteCount = static_cast<int>(sizeof(kActiveRangePatchSites) / sizeof(kActiveRangePatchSites[0]));
    for (int i = 0; i < siteCount; ++i) {
        if (!ValidateActiveRangePatchSite(kActiveRangePatchSites[i])) {
            return false;
        }
    }

    int patchedCount = 0;
    for (int i = 0; i < siteCount; ++i) {
        if (!ApplyActiveRangePatchSite(kActiveRangePatchSites[i])) {
            RollbackActiveRangePatchSites(patchedCount);
            LogLine("MSWSOCK ActiveRangePatch: rollback completed after write failure.");
            return false;
        }
        ++patchedCount;
    }

    InterlockedExchange(&g_activeRangePatchApplied, 1);
    LogLine("MSWSOCK ActiveRangePatch: x1.5 fixed consumer-code patch applied.");
    return true;
}

void ApplyActiveRangePatchFromIni() {
    wchar_t iniPath[MAX_PATH]{};
    BuildPath(iniPath, L"TL2TrueKeyboardMove.ini");
    const bool enabled =
        GetPrivateProfileIntW(L"EnginePatch", L"EnableActiveRangePatch", 0, iniPath) != 0;
    if (!enabled) {
        LogLine("MSWSOCK ActiveRangePatch: disabled in ini; no engine code patch applied.");
        return;
    }

    if (!ApplyActiveRangePatch()) {
        LogLine("MSWSOCK ActiveRangePatch: startup patch failed; active range remains unmodified.");
    }
}

void LoadRenderScaleFromIni() {
    LogLine("MSWSOCK RenderScale: not included in TL2GlobalMod v0.1 stable; TL2RenderScale.dll not loaded.");
}

bool LoadRealMswsock() {
    if (g_realMswsock != nullptr) {
        return true;
    }

    wchar_t systemDir[MAX_PATH]{};
    if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0) {
        return false;
    }

    wchar_t realPath[MAX_PATH]{};
    wcscpy_s(realPath, systemDir);
    wcscat_s(realPath, L"\\mswsock.dll");

    g_realMswsock = LoadLibraryW(realPath);
    return g_realMswsock != nullptr;
}

extern "C" __declspec(noinline) FARPROC ResolveMswsockProc(const char* name) {
    if (!LoadRealMswsock()) {
        return nullptr;
    }

    return GetProcAddress(g_realMswsock, name);
}

DWORD WINAPI LoadModThread(void*) {
    char line[128]{};
    wsprintfA(line, "MSWSOCK proxy active for TL2TrueKeyboardMove. Author: %s.", kModAuthor);
    LogLine(line);

    if (!kLoadKeyboardMoveDll) {
        LogLine("MSWSOCK proxy smoke test: TL2TrueKeyboardMove.dll loading disabled.");
        return 0;
    }

    Sleep(1000);

    wchar_t modPath[MAX_PATH]{};
    BuildPath(modPath, L"TL2TrueKeyboardMove.dll");
    g_modDll = LoadLibraryW(modPath);
    LogLine(g_modDll != nullptr ? "Loaded TL2TrueKeyboardMove.dll." : "Failed to load TL2TrueKeyboardMove.dll.");
    Sleep(500);
    LoadRenderScaleFromIni();
    Sleep(1500);
    ApplyActiveRangePatchFromIni();
    return 0;
}

void StartModLoaderThread() {
    HANDLE thread = CreateThread(nullptr, 0, LoadModThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

} // namespace

#define MSWSOCK_PROXY_STUB(internalName) \
    extern "C" __declspec(naked) void internalName() { \
        __asm { push offset name_##internalName } \
        __asm { call ResolveMswsockProc } \
        __asm { add esp, 4 } \
        __asm { jmp eax } \
    }

#define MSWSOCK_PROXY_NAME(internalName, realName) extern "C" const char name_##internalName[] = realName;

MSWSOCK_PROXY_NAME(Proxy_AcceptEx, "AcceptEx")
MSWSOCK_PROXY_NAME(Proxy_EnumProtocolsA, "EnumProtocolsA")
MSWSOCK_PROXY_NAME(Proxy_EnumProtocolsW, "EnumProtocolsW")
MSWSOCK_PROXY_NAME(Proxy_GetAcceptExSockaddrs, "GetAcceptExSockaddrs")
MSWSOCK_PROXY_NAME(Proxy_GetAddressByNameA, "GetAddressByNameA")
MSWSOCK_PROXY_NAME(Proxy_GetAddressByNameW, "GetAddressByNameW")
MSWSOCK_PROXY_NAME(Proxy_GetNameByTypeA, "GetNameByTypeA")
MSWSOCK_PROXY_NAME(Proxy_GetNameByTypeW, "GetNameByTypeW")
MSWSOCK_PROXY_NAME(Proxy_GetServiceA, "GetServiceA")
MSWSOCK_PROXY_NAME(Proxy_GetServiceW, "GetServiceW")
MSWSOCK_PROXY_NAME(Proxy_GetSocketErrorMessageW, "GetSocketErrorMessageW")
MSWSOCK_PROXY_NAME(Proxy_GetTypeByNameA, "GetTypeByNameA")
MSWSOCK_PROXY_NAME(Proxy_GetTypeByNameW, "GetTypeByNameW")
MSWSOCK_PROXY_NAME(Proxy_MigrateWinsockConfiguration, "MigrateWinsockConfiguration")
MSWSOCK_PROXY_NAME(Proxy_MigrateWinsockConfigurationEx, "MigrateWinsockConfigurationEx")
MSWSOCK_PROXY_NAME(Proxy_NPLoadNameSpaces, "NPLoadNameSpaces")
MSWSOCK_PROXY_NAME(Proxy_NSPStartup, "NSPStartup")
MSWSOCK_PROXY_NAME(Proxy_SetServiceA, "SetServiceA")
MSWSOCK_PROXY_NAME(Proxy_SetServiceW, "SetServiceW")
MSWSOCK_PROXY_NAME(Proxy_StartWsdpService, "StartWsdpService")
MSWSOCK_PROXY_NAME(Proxy_StopWsdpService, "StopWsdpService")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHAddressToString, "Tcpip4_WSHAddressToString")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHEnumProtocols, "Tcpip4_WSHEnumProtocols")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHGetBroadcastSockaddr, "Tcpip4_WSHGetBroadcastSockaddr")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHGetProviderGuid, "Tcpip4_WSHGetProviderGuid")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHGetSockaddrType, "Tcpip4_WSHGetSockaddrType")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHGetSocketInformation, "Tcpip4_WSHGetSocketInformation")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHGetWSAProtocolInfo, "Tcpip4_WSHGetWSAProtocolInfo")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHGetWildcardSockaddr, "Tcpip4_WSHGetWildcardSockaddr")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHGetWinsockMapping, "Tcpip4_WSHGetWinsockMapping")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHIoctl, "Tcpip4_WSHIoctl")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHJoinLeaf, "Tcpip4_WSHJoinLeaf")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHNotify, "Tcpip4_WSHNotify")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHOpenSocket2, "Tcpip4_WSHOpenSocket2")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHOpenSocket, "Tcpip4_WSHOpenSocket")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHSetSocketInformation, "Tcpip4_WSHSetSocketInformation")
MSWSOCK_PROXY_NAME(Proxy_Tcpip4_WSHStringToAddress, "Tcpip4_WSHStringToAddress")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHAddressToString, "Tcpip6_WSHAddressToString")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHEnumProtocols, "Tcpip6_WSHEnumProtocols")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHGetProviderGuid, "Tcpip6_WSHGetProviderGuid")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHGetSockaddrType, "Tcpip6_WSHGetSockaddrType")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHGetSocketInformation, "Tcpip6_WSHGetSocketInformation")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHGetWSAProtocolInfo, "Tcpip6_WSHGetWSAProtocolInfo")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHGetWildcardSockaddr, "Tcpip6_WSHGetWildcardSockaddr")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHGetWinsockMapping, "Tcpip6_WSHGetWinsockMapping")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHIoctl, "Tcpip6_WSHIoctl")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHJoinLeaf, "Tcpip6_WSHJoinLeaf")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHNotify, "Tcpip6_WSHNotify")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHOpenSocket2, "Tcpip6_WSHOpenSocket2")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHOpenSocket, "Tcpip6_WSHOpenSocket")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHSetSocketInformation, "Tcpip6_WSHSetSocketInformation")
MSWSOCK_PROXY_NAME(Proxy_Tcpip6_WSHStringToAddress, "Tcpip6_WSHStringToAddress")
MSWSOCK_PROXY_NAME(Proxy_TransmitFile, "TransmitFile")
MSWSOCK_PROXY_NAME(Proxy_WSARecvEx, "WSARecvEx")
MSWSOCK_PROXY_NAME(Proxy_WSPStartup, "WSPStartup")
MSWSOCK_PROXY_NAME(Proxy_dn_expand, "dn_expand")
MSWSOCK_PROXY_NAME(Proxy_getnetbyname, "getnetbyname")
MSWSOCK_PROXY_NAME(Proxy_inet_network, "inet_network")
MSWSOCK_PROXY_NAME(Proxy_rcmd, "rcmd")
MSWSOCK_PROXY_NAME(Proxy_rexec, "rexec")
MSWSOCK_PROXY_NAME(Proxy_rresvport, "rresvport")
MSWSOCK_PROXY_NAME(Proxy_s_perror, "s_perror")
MSWSOCK_PROXY_NAME(Proxy_sethostname, "sethostname")

MSWSOCK_PROXY_STUB(Proxy_AcceptEx)
MSWSOCK_PROXY_STUB(Proxy_EnumProtocolsA)
MSWSOCK_PROXY_STUB(Proxy_EnumProtocolsW)
MSWSOCK_PROXY_STUB(Proxy_GetAcceptExSockaddrs)
MSWSOCK_PROXY_STUB(Proxy_GetAddressByNameA)
MSWSOCK_PROXY_STUB(Proxy_GetAddressByNameW)
MSWSOCK_PROXY_STUB(Proxy_GetNameByTypeA)
MSWSOCK_PROXY_STUB(Proxy_GetNameByTypeW)
MSWSOCK_PROXY_STUB(Proxy_GetServiceA)
MSWSOCK_PROXY_STUB(Proxy_GetServiceW)
MSWSOCK_PROXY_STUB(Proxy_GetSocketErrorMessageW)
MSWSOCK_PROXY_STUB(Proxy_GetTypeByNameA)
MSWSOCK_PROXY_STUB(Proxy_GetTypeByNameW)
MSWSOCK_PROXY_STUB(Proxy_MigrateWinsockConfiguration)
MSWSOCK_PROXY_STUB(Proxy_MigrateWinsockConfigurationEx)
MSWSOCK_PROXY_STUB(Proxy_NPLoadNameSpaces)
MSWSOCK_PROXY_STUB(Proxy_NSPStartup)
MSWSOCK_PROXY_STUB(Proxy_SetServiceA)
MSWSOCK_PROXY_STUB(Proxy_SetServiceW)
MSWSOCK_PROXY_STUB(Proxy_StartWsdpService)
MSWSOCK_PROXY_STUB(Proxy_StopWsdpService)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHAddressToString)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHEnumProtocols)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHGetBroadcastSockaddr)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHGetProviderGuid)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHGetSockaddrType)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHGetSocketInformation)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHGetWSAProtocolInfo)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHGetWildcardSockaddr)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHGetWinsockMapping)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHIoctl)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHJoinLeaf)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHNotify)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHOpenSocket2)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHOpenSocket)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHSetSocketInformation)
MSWSOCK_PROXY_STUB(Proxy_Tcpip4_WSHStringToAddress)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHAddressToString)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHEnumProtocols)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHGetProviderGuid)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHGetSockaddrType)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHGetSocketInformation)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHGetWSAProtocolInfo)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHGetWildcardSockaddr)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHGetWinsockMapping)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHIoctl)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHJoinLeaf)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHNotify)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHOpenSocket2)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHOpenSocket)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHSetSocketInformation)
MSWSOCK_PROXY_STUB(Proxy_Tcpip6_WSHStringToAddress)
MSWSOCK_PROXY_STUB(Proxy_TransmitFile)
MSWSOCK_PROXY_STUB(Proxy_WSARecvEx)
MSWSOCK_PROXY_STUB(Proxy_WSPStartup)
MSWSOCK_PROXY_STUB(Proxy_dn_expand)
MSWSOCK_PROXY_STUB(Proxy_getnetbyname)
MSWSOCK_PROXY_STUB(Proxy_inet_network)
MSWSOCK_PROXY_STUB(Proxy_rcmd)
MSWSOCK_PROXY_STUB(Proxy_rexec)
MSWSOCK_PROXY_STUB(Proxy_rresvport)
MSWSOCK_PROXY_STUB(Proxy_s_perror)
MSWSOCK_PROXY_STUB(Proxy_sethostname)

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        BuildProxyDirectory(module);
        BuildPath(g_logFile, L"TL2TrueKeyboardMove.mswsockproxy.log");
        LogLine("MSWSOCK proxy loaded.");
        StartModLoaderThread();
    } else if (reason == DLL_PROCESS_DETACH) {
        LogLine("MSWSOCK proxy unloading.");
    }

    return TRUE;
}
