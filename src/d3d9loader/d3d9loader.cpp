#include <windows.h>
#include <Xinput.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <shlobj.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr const char* kAddonName = "TL2TrueControlerSupport";
constexpr const char* kAddonVersion = "v0.0.9";
constexpr const wchar_t* kSupportIniName = L"TL2TrueControlerSupport.ini";
constexpr const wchar_t* kMovementIniName = L"TL2TrueKeyboardMove.ini";
constexpr const wchar_t* kTorchlightExeName = L"Torchlight2.exe";
constexpr const wchar_t* kConfigMutexName = L"Local\\TL2TrueControlerSupport_ConfigActive";
constexpr const char* kOverlayClassName = "TL2TrueKeyboardMoveOverlay";
constexpr const char* kOverlayVisibleProp = "TL2TrueKeyboardMove.D3D9OverlayVisible";
constexpr DWORD kConfigPollMs = 500;
constexpr int kMaxChordBindings = 128;
constexpr int kDefaultLeftDeadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
constexpr int kDefaultRightDeadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

using XInputGetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);

enum DirectionBit {
    kDirUp = 1,
    kDirLeft = 2,
    kDirDown = 4,
    kDirRight = 8,
};

enum class ActionKind {
    None,
    Key,
    MouseLeft,
    MouseRight,
    MouseMiddle,
    MouseX1,
    MouseX2,
    ToggleRightStickMouseMode,
};

struct ActionBinding {
    ActionKind kind = ActionKind::None;
    WORD vk = 0;
    WORD modifierVk = 0;
};

struct ButtonBinding {
    WORD xinputButton = 0;
    const wchar_t* iniName = nullptr;
    ActionBinding action{};
    bool ownedDown = false;
};

struct TriggerBinding {
    const wchar_t* iniName = nullptr;
    ActionBinding action{};
    bool ownedDown = false;
};

struct ChordBinding {
    wchar_t iniName[96]{};
    WORD requiredButtons = 0;
    bool requiredLeftTrigger = false;
    bool requiredRightTrigger = false;
    ActionBinding action{};
    bool ownedDown = false;
};

struct ChordConsumption {
    WORD buttons = 0;
    bool leftTrigger = false;
    bool rightTrigger = false;
};

struct StickLatch {
    bool up = false;
    bool left = false;
    bool down = false;
    bool right = false;
};

struct MovementKeys {
    WORD up = 'W';
    WORD left = 'A';
    WORD down = 'S';
    WORD right = 'D';
};

struct Config {
    bool enable = true;
    bool focusRequired = true;
    bool suspendWhileOverlayVisible = true;
    bool acquireControllerExclusive = true;
    bool enablePhysicalOverride = true;
    bool enablePanicCombo = true;
    bool leftStickEnabled = true;
    bool rightStickMouse = true;
    bool followKeyboardProfile = true;
    DWORD controllerIndex = 0;
    DWORD pollIntervalMs = 8;
    DWORD physicalOverrideMs = 1200;
    DWORD ignorePhysicalAfterGamepadMs = 350;
    DWORD panicComboSuspendMs = 2500;
    int leftDeadzone = kDefaultLeftDeadzone;
    int rightDeadzone = kDefaultRightDeadzone;
    int activationThresholdX1000 = 350;
    int releaseThresholdX1000 = 250;
    int mouseSensitivityX = 14;
    int mouseSensitivityY = 14;
    int radialMouseRadius = 140;
    int radialMouseCenterOffsetX = 0;
    int radialMouseCenterOffsetY = 0;
    bool radialReturnToCenter = false;
    bool radialUseFixedRadius = true;
    int radialActivationThresholdX1000 = 450;
    int radialReleaseThresholdX1000 = 300;
    bool invertMouseY = false;
    BYTE triggerThreshold = 30;
    MovementKeys movement{};
};

HMODULE g_module = nullptr;
HMODULE g_realD3D9 = nullptr;
HMODULE g_xinputModule = nullptr;
XInputGetStateFn g_xinputGetState = nullptr;
IDirectInput8W* g_directInput = nullptr;
IDirectInputDevice8W* g_exclusiveDevice = nullptr;
HWND g_exclusiveWindow = nullptr;
HHOOK g_keyboardHook = nullptr;
HHOOK g_mouseHook = nullptr;
wchar_t g_proxyDir[MAX_PATH]{};
wchar_t g_supportIniFile[MAX_PATH]{};
wchar_t g_movementIniFile[MAX_PATH]{};
wchar_t g_keyBindingsFile[MAX_PATH]{};
wchar_t g_settingsFile[MAX_PATH]{};
wchar_t g_logFile[MAX_PATH]{};
volatile LONG g_running = 1;
volatile LONG g_supportThreadStarted = 0;
volatile LONG g_manualOverrideUntilTick = 0;
volatile LONG g_ignorePhysicalInputUntilTick = 0;
volatile LONG g_controllerEnabledInThisProcess = 0;
Config g_config{};
FILETIME g_lastSupportIniWriteTime{};
FILETIME g_lastMovementIniWriteTime{};
FILETIME g_lastKeyBindingsWriteTime{};
FILETIME g_lastSettingsWriteTime{};
DWORD g_lastConfigPollTick = 0;
DWORD g_lastExclusivePollTick = 0;
int g_ownedMovementMask = 0;
StickLatch g_leftLatch{};
bool g_wasConnected = false;
bool g_loggedNoFocus = false;
bool g_loggedManualOverride = false;
bool g_loggedConfigToolPause = false;
bool g_loggedExclusiveAcquired = false;
bool g_loggedExclusiveFailed = false;
bool g_radialMouseMode = false;
bool g_radialAimActive = false;
float g_radialLastX = 1.0f;
float g_radialLastY = 0.0f;

ButtonBinding g_buttons[] = {
    {XINPUT_GAMEPAD_A, L"A"},
    {XINPUT_GAMEPAD_B, L"B"},
    {XINPUT_GAMEPAD_X, L"X"},
    {XINPUT_GAMEPAD_Y, L"Y"},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, L"LeftShoulder"},
    {XINPUT_GAMEPAD_RIGHT_SHOULDER, L"RightShoulder"},
    {XINPUT_GAMEPAD_BACK, L"Back"},
    {XINPUT_GAMEPAD_START, L"Start"},
    {XINPUT_GAMEPAD_LEFT_THUMB, L"LeftThumb"},
    {XINPUT_GAMEPAD_RIGHT_THUMB, L"RightThumb"},
    {XINPUT_GAMEPAD_DPAD_UP, L"DPadUp"},
    {XINPUT_GAMEPAD_DPAD_RIGHT, L"DPadRight"},
    {XINPUT_GAMEPAD_DPAD_DOWN, L"DPadDown"},
    {XINPUT_GAMEPAD_DPAD_LEFT, L"DPadLeft"},
};

TriggerBinding g_leftTrigger{L"LeftTrigger"};
TriggerBinding g_rightTrigger{L"RightTrigger"};
ChordBinding g_chords[kMaxChordBindings]{};
int g_chordCount = 0;

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
    HANDLE file = CreateFileW(
        g_logFile,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

LONG ClampLong(LONG value, LONG minimum, LONG maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

bool QueryWriteTime(const wchar_t* path, FILETIME* writeTimeOut) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (path == nullptr || writeTimeOut == nullptr ||
        !GetFileAttributesExW(path, GetFileExInfoStandard, &data)) {
        return false;
    }
    *writeTimeOut = data.ftLastWriteTime;
    return true;
}

bool PathIsAbsolute(const wchar_t* path) {
    if (path == nullptr || path[0] == L'\0') return false;
    if (path[1] == L':') return true;
    return path[0] == L'\\' && path[1] == L'\\';
}

void ResolveConfiguredPath(
    const wchar_t* section,
    const wchar_t* key,
    const wchar_t* defaultValue,
    wchar_t* outPath) {
    wchar_t value[MAX_PATH]{};
    GetPrivateProfileStringW(section, key, defaultValue, value, MAX_PATH, g_supportIniFile);
    if (value[0] == L'\0') {
        outPath[0] = L'\0';
        return;
    }
    if (PathIsAbsolute(value)) {
        wcscpy_s(outPath, MAX_PATH, value);
    } else {
        BuildPath(outPath, value);
    }
}

bool IsAutoValue(const wchar_t* value) {
    return value == nullptr ||
        value[0] == L'\0' ||
        _wcsicmp(value, L"AUTO") == 0 ||
        _wcsicmp(value, L"DEFAULT") == 0;
}

void BuildTorchlightDocumentsPath(const wchar_t* tail, wchar_t* outPath) {
    outPath[0] = L'\0';
    wchar_t documents[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents) != S_OK ||
        documents[0] == L'\0') {
        return;
    }
    wcscpy_s(outPath, MAX_PATH, documents);
    wcscat_s(outPath, MAX_PATH, L"\\My Games\\runic games\\torchlight 2");
    if (tail != nullptr && tail[0] != L'\0') wcscat_s(outPath, MAX_PATH, tail);
}

void LogWidePath(const char* prefix, const wchar_t* path) {
    char line[1024]{};
    char utf8Path[900]{};
    WideCharToMultiByte(CP_UTF8, 0, path != nullptr ? path : L"", -1, utf8Path, sizeof(utf8Path), nullptr, nullptr);
    wsprintfA(line, "%s%s", prefix, utf8Path);
    LogLine(line);
}

void ResolveKeyBindingsFile() {
    wchar_t configured[MAX_PATH]{};
    GetPrivateProfileStringW(
        L"General",
        L"KeyBindingsDat",
        L"AUTO",
        configured,
        MAX_PATH,
        g_supportIniFile);
    if (!IsAutoValue(configured)) {
        if (PathIsAbsolute(configured)) {
            wcscpy_s(g_keyBindingsFile, configured);
        } else {
            BuildPath(g_keyBindingsFile, configured);
        }
        return;
    }

    g_keyBindingsFile[0] = L'\0';

    BuildTorchlightDocumentsPath(L"\\KeyBindings.dat", g_keyBindingsFile);
    FILETIME newest{};
    if (QueryWriteTime(g_keyBindingsFile, &newest)) {
        LogWidePath("Using root KeyBindings.dat: ", g_keyBindingsFile);
        return;
    }

    wchar_t documents[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents) != S_OK ||
        documents[0] == L'\0') {
        g_keyBindingsFile[0] = L'\0';
        return;
    }

    wchar_t saveRoot[MAX_PATH]{};
    wcscpy_s(saveRoot, documents);
    wcscat_s(saveRoot, L"\\My Games\\runic games\\torchlight 2\\save\\*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(saveRoot, &data);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            lstrcmpW(data.cFileName, L".") == 0 ||
            lstrcmpW(data.cFileName, L"..") == 0) {
            continue;
        }

        wchar_t candidate[MAX_PATH]{};
        wcscpy_s(candidate, documents);
        wcscat_s(candidate, L"\\My Games\\runic games\\torchlight 2\\save\\");
        wcscat_s(candidate, data.cFileName);
        wcscat_s(candidate, L"\\KeyBindings.dat");

        FILETIME writeTime{};
        if (QueryWriteTime(candidate, &writeTime) &&
            (g_keyBindingsFile[0] == L'\0' || CompareFileTime(&writeTime, &newest) > 0)) {
            newest = writeTime;
            wcscpy_s(g_keyBindingsFile, candidate);
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);

    if (g_keyBindingsFile[0] != L'\0') {
        LogWidePath("Using save KeyBindings.dat: ", g_keyBindingsFile);
    } else {
        LogLine("No KeyBindings.dat found.");
    }
}

void ResolveSettingsFile() {
    g_settingsFile[0] = L'\0';
    BuildTorchlightDocumentsPath(L"\\settings.txt", g_settingsFile);
    FILETIME writeTime{};
    if (QueryWriteTime(g_settingsFile, &writeTime)) {
        LogWidePath("Using root settings.txt: ", g_settingsFile);
        return;
    }

    wchar_t documents[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents) != S_OK ||
        documents[0] == L'\0') {
        g_settingsFile[0] = L'\0';
        return;
    }

    wchar_t saveRoot[MAX_PATH]{};
    wcscpy_s(saveRoot, documents);
    wcscat_s(saveRoot, L"\\My Games\\runic games\\torchlight 2\\save\\*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(saveRoot, &data);
    if (find == INVALID_HANDLE_VALUE) {
        g_settingsFile[0] = L'\0';
        return;
    }

    FILETIME newest{};
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            lstrcmpW(data.cFileName, L".") == 0 ||
            lstrcmpW(data.cFileName, L"..") == 0) {
            continue;
        }

        wchar_t candidate[MAX_PATH]{};
        wcscpy_s(candidate, documents);
        wcscat_s(candidate, L"\\My Games\\runic games\\torchlight 2\\save\\");
        wcscat_s(candidate, data.cFileName);
        wcscat_s(candidate, L"\\settings.txt");

        FILETIME candidateTime{};
        if (QueryWriteTime(candidate, &candidateTime) &&
            (g_settingsFile[0] == L'\0' || CompareFileTime(&candidateTime, &newest) > 0)) {
            newest = candidateTime;
            wcscpy_s(g_settingsFile, candidate);
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);

    if (g_settingsFile[0] != L'\0') {
        LogWidePath("Using save settings.txt: ", g_settingsFile);
    } else {
        LogLine("No settings.txt found.");
    }
}

bool FileTimesEqual(const FILETIME& left, const FILETIME& right) {
    return left.dwLowDateTime == right.dwLowDateTime &&
        left.dwHighDateTime == right.dwHighDateTime;
}

const wchar_t* BaseName(const wchar_t* path) {
    const wchar_t* base = path;
    if (path == nullptr) return L"";
    for (const wchar_t* cursor = path; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'\\' || *cursor == L'/') base = cursor + 1;
    }
    return base;
}

bool CurrentProcessIsTorchlight2() {
    wchar_t processPath[MAX_PATH]{};
    const DWORD written = GetModuleFileNameW(nullptr, processPath, MAX_PATH);
    if (written == 0 || written >= MAX_PATH) return false;
    return _wcsicmp(BaseName(processPath), kTorchlightExeName) == 0;
}

HWND ForegroundWindowForCurrentProcess() {
    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) return nullptr;
    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    return processId == GetCurrentProcessId() ? foreground : nullptr;
}

bool IsExtendedKey(WORD vk) {
    switch (vk) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_RCONTROL:
        case VK_RMENU:
            return true;
        default:
            return false;
    }
}

void SendKey(WORD vk, bool down) {
    if (vk == 0) return;
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (IsExtendedKey(vk)) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &input, sizeof(input));
}

void SendMouseButton(ActionKind kind, bool down) {
    DWORD flags = 0;
    if (kind == ActionKind::MouseLeft) flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    if (kind == ActionKind::MouseRight) flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    if (kind == ActionKind::MouseMiddle) flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    if (kind == ActionKind::MouseX1 || kind == ActionKind::MouseX2) flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    if (flags == 0) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    if (kind == ActionKind::MouseX1) input.mi.mouseData = XBUTTON1;
    if (kind == ActionKind::MouseX2) input.mi.mouseData = XBUTTON2;
    SendInput(1, &input, sizeof(input));
}

void ApplyAction(const ActionBinding& action, bool down) {
    if (action.kind == ActionKind::ToggleRightStickMouseMode) {
        if (down) {
            g_radialMouseMode = !g_radialMouseMode;
            g_radialAimActive = false;
            LogLine(g_radialMouseMode
                ? "Right stick mouse mode switched to radial aim."
                : "Right stick mouse mode switched to relative cursor.");
        }
        return;
    }

    if (action.kind == ActionKind::Key) {
        if (down) {
            if (action.modifierVk != 0) SendKey(action.modifierVk, true);
            SendKey(action.vk, true);
        } else {
            SendKey(action.vk, false);
            if (action.modifierVk != 0) SendKey(action.modifierVk, false);
        }
    }
    if (action.kind == ActionKind::MouseLeft ||
        action.kind == ActionKind::MouseRight ||
        action.kind == ActionKind::MouseMiddle ||
        action.kind == ActionKind::MouseX1 ||
        action.kind == ActionKind::MouseX2) {
        SendMouseButton(action.kind, down);
    }
}

void ReleaseAllButtons() {
    for (ButtonBinding& button : g_buttons) {
        if (button.ownedDown) {
            ApplyAction(button.action, false);
            button.ownedDown = false;
        }
    }
    if (g_leftTrigger.ownedDown) {
        ApplyAction(g_leftTrigger.action, false);
        g_leftTrigger.ownedDown = false;
    }
    if (g_rightTrigger.ownedDown) {
        ApplyAction(g_rightTrigger.action, false);
        g_rightTrigger.ownedDown = false;
    }
    for (int i = 0; i < g_chordCount; ++i) {
        if (g_chords[i].ownedDown) {
            ApplyAction(g_chords[i].action, false);
            g_chords[i].ownedDown = false;
        }
    }
}

void ApplyMovementKey(int bit, WORD vk, int newMask) {
    const bool wasDown = (g_ownedMovementMask & bit) != 0;
    const bool isDown = (newMask & bit) != 0;
    if (wasDown != isDown) SendKey(vk, isDown);
}

void ApplyMovementMask(int newMask) {
    ApplyMovementKey(kDirUp, g_config.movement.up, newMask);
    ApplyMovementKey(kDirLeft, g_config.movement.left, newMask);
    ApplyMovementKey(kDirDown, g_config.movement.down, newMask);
    ApplyMovementKey(kDirRight, g_config.movement.right, newMask);
    g_ownedMovementMask = newMask;
}

void ReleaseAllOwnedInput() {
    ApplyMovementMask(0);
    g_leftLatch = {};
    ReleaseAllButtons();
}

WORD ParseVirtualKeyName(const wchar_t* text, WORD fallback) {
    if (text == nullptr || text[0] == L'\0') return fallback;
    wchar_t key[48]{};
    lstrcpynW(key, text, 48);
    CharUpperBuffW(key, lstrlenW(key));
    if (lstrlenW(key) == 1) {
        const wchar_t c = key[0];
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
            return static_cast<WORD>(c);
        }
    }
    if (lstrcmpW(key, L"UP") == 0) return VK_UP;
    if (lstrcmpW(key, L"DOWN") == 0) return VK_DOWN;
    if (lstrcmpW(key, L"LEFT") == 0) return VK_LEFT;
    if (lstrcmpW(key, L"RIGHT") == 0) return VK_RIGHT;
    if (lstrcmpW(key, L"SPACE") == 0) return VK_SPACE;
    if (lstrcmpW(key, L"TAB") == 0) return VK_TAB;
    if (lstrcmpW(key, L"ESC") == 0 || lstrcmpW(key, L"ESCAPE") == 0) return VK_ESCAPE;
    if (lstrcmpW(key, L"ENTER") == 0 || lstrcmpW(key, L"RETURN") == 0) return VK_RETURN;
    if (lstrcmpW(key, L"SHIFT") == 0) return VK_SHIFT;
    if (lstrcmpW(key, L"LSHIFT") == 0 || lstrcmpW(key, L"LEFTSHIFT") == 0) return VK_LSHIFT;
    if (lstrcmpW(key, L"RSHIFT") == 0 || lstrcmpW(key, L"RIGHTSHIFT") == 0) return VK_RSHIFT;
    if (lstrcmpW(key, L"CTRL") == 0 || lstrcmpW(key, L"CONTROL") == 0) return VK_CONTROL;
    if (lstrcmpW(key, L"LCTRL") == 0 || lstrcmpW(key, L"LCONTROL") == 0) return VK_LCONTROL;
    if (lstrcmpW(key, L"RCTRL") == 0 || lstrcmpW(key, L"RCONTROL") == 0) return VK_RCONTROL;
    if (lstrcmpW(key, L"ALT") == 0) return VK_MENU;
    if (lstrcmpW(key, L"DELETE") == 0 || lstrcmpW(key, L"DEL") == 0) return VK_DELETE;
    if (lstrcmpW(key, L"END") == 0) return VK_END;
    if (lstrcmpW(key, L"HOME") == 0) return VK_HOME;
    if (key[0] == L'F') {
        const int fNumber = _wtoi(key + 1);
        if (fNumber >= 1 && fNumber <= 12) return static_cast<WORD>(VK_F1 + fNumber - 1);
    }
    if (key[0] == L'V' && key[1] == L'K') {
        const int parsed = static_cast<int>(wcstol(key + 2, nullptr, 16));
        if (parsed > 0 && parsed <= 0xFF) return static_cast<WORD>(parsed);
    }
    if (key[0] >= L'0' && key[0] <= L'9') {
        const int parsed = _wtoi(key);
        if (parsed > 0 && parsed <= 0xFF) return static_cast<WORD>(parsed);
    }
    return fallback;
}

WORD ModifierVirtualKeyFromGameValue(int modifierKey) {
    if (modifierKey < 0) return 0;
    if (modifierKey == VK_SHIFT || modifierKey == VK_CONTROL || modifierKey == VK_MENU) {
        return static_cast<WORD>(modifierKey);
    }
    return ParseVirtualKeyName(modifierKey == 16 ? L"SHIFT" : modifierKey == 17 ? L"CTRL" : modifierKey == 18 ? L"ALT" : L"", 0);
}

bool ActionFromGameKeyValue(int key, int modifier, ActionBinding* actionOut) {
    if (actionOut == nullptr) return false;
    ActionBinding action{};
    if (key > 0 && key <= 0xFF) {
        action.kind = ActionKind::Key;
        action.vk = static_cast<WORD>(key);
        action.modifierVk = ModifierVirtualKeyFromGameValue(modifier);
        *actionOut = action;
        return true;
    }

    if (key == 999) action.kind = ActionKind::MouseLeft;
    else if (key == 1000) action.kind = ActionKind::MouseRight;
    else if (key == 1001) action.kind = ActionKind::MouseMiddle;
    else if (key == 1002) action.kind = ActionKind::MouseX1;
    else if (key == 1003) action.kind = ActionKind::MouseX2;
    else return false;

    *actionOut = action;
    return true;
}

bool TryReadWholeFileUtf16(const wchar_t* path, wchar_t** textOut) {
    if (path == nullptr || path[0] == L'\0' || textOut == nullptr) return false;
    *textOut = nullptr;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size < 2 || size > 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    wchar_t* buffer = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + sizeof(wchar_t)));
    if (buffer == nullptr) {
        CloseHandle(file);
        return false;
    }
    DWORD read = 0;
    const BOOL ok = ReadFile(file, buffer, size, &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return false;
    }
    if (buffer[0] == 0xFEFF) {
        MoveMemory(buffer, buffer + 1, size - sizeof(wchar_t));
    }
    *textOut = buffer;
    return true;
}

bool TryResolveGameAction(const wchar_t* actionName, ActionBinding* actionOut) {
    if (actionName == nullptr || actionOut == nullptr || g_keyBindingsFile[0] == L'\0') return false;
    wchar_t* text = nullptr;
    if (!TryReadWholeFileUtf16(g_keyBindingsFile, &text)) return false;

    wchar_t needle[128]{};
    wcscpy_s(needle, L"<STRING>NAME:");
    wcscat_s(needle, actionName);
    wchar_t* entry = wcsstr(text, needle);
    if (entry == nullptr) {
        HeapFree(GetProcessHeap(), 0, text);
        return false;
    }
    wchar_t* keyMarker = wcsstr(entry, L"<INTEGER>KEY:");
    wchar_t* modifierMarker = wcsstr(entry, L"<INTEGER>MODIFIERKEY:");
    if (keyMarker == nullptr || modifierMarker == nullptr) {
        HeapFree(GetProcessHeap(), 0, text);
        return false;
    }
    keyMarker += lstrlenW(L"<INTEGER>KEY:");
    modifierMarker += lstrlenW(L"<INTEGER>MODIFIERKEY:");
    const int key = _wtoi(keyMarker);
    const int modifier = _wtoi(modifierMarker);
    HeapFree(GetProcessHeap(), 0, text);

    return ActionFromGameKeyValue(key, modifier, actionOut);
}

const wchar_t* SettingsKeyForAction(const wchar_t* actionName) {
    if (actionName == nullptr) return nullptr;
    if (_wcsicmp(actionName, L"QK1") == 0) return L"KEYMAP_1";
    if (_wcsicmp(actionName, L"QK2") == 0) return L"KEYMAP_2";
    if (_wcsicmp(actionName, L"QK3") == 0) return L"KEYMAP_3";
    if (_wcsicmp(actionName, L"QK4") == 0) return L"KEYMAP_4";
    if (_wcsicmp(actionName, L"QK5") == 0) return L"KEYMAP_5";
    if (_wcsicmp(actionName, L"QK6") == 0) return L"KEYMAP_6";
    if (_wcsicmp(actionName, L"QK7") == 0) return L"KEYMAP_7";
    if (_wcsicmp(actionName, L"QK8") == 0) return L"KEYMAP_8";
    if (_wcsicmp(actionName, L"QK9") == 0) return L"KEYMAP_9";
    if (_wcsicmp(actionName, L"QK0") == 0) return L"KEYMAP_0";
    if (_wcsicmp(actionName, L"INVENTORYMENU") == 0) return L"KEYMAP_INVENTORY";
    if (_wcsicmp(actionName, L"PETMENU") == 0) return L"KEYMAP_PET";
    if (_wcsicmp(actionName, L"STATMENU") == 0) return L"KEYMAP_STATS";
    if (_wcsicmp(actionName, L"SKILLMENU") == 0) return L"KEYMAP_SKILLS";
    if (_wcsicmp(actionName, L"QUESTMENU") == 0) return L"KEYMAP_QUESTS";
    if (_wcsicmp(actionName, L"JOURNAL") == 0) return L"KEYMAP_JOURNAL";
    if (_wcsicmp(actionName, L"OPTIONSMENU") == 0) return L"KEYMAP_OPTIONS";
    if (_wcsicmp(actionName, L"AUTOMAP") == 0) return L"KEYMAP_AUTOMAP";
    if (_wcsicmp(actionName, L"AUTOMAPZOOMIN") == 0) return L"KEYMAP_AUTOMAPZOOMIN";
    if (_wcsicmp(actionName, L"AUTOMAPZOOMOUT") == 0) return L"KEYMAP_AUTOMAPZOOMOUT";
    if (_wcsicmp(actionName, L"HOLDPOSITION") == 0) return L"KEYMAP_HOLDPOS";
    if (_wcsicmp(actionName, L"SHOWITEMS") == 0) return L"KEYMAP_SHOWITEMS";
    if (_wcsicmp(actionName, L"CYCLESKILLUP") == 0) return L"KEYMAP_CYCLESKILLUP";
    if (_wcsicmp(actionName, L"CYCLESKILLDOWN") == 0) return L"KEYMAP_CYCLESKILLDOWN";
    if (_wcsicmp(actionName, L"SKILLSWAP") == 0) return L"KEYMAP_SWAPSKILLS";
    if (_wcsicmp(actionName, L"WEAPONSWAP") == 0) return L"KEYMAP_WEAPONSET";
    if (_wcsicmp(actionName, L"CLOSEALL") == 0) return L"KEYMAP_CLOSEALL";
    if (_wcsicmp(actionName, L"CAMERAZOOMIN") == 0) return L"KEYMAP_ZOOMIN";
    if (_wcsicmp(actionName, L"CAMERAZOOMOUT") == 0) return L"KEYMAP_ZOOMOUT";
    return nullptr;
}

bool TryReadSettingsKey(const wchar_t* settingName, int* keyOut) {
    if (settingName == nullptr || keyOut == nullptr || g_settingsFile[0] == L'\0') return false;
    wchar_t* text = nullptr;
    if (!TryReadWholeFileUtf16(g_settingsFile, &text)) return false;

    wchar_t needle[96]{};
    wcscpy_s(needle, settingName);
    wcscat_s(needle, L" :");
    wchar_t* entry = wcsstr(text, needle);
    if (entry == nullptr) {
        HeapFree(GetProcessHeap(), 0, text);
        return false;
    }
    entry += lstrlenW(needle);
    *keyOut = _wtoi(entry);
    HeapFree(GetProcessHeap(), 0, text);
    return *keyOut > 0;
}

bool TryResolveSettingsAction(const wchar_t* actionName, ActionBinding* actionOut) {
    const wchar_t* settingName = SettingsKeyForAction(actionName);
    int key = 0;
    if (!TryReadSettingsKey(settingName, &key)) return false;
    return ActionFromGameKeyValue(key, -1, actionOut);
}

ActionBinding ParseKeyCombination(const wchar_t* text) {
    ActionBinding action{};
    wchar_t value[64]{};
    lstrcpynW(value, text, 64);
    CharUpperBuffW(value, lstrlenW(value));

    wchar_t* plus = wcschr(value, L'+');
    if (plus != nullptr) {
        *plus = L'\0';
        const WORD modifier = ParseVirtualKeyName(value, 0);
        const WORD key = ParseVirtualKeyName(plus + 1, 0);
        if (modifier != 0 && key != 0) {
            action.kind = ActionKind::Key;
            action.modifierVk = modifier;
            action.vk = key;
        }
        return action;
    }

    const WORD key = ParseVirtualKeyName(value, 0);
    if (key != 0) {
        action.kind = ActionKind::Key;
        action.vk = key;
    }
    return action;
}

ActionBinding ParseActionName(const wchar_t* text) {
    ActionBinding action{};
    wchar_t value[64]{};
    if (text == nullptr || text[0] == L'\0') return action;
    lstrcpynW(value, text, 64);
    CharUpperBuffW(value, lstrlenW(value));
    if (lstrcmpW(value, L"NONE") == 0 || lstrcmpW(value, L"DISABLED") == 0) return action;
    if (lstrcmpW(value, L"MOUSELEFT") == 0 || lstrcmpW(value, L"LEFTCLICK") == 0) {
        action.kind = ActionKind::MouseLeft;
        return action;
    }
    if (lstrcmpW(value, L"MOUSERIGHT") == 0 || lstrcmpW(value, L"RIGHTCLICK") == 0) {
        action.kind = ActionKind::MouseRight;
        return action;
    }
    if (lstrcmpW(value, L"MOUSEMIDDLE") == 0 || lstrcmpW(value, L"MIDDLECLICK") == 0) {
        action.kind = ActionKind::MouseMiddle;
        return action;
    }
    if (lstrcmpW(value, L"MOUSEX1") == 0 || lstrcmpW(value, L"X1B") == 0 || lstrcmpW(value, L"XBUTTON1") == 0) {
        action.kind = ActionKind::MouseX1;
        return action;
    }
    if (lstrcmpW(value, L"MOUSEX2") == 0 || lstrcmpW(value, L"X2B") == 0 || lstrcmpW(value, L"XBUTTON2") == 0) {
        action.kind = ActionKind::MouseX2;
        return action;
    }
    if (lstrcmpW(value, L"ADDON:TOGGLERIGHTSTICKMOUSEMODE") == 0 ||
        lstrcmpW(value, L"ADDON:TOGGLEAIMMODE") == 0 ||
        lstrcmpW(value, L"ADDON:TOGGLERADIALAIM") == 0 ||
        lstrcmpW(value, L"TOGGLERIGHTSTICKMOUSEMODE") == 0 ||
        lstrcmpW(value, L"TOGGLEAIMMODE") == 0 ||
        lstrcmpW(value, L"TOGGLERADIALAIM") == 0) {
        action.kind = ActionKind::ToggleRightStickMouseMode;
        return action;
    }

    if (wcsncmp(value, L"GAME:", 5) == 0 || wcsncmp(value, L"ACTION:", 7) == 0) {
        const wchar_t* actionName = value + (value[0] == L'G' ? 5 : 7);
        if (TryResolveGameAction(actionName, &action)) return action;
        if (TryResolveSettingsAction(actionName, &action)) return action;
        return action;
    }

    if (wcsncmp(value, L"MOVE:", 5) == 0 || wcsncmp(value, L"MOVEMENT:", 9) == 0) {
        const wchar_t* direction = value + (value[0] == L'M' && value[4] == L':' ? 5 : 9);
        action.kind = ActionKind::Key;
        if (lstrcmpW(direction, L"UP") == 0) action.vk = g_config.movement.up;
        if (lstrcmpW(direction, L"LEFT") == 0) action.vk = g_config.movement.left;
        if (lstrcmpW(direction, L"DOWN") == 0) action.vk = g_config.movement.down;
        if (lstrcmpW(direction, L"RIGHT") == 0) action.vk = g_config.movement.right;
        if (action.vk == 0) action.kind = ActionKind::None;
        return action;
    }

    action = ParseKeyCombination(value);
    return action;
}

HKL GetForegroundKeyboardLayout() {
    HWND foreground = GetForegroundWindow();
    if (foreground != nullptr) {
        const DWORD threadId = GetWindowThreadProcessId(foreground, nullptr);
        if (threadId != 0) {
            HKL layout = GetKeyboardLayout(threadId);
            if (layout != nullptr) return layout;
        }
    }
    return GetKeyboardLayout(0);
}

WORD VirtualKeyFromScanCode(LONG scanCode, WORD fallback) {
    if (scanCode == 0) return fallback;
    const UINT mapped = MapVirtualKeyExW(
        static_cast<UINT>(scanCode),
        MAPVK_VSC_TO_VK_EX,
        GetForegroundKeyboardLayout());
    return mapped == 0 ? fallback : static_cast<WORD>(mapped);
}

WORD ReadMovementKey(const wchar_t* name, const wchar_t* fallbackName, WORD fallback) {
    wchar_t value[48]{};
    GetPrivateProfileStringW(L"Keys", name, fallbackName, value, 48, g_movementIniFile);
    return ParseVirtualKeyName(value, fallback);
}

void LoadMovementBindings(Config* config) {
    wchar_t preset[48]{};
    wchar_t bindingMode[48]{};
    GetPrivateProfileStringW(L"Keys", L"Preset", L"AUTO", preset, 48, g_movementIniFile);
    GetPrivateProfileStringW(L"Keys", L"BindingMode", L"VIRTUAL", bindingMode, 48, g_movementIniFile);
    CharUpperBuffW(preset, lstrlenW(preset));
    CharUpperBuffW(bindingMode, lstrlenW(bindingMode));
    const bool physical =
        config->followKeyboardProfile &&
        (lstrcmpW(bindingMode, L"PHYSICAL") == 0 ||
            lstrcmpW(preset, L"AUTO") == 0 ||
            lstrcmpW(preset, L"WASD") == 0 ||
            lstrcmpW(preset, L"ZQSD") == 0);
    if (physical) {
        config->movement.up = VirtualKeyFromScanCode(GetPrivateProfileIntW(L"Keys", L"ScanUp", 0x11, g_movementIniFile), 'W');
        config->movement.left = VirtualKeyFromScanCode(GetPrivateProfileIntW(L"Keys", L"ScanLeft", 0x1E, g_movementIniFile), 'A');
        config->movement.down = VirtualKeyFromScanCode(GetPrivateProfileIntW(L"Keys", L"ScanDown", 0x1F, g_movementIniFile), 'S');
        config->movement.right = VirtualKeyFromScanCode(GetPrivateProfileIntW(L"Keys", L"ScanRight", 0x20, g_movementIniFile), 'D');
        return;
    }
    config->movement.up = ReadMovementKey(L"Up", L"W", 'W');
    config->movement.left = ReadMovementKey(L"Left", L"A", 'A');
    config->movement.down = ReadMovementKey(L"Down", L"S", 'S');
    config->movement.right = ReadMovementKey(L"Right", L"D", 'D');
}

void ReadActionBinding(const wchar_t* iniName, const wchar_t* fallback, ActionBinding* actionOut) {
    wchar_t value[128]{};
    GetPrivateProfileStringW(L"Buttons", iniName, fallback, value, 128, g_supportIniFile);
    *actionOut = ParseActionName(value);
}

bool ControlPartToInput(const wchar_t* part, WORD* buttonOut, bool* leftTriggerOut, bool* rightTriggerOut) {
    if (part == nullptr || part[0] == L'\0') return false;
    if (_wcsicmp(part, L"A") == 0) *buttonOut |= XINPUT_GAMEPAD_A;
    else if (_wcsicmp(part, L"B") == 0) *buttonOut |= XINPUT_GAMEPAD_B;
    else if (_wcsicmp(part, L"X") == 0) *buttonOut |= XINPUT_GAMEPAD_X;
    else if (_wcsicmp(part, L"Y") == 0) *buttonOut |= XINPUT_GAMEPAD_Y;
    else if (_wcsicmp(part, L"LeftShoulder") == 0 || _wcsicmp(part, L"LB") == 0) *buttonOut |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    else if (_wcsicmp(part, L"RightShoulder") == 0 || _wcsicmp(part, L"RB") == 0) *buttonOut |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    else if (_wcsicmp(part, L"Back") == 0 || _wcsicmp(part, L"Select") == 0) *buttonOut |= XINPUT_GAMEPAD_BACK;
    else if (_wcsicmp(part, L"Start") == 0) *buttonOut |= XINPUT_GAMEPAD_START;
    else if (_wcsicmp(part, L"LeftThumb") == 0 || _wcsicmp(part, L"L3") == 0) *buttonOut |= XINPUT_GAMEPAD_LEFT_THUMB;
    else if (_wcsicmp(part, L"RightThumb") == 0 || _wcsicmp(part, L"R3") == 0) *buttonOut |= XINPUT_GAMEPAD_RIGHT_THUMB;
    else if (_wcsicmp(part, L"DPadUp") == 0) *buttonOut |= XINPUT_GAMEPAD_DPAD_UP;
    else if (_wcsicmp(part, L"DPadRight") == 0) *buttonOut |= XINPUT_GAMEPAD_DPAD_RIGHT;
    else if (_wcsicmp(part, L"DPadDown") == 0) *buttonOut |= XINPUT_GAMEPAD_DPAD_DOWN;
    else if (_wcsicmp(part, L"DPadLeft") == 0) *buttonOut |= XINPUT_GAMEPAD_DPAD_LEFT;
    else if (_wcsicmp(part, L"LeftTrigger") == 0 || _wcsicmp(part, L"LT") == 0) *leftTriggerOut = true;
    else if (_wcsicmp(part, L"RightTrigger") == 0 || _wcsicmp(part, L"RT") == 0) *rightTriggerOut = true;
    else return false;
    return true;
}

bool ParseChordName(const wchar_t* name, ChordBinding* chordOut) {
    if (name == nullptr || chordOut == nullptr || wcschr(name, L'+') == nullptr) return false;

    ChordBinding parsed{};
    lstrcpynW(parsed.iniName, name, 96);

    wchar_t copy[96]{};
    lstrcpynW(copy, name, 96);
    wchar_t* context = nullptr;
    wchar_t* part = wcstok_s(copy, L"+", &context);
    int partCount = 0;
    while (part != nullptr) {
        while (*part == L' ' || *part == L'\t') ++part;
        wchar_t* end = part + lstrlenW(part);
        while (end > part && (end[-1] == L' ' || end[-1] == L'\t')) {
            --end;
            *end = L'\0';
        }
        if (!ControlPartToInput(part, &parsed.requiredButtons, &parsed.requiredLeftTrigger, &parsed.requiredRightTrigger)) {
            return false;
        }
        ++partCount;
        part = wcstok_s(nullptr, L"+", &context);
    }

    if (partCount < 2) return false;
    *chordOut = parsed;
    return true;
}

void LoadChordBindings() {
    g_chordCount = 0;
    wchar_t section[32767]{};
    const DWORD chars = GetPrivateProfileSectionW(L"Buttons", section, 32767, g_supportIniFile);
    if (chars == 0 || chars >= 32766) return;

    for (wchar_t* entry = section; *entry != L'\0'; entry += lstrlenW(entry) + 1) {
        wchar_t* equals = wcschr(entry, L'=');
        if (equals == nullptr) continue;
        *equals = L'\0';
        const wchar_t* key = entry;
        const wchar_t* value = equals + 1;
        if (wcschr(key, L'+') == nullptr || g_chordCount >= kMaxChordBindings) continue;

        ChordBinding chord{};
        if (!ParseChordName(key, &chord)) continue;
        chord.action = ParseActionName(value);
        g_chords[g_chordCount++] = chord;
    }
}

void LoadConfig() {
    ReleaseAllOwnedInput();
    ResolveConfiguredPath(
        L"General",
        L"MovementProfileIni",
        kMovementIniName,
        g_movementIniFile);
    ResolveKeyBindingsFile();
    ResolveSettingsFile();

    Config next{};
    next.enable = GetPrivateProfileIntW(L"General", L"Enable", 1, g_supportIniFile) != 0;
    next.controllerIndex = static_cast<DWORD>(ClampLong(GetPrivateProfileIntW(L"General", L"ControllerIndex", 0, g_supportIniFile), 0, 3));
    next.pollIntervalMs = static_cast<DWORD>(ClampLong(GetPrivateProfileIntW(L"General", L"PollIntervalMs", 8, g_supportIniFile), 4, 33));
    next.focusRequired = GetPrivateProfileIntW(L"General", L"FocusRequired", 1, g_supportIniFile) != 0;
    next.acquireControllerExclusive = GetPrivateProfileIntW(L"General", L"AcquireControllerExclusive", 1, g_supportIniFile) != 0;
    next.suspendWhileOverlayVisible = GetPrivateProfileIntW(L"General", L"SuspendWhileOverlayVisible", 1, g_supportIniFile) != 0;
    next.enablePhysicalOverride = GetPrivateProfileIntW(L"Recovery", L"EnablePhysicalKeyboardMouseOverride", 1, g_supportIniFile) != 0;
    next.enablePanicCombo = GetPrivateProfileIntW(L"Recovery", L"EnableBackStartReleaseCombo", 1, g_supportIniFile) != 0;
    next.physicalOverrideMs = static_cast<DWORD>(ClampLong(GetPrivateProfileIntW(L"Recovery", L"PhysicalOverrideMs", 1200, g_supportIniFile), 100, 10000));
    next.ignorePhysicalAfterGamepadMs = static_cast<DWORD>(ClampLong(GetPrivateProfileIntW(L"Recovery", L"IgnorePhysicalAfterGamepadMs", 350, g_supportIniFile), 0, 2000));
    next.panicComboSuspendMs = static_cast<DWORD>(ClampLong(GetPrivateProfileIntW(L"Recovery", L"BackStartSuspendMs", 2500, g_supportIniFile), 250, 10000));
    next.leftStickEnabled = GetPrivateProfileIntW(L"Movement", L"LeftStickEnabled", 1, g_supportIniFile) != 0;
    next.followKeyboardProfile = GetPrivateProfileIntW(L"Movement", L"FollowKeyboardProfile", 1, g_supportIniFile) != 0;
    next.leftDeadzone = ClampLong(GetPrivateProfileIntW(L"Movement", L"Deadzone", kDefaultLeftDeadzone, g_supportIniFile), 0, 32767);
    next.activationThresholdX1000 = ClampLong(GetPrivateProfileIntW(L"Movement", L"ActivationThresholdX1000", 350, g_supportIniFile), 1, 1000);
    next.releaseThresholdX1000 = ClampLong(GetPrivateProfileIntW(L"Movement", L"ReleaseThresholdX1000", 250, g_supportIniFile), 0, next.activationThresholdX1000);
    next.rightStickMouse = GetPrivateProfileIntW(L"Mouse", L"RightStickMouse", 1, g_supportIniFile) != 0;
    next.rightDeadzone = ClampLong(GetPrivateProfileIntW(L"Mouse", L"Deadzone", kDefaultRightDeadzone, g_supportIniFile), 0, 32767);
    next.mouseSensitivityX = ClampLong(GetPrivateProfileIntW(L"Mouse", L"SensitivityX", 14, g_supportIniFile), 1, 80);
    next.mouseSensitivityY = ClampLong(GetPrivateProfileIntW(L"Mouse", L"SensitivityY", 14, g_supportIniFile), 1, 80);
    next.radialMouseRadius = ClampLong(GetPrivateProfileIntW(L"Mouse", L"RadialRadius", 140, g_supportIniFile), 40, 2000);
    next.radialMouseCenterOffsetX = ClampLong(GetPrivateProfileIntW(L"Mouse", L"RadialCenterOffsetX", 0, g_supportIniFile), -2000, 2000);
    next.radialMouseCenterOffsetY = ClampLong(GetPrivateProfileIntW(L"Mouse", L"RadialCenterOffsetY", 0, g_supportIniFile), -2000, 2000);
    next.radialReturnToCenter = GetPrivateProfileIntW(L"Mouse", L"RadialReturnToCenter", 0, g_supportIniFile) != 0;
    next.radialUseFixedRadius = GetPrivateProfileIntW(L"Mouse", L"RadialUseFixedRadius", 1, g_supportIniFile) != 0;
    next.radialActivationThresholdX1000 = ClampLong(GetPrivateProfileIntW(L"Mouse", L"RadialActivationThresholdX1000", 450, g_supportIniFile), 1, 1000);
    next.radialReleaseThresholdX1000 = ClampLong(GetPrivateProfileIntW(L"Mouse", L"RadialReleaseThresholdX1000", 300, g_supportIniFile), 0, next.radialActivationThresholdX1000);
    next.invertMouseY = GetPrivateProfileIntW(L"Mouse", L"InvertY", 0, g_supportIniFile) != 0;
    next.triggerThreshold = static_cast<BYTE>(ClampLong(GetPrivateProfileIntW(L"Buttons", L"TriggerThreshold", 30, g_supportIniFile), 1, 255));
    LoadMovementBindings(&next);
    g_config = next;
    ReadActionBinding(L"A", L"MouseLeft", &g_buttons[0].action);
    ReadActionBinding(L"B", L"Escape", &g_buttons[1].action);
    ReadActionBinding(L"X", L"MouseRight", &g_buttons[2].action);
    ReadActionBinding(L"Y", L"Space", &g_buttons[3].action);
    ReadActionBinding(L"LeftShoulder", L"Shift", &g_buttons[4].action);
    ReadActionBinding(L"RightShoulder", L"Ctrl", &g_buttons[5].action);
    ReadActionBinding(L"Back", L"Tab", &g_buttons[6].action);
    ReadActionBinding(L"Start", L"Escape", &g_buttons[7].action);
    ReadActionBinding(L"LeftThumb", L"None", &g_buttons[8].action);
    ReadActionBinding(L"RightThumb", L"Addon:ToggleRightStickMouseMode", &g_buttons[9].action);
    ReadActionBinding(L"DPadUp", L"1", &g_buttons[10].action);
    ReadActionBinding(L"DPadRight", L"2", &g_buttons[11].action);
    ReadActionBinding(L"DPadDown", L"3", &g_buttons[12].action);
    ReadActionBinding(L"DPadLeft", L"4", &g_buttons[13].action);
    ReadActionBinding(L"LeftTrigger", L"MouseLeft", &g_leftTrigger.action);
    ReadActionBinding(L"RightTrigger", L"MouseRight", &g_rightTrigger.action);
    LoadChordBindings();
    g_config = next;
    QueryWriteTime(g_supportIniFile, &g_lastSupportIniWriteTime);
    QueryWriteTime(g_movementIniFile, &g_lastMovementIniWriteTime);
    QueryWriteTime(g_keyBindingsFile, &g_lastKeyBindingsWriteTime);
    QueryWriteTime(g_settingsFile, &g_lastSettingsWriteTime);
    LogLine("Controller support config loaded.");
}

void PollConfigReload(DWORD now) {
    if (now - g_lastConfigPollTick < kConfigPollMs) return;
    g_lastConfigPollTick = now;
    FILETIME supportTime{};
    FILETIME movementTime{};
    const bool supportChanged =
        QueryWriteTime(g_supportIniFile, &supportTime) &&
        !FileTimesEqual(supportTime, g_lastSupportIniWriteTime);
    const bool movementChanged =
        QueryWriteTime(g_movementIniFile, &movementTime) &&
        !FileTimesEqual(movementTime, g_lastMovementIniWriteTime);
    FILETIME keyBindingsTime{};
    const bool keyBindingsChanged =
        QueryWriteTime(g_keyBindingsFile, &keyBindingsTime) &&
        !FileTimesEqual(keyBindingsTime, g_lastKeyBindingsWriteTime);
    FILETIME settingsTime{};
    const bool settingsChanged =
        QueryWriteTime(g_settingsFile, &settingsTime) &&
        !FileTimesEqual(settingsTime, g_lastSettingsWriteTime);
    if (supportChanged || movementChanged || keyBindingsChanged || settingsChanged) LoadConfig();
}

bool LoadRealD3D9() {
    if (g_realD3D9 != nullptr) return true;
    wchar_t systemDir[MAX_PATH]{};
    if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0) return false;
    wchar_t realPath[MAX_PATH]{};
    wcscpy_s(realPath, systemDir);
    wcscat_s(realPath, L"\\d3d9.dll");
    g_realD3D9 = LoadLibraryW(realPath);
    return g_realD3D9 != nullptr;
}

bool LoadXInput() {
    if (g_xinputGetState != nullptr) return true;
    const wchar_t* candidates[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    for (const wchar_t* candidate : candidates) {
        g_xinputModule = LoadLibraryW(candidate);
        if (g_xinputModule == nullptr) continue;
        g_xinputGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(g_xinputModule, "XInputGetState"));
        if (g_xinputGetState != nullptr) {
            LogLine("XInput backend loaded.");
            return true;
        }
        FreeLibrary(g_xinputModule);
        g_xinputModule = nullptr;
    }
    return false;
}

bool GameHasForegroundFocus() {
    return ForegroundWindowForCurrentProcess() != nullptr;
}

bool OverlayLooksVisible() {
    HWND overlay = FindWindowA(kOverlayClassName, nullptr);
    if (overlay == nullptr) return false;
    return IsWindowVisible(overlay) || GetPropA(overlay, kOverlayVisibleProp) != nullptr;
}

BOOL CALLBACK DirectInputEnumCallback(const DIDEVICEINSTANCEW* instance, void*) {
    if (g_directInput == nullptr || g_exclusiveDevice != nullptr || g_exclusiveWindow == nullptr) {
        return DIENUM_STOP;
    }

    IDirectInputDevice8W* device = nullptr;
    if (FAILED(g_directInput->CreateDevice(instance->guidInstance, &device, nullptr))) {
        return DIENUM_CONTINUE;
    }

    if (FAILED(device->SetDataFormat(&c_dfDIJoystick2))) {
        device->Release();
        return DIENUM_CONTINUE;
    }

    if (FAILED(device->SetCooperativeLevel(g_exclusiveWindow, DISCL_FOREGROUND | DISCL_EXCLUSIVE))) {
        device->Release();
        return DIENUM_CONTINUE;
    }

    g_exclusiveDevice = device;
    return DIENUM_STOP;
}

void ReleaseExclusiveController() {
    if (g_exclusiveDevice != nullptr) {
        g_exclusiveDevice->Unacquire();
        g_exclusiveDevice->Release();
        g_exclusiveDevice = nullptr;
    }
    if (g_directInput != nullptr) {
        g_directInput->Release();
        g_directInput = nullptr;
    }
    g_exclusiveWindow = nullptr;
    g_loggedExclusiveAcquired = false;
}

void MaintainExclusiveController(DWORD now) {
    if (!g_config.acquireControllerExclusive) {
        ReleaseExclusiveController();
        return;
    }

    HWND targetWindow = ForegroundWindowForCurrentProcess();
    if (targetWindow == nullptr) {
        ReleaseExclusiveController();
        return;
    }

    if (g_exclusiveWindow != nullptr && g_exclusiveWindow != targetWindow) {
        ReleaseExclusiveController();
    }
    g_exclusiveWindow = targetWindow;

    if (g_exclusiveDevice != nullptr) {
        const HRESULT result = g_exclusiveDevice->Acquire();
        if (result == DI_OK || result == S_FALSE || result == DI_NOEFFECT) return;
        ReleaseExclusiveController();
    }

    if (now - g_lastExclusivePollTick < 1000) return;
    g_lastExclusivePollTick = now;

    if (g_directInput == nullptr &&
        FAILED(DirectInput8Create(
            g_module,
            DIRECTINPUT_VERSION,
            IID_IDirectInput8W,
            reinterpret_cast<void**>(&g_directInput),
            nullptr))) {
        if (!g_loggedExclusiveFailed) {
            g_loggedExclusiveFailed = true;
            LogLine("DirectInput exclusive controller lock unavailable; continuing with XInput only.");
        }
        return;
    }

    if (g_exclusiveDevice == nullptr && g_directInput != nullptr) {
        g_directInput->EnumDevices(DI8DEVCLASS_GAMECTRL, DirectInputEnumCallback, nullptr, DIEDFL_ATTACHEDONLY);
    }

    if (g_exclusiveDevice == nullptr) {
        if (!g_loggedExclusiveFailed) {
            g_loggedExclusiveFailed = true;
            LogLine("No DirectInput game controller could be locked exclusively; continuing with XInput only.");
        }
        return;
    }

    const HRESULT result = g_exclusiveDevice->Acquire();
    if (result == DI_OK || result == S_FALSE || result == DI_NOEFFECT) {
        if (!g_loggedExclusiveAcquired) {
            g_loggedExclusiveAcquired = true;
            g_loggedExclusiveFailed = false;
            LogLine("DirectInput exclusive controller lock acquired while Torchlight II has focus.");
        }
    } else if (!g_loggedExclusiveFailed) {
        g_loggedExclusiveFailed = true;
        LogLine("DirectInput exclusive controller lock failed; continuing with XInput only.");
    }
}

bool ControllerInjectionAllowed() {
    return !g_config.focusRequired || GameHasForegroundFocus();
}

bool EnsureControllerInjectionAllowed() {
    if (ControllerInjectionAllowed()) return true;
    ReleaseAllOwnedInput();
    ReleaseExclusiveController();
    return false;
}

float NormalizeThumbAxis(SHORT value, int deadzone) {
    const float raw = static_cast<float>(value);
    const float sign = raw < 0.0f ? -1.0f : 1.0f;
    const float magnitude = fabsf(raw);
    if (magnitude <= static_cast<float>(deadzone)) return 0.0f;
    const float normalized =
        (magnitude - static_cast<float>(deadzone)) /
        (32767.0f - static_cast<float>(deadzone));
    return (normalized > 1.0f ? 1.0f : normalized) * sign;
}

bool LatchAxisPositive(float value, bool previous, int activateX1000, int releaseX1000) {
    const float activate = static_cast<float>(activateX1000) / 1000.0f;
    const float release = static_cast<float>(releaseX1000) / 1000.0f;
    return previous ? value > release : value > activate;
}

bool LatchAxisNegative(float value, bool previous, int activateX1000, int releaseX1000) {
    const float activate = static_cast<float>(activateX1000) / 1000.0f;
    const float release = static_cast<float>(releaseX1000) / 1000.0f;
    return previous ? value < -release : value < -activate;
}

int MovementMaskFromLeftStick(const XINPUT_GAMEPAD& gamepad) {
    if (!g_config.leftStickEnabled) return 0;
    const float x = NormalizeThumbAxis(gamepad.sThumbLX, g_config.leftDeadzone);
    const float y = NormalizeThumbAxis(gamepad.sThumbLY, g_config.leftDeadzone);
    g_leftLatch.left = LatchAxisNegative(x, g_leftLatch.left, g_config.activationThresholdX1000, g_config.releaseThresholdX1000);
    g_leftLatch.right = LatchAxisPositive(x, g_leftLatch.right, g_config.activationThresholdX1000, g_config.releaseThresholdX1000);
    g_leftLatch.up = LatchAxisPositive(y, g_leftLatch.up, g_config.activationThresholdX1000, g_config.releaseThresholdX1000);
    g_leftLatch.down = LatchAxisNegative(y, g_leftLatch.down, g_config.activationThresholdX1000, g_config.releaseThresholdX1000);
    int mask = 0;
    if (g_leftLatch.up) mask |= kDirUp;
    if (g_leftLatch.left) mask |= kDirLeft;
    if (g_leftLatch.down) mask |= kDirDown;
    if (g_leftLatch.right) mask |= kDirRight;
    return mask;
}

void PumpRightStickMouse(const XINPUT_GAMEPAD& gamepad) {
    if (!g_config.rightStickMouse) return;
    const float x = NormalizeThumbAxis(gamepad.sThumbRX, g_config.rightDeadzone);
    const float y = NormalizeThumbAxis(gamepad.sThumbRY, g_config.rightDeadzone);
    if (g_radialMouseMode) {
        HWND targetWindow = ForegroundWindowForCurrentProcess();
        if (targetWindow == nullptr) return;

        RECT client{};
        if (!GetClientRect(targetWindow, &client)) return;

        POINT center{
            (client.right - client.left) / 2 + g_config.radialMouseCenterOffsetX,
            (client.bottom - client.top) / 2 + g_config.radialMouseCenterOffsetY,
        };
        if (!ClientToScreen(targetWindow, &center)) return;

        const float magnitude = sqrtf(x * x + y * y);
        const float activationThreshold = static_cast<float>(g_config.radialActivationThresholdX1000) / 1000.0f;
        const float releaseThreshold = static_cast<float>(g_config.radialReleaseThresholdX1000) / 1000.0f;

        if (!g_radialAimActive) {
            if (magnitude < activationThreshold) {
                if (g_config.radialReturnToCenter) SetCursorPos(center.x, center.y);
                return;
            }
            g_radialAimActive = true;
        } else if (magnitude < releaseThreshold) {
            g_radialAimActive = false;
            if (g_config.radialReturnToCenter) SetCursorPos(center.x, center.y);
            return;
        }

        float radialX = x;
        float radialY = y;
        if (g_config.radialUseFixedRadius && magnitude > 0.0f) {
            radialX = x / magnitude;
            radialY = y / magnitude;
        }
        g_radialLastX = radialX;
        g_radialLastY = radialY;

        const LONG targetX = center.x + static_cast<LONG>(g_radialLastX * static_cast<float>(g_config.radialMouseRadius));
        const LONG targetY = center.y + static_cast<LONG>((g_config.invertMouseY ? g_radialLastY : -g_radialLastY) * static_cast<float>(g_config.radialMouseRadius));
        SetCursorPos(targetX, targetY);
        return;
    }

    const LONG dx = static_cast<LONG>(x * static_cast<float>(g_config.mouseSensitivityX));
    const LONG dy = static_cast<LONG>((g_config.invertMouseY ? y : -y) * static_cast<float>(g_config.mouseSensitivityY));
    if (dx == 0 && dy == 0) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(input));
}

void PumpButton(ButtonBinding& binding, WORD pressedButtons) {
    const bool isDown = (pressedButtons & binding.xinputButton) != 0;
    if (isDown == binding.ownedDown) return;
    ApplyAction(binding.action, isDown);
    binding.ownedDown = isDown;
}

void PumpTrigger(TriggerBinding& binding, BYTE value) {
    const bool isDown = value >= g_config.triggerThreshold;
    if (isDown == binding.ownedDown) return;
    ApplyAction(binding.action, isDown);
    binding.ownedDown = isDown;
}

ChordConsumption PumpChords(const XINPUT_GAMEPAD& gamepad) {
    ChordConsumption consumed{};
    for (int i = 0; i < g_chordCount; ++i) {
        ChordBinding& chord = g_chords[i];
        const bool buttonsDown = (gamepad.wButtons & chord.requiredButtons) == chord.requiredButtons;
        const bool leftTriggerDown = !chord.requiredLeftTrigger || gamepad.bLeftTrigger >= g_config.triggerThreshold;
        const bool rightTriggerDown = !chord.requiredRightTrigger || gamepad.bRightTrigger >= g_config.triggerThreshold;
        const bool isDown = buttonsDown && leftTriggerDown && rightTriggerDown;
        if (isDown) {
            consumed.buttons |= chord.requiredButtons;
            consumed.leftTrigger = consumed.leftTrigger || chord.requiredLeftTrigger;
            consumed.rightTrigger = consumed.rightTrigger || chord.requiredRightTrigger;
        }
        if (isDown == chord.ownedDown) continue;
        ApplyAction(chord.action, isDown);
        chord.ownedDown = isDown;
    }
    return consumed;
}

void PumpButtons(const XINPUT_GAMEPAD& gamepad) {
    const ChordConsumption consumed = PumpChords(gamepad);
    const WORD simpleButtons = gamepad.wButtons & ~consumed.buttons;
    const bool leftTriggerConsumed = consumed.leftTrigger;
    const bool rightTriggerConsumed = consumed.rightTrigger;
    for (ButtonBinding& button : g_buttons) PumpButton(button, simpleButtons);
    PumpTrigger(g_leftTrigger, leftTriggerConsumed ? 0 : gamepad.bLeftTrigger);
    PumpTrigger(g_rightTrigger, rightTriggerConsumed ? 0 : gamepad.bRightTrigger);
}

void ArmManualOverride(DWORD now, DWORD durationMs) {
    if (!g_config.enablePhysicalOverride) return;
    const LONG ignoreUntil = InterlockedCompareExchange(&g_ignorePhysicalInputUntilTick, 0, 0);
    if (ignoreUntil != 0 && static_cast<LONG>(now - static_cast<DWORD>(ignoreUntil)) < 0) return;
    InterlockedExchange(&g_manualOverrideUntilTick, static_cast<LONG>(now + durationMs));
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && lParam != 0) {
        const KBDLLHOOKSTRUCT* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if ((info->flags & LLKHF_INJECTED) == 0 &&
            (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN || wParam == WM_KEYUP || wParam == WM_SYSKEYUP)) {
            ArmManualOverride(GetTickCount(), g_config.physicalOverrideMs);
        }
    }
    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && lParam != 0) {
        const MSLLHOOKSTRUCT* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if ((info->flags & LLMHF_INJECTED) == 0 &&
            (wParam == WM_MOUSEMOVE || wParam == WM_LBUTTONDOWN || wParam == WM_LBUTTONUP ||
                wParam == WM_RBUTTONDOWN || wParam == WM_RBUTTONUP || wParam == WM_MBUTTONDOWN ||
                wParam == WM_MBUTTONUP || wParam == WM_MOUSEWHEEL)) {
            ArmManualOverride(GetTickCount(), g_config.physicalOverrideMs);
        }
    }
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

void InstallInputRecoveryHooks() {
    if (g_keyboardHook == nullptr) {
        g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, g_module, 0);
    }
    if (g_mouseHook == nullptr) {
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, g_module, 0);
    }
    LogLine(
        g_keyboardHook != nullptr && g_mouseHook != nullptr
            ? "Physical keyboard/mouse recovery hooks installed."
            : "Physical keyboard/mouse recovery hooks could not be fully installed.");
}

void UninstallInputRecoveryHooks() {
    if (g_keyboardHook != nullptr) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
    if (g_mouseHook != nullptr) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
}

void PumpThreadMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool PanicComboDown(const XINPUT_GAMEPAD& gamepad) {
    return g_config.enablePanicCombo &&
        (gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0 &&
        (gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
}

bool ConfigToolActive() {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, kConfigMutexName);
    if (mutex == nullptr) return false;

    const DWORD result = WaitForSingleObject(mutex, 0);
    const bool active = result == WAIT_TIMEOUT;
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
        ReleaseMutex(mutex);
    }
    CloseHandle(mutex);
    return active;
}

DWORD WINAPI ControllerSupportThread(void*) {
    LogLine("TL2TrueControlerSupport d3d9 integrated controller thread started.");
    LoadXInput();
    LoadConfig();
    InstallInputRecoveryHooks();

    while (InterlockedCompareExchange(&g_running, 1, 1) != 0) {
        PumpThreadMessages();
        const DWORD now = GetTickCount();
        PollConfigReload(now);

        if (ConfigToolActive()) {
            if (!g_loggedConfigToolPause) {
                g_loggedConfigToolPause = true;
                LogLine("Configurator active; controller injection paused.");
            }
            ReleaseAllOwnedInput();
            ReleaseExclusiveController();
            Sleep(33);
            continue;
        }
        g_loggedConfigToolPause = false;

        if (!g_config.enable ||
            (g_config.focusRequired && !GameHasForegroundFocus()) ||
            (g_config.suspendWhileOverlayVisible && OverlayLooksVisible())) {
            if (g_config.focusRequired && !GameHasForegroundFocus() && !g_loggedNoFocus) {
                g_loggedNoFocus = true;
                LogLine("Waiting for Torchlight II foreground focus.");
            }
            ReleaseAllOwnedInput();
            if (g_config.focusRequired && !GameHasForegroundFocus()) ReleaseExclusiveController();
            Sleep(33);
            continue;
        }
        g_loggedNoFocus = false;
        MaintainExclusiveController(now);

        const LONG manualOverrideUntil = InterlockedCompareExchange(&g_manualOverrideUntilTick, 0, 0);
        if (manualOverrideUntil != 0 && static_cast<LONG>(now - static_cast<DWORD>(manualOverrideUntil)) < 0) {
            if (!g_loggedManualOverride) {
                g_loggedManualOverride = true;
                LogLine("Physical keyboard/mouse input detected; controller support temporarily released ownership.");
            }
            ReleaseAllOwnedInput();
            Sleep(16);
            continue;
        }
        g_loggedManualOverride = false;

        if (!LoadXInput()) {
            ReleaseAllOwnedInput();
            Sleep(250);
            continue;
        }

        XINPUT_STATE state{};
        const DWORD result = g_xinputGetState(g_config.controllerIndex, &state);
        if (result != ERROR_SUCCESS) {
            if (g_wasConnected) LogLine("XInput controller disconnected; released owned input.");
            g_wasConnected = false;
            ReleaseAllOwnedInput();
            Sleep(100);
            continue;
        }

        if (!g_wasConnected) {
            g_wasConnected = true;
            LogLine("XInput controller connected.");
        }

        if (state.Gamepad.wButtons != 0 ||
            state.Gamepad.bLeftTrigger >= g_config.triggerThreshold ||
            state.Gamepad.bRightTrigger >= g_config.triggerThreshold ||
            state.Gamepad.sThumbLX != 0 ||
            state.Gamepad.sThumbLY != 0 ||
            state.Gamepad.sThumbRX != 0 ||
            state.Gamepad.sThumbRY != 0) {
            InterlockedExchange(
                &g_ignorePhysicalInputUntilTick,
                static_cast<LONG>(now + g_config.ignorePhysicalAfterGamepadMs));
        }

        if (PanicComboDown(state.Gamepad)) {
            ReleaseAllOwnedInput();
            InterlockedExchange(&g_manualOverrideUntilTick, static_cast<LONG>(now + g_config.panicComboSuspendMs));
            LogLine("Back+Start release combo activated; controller support suspended briefly.");
            Sleep(100);
            continue;
        }

        if (!EnsureControllerInjectionAllowed()) {
            Sleep(16);
            continue;
        }
        ApplyMovementMask(MovementMaskFromLeftStick(state.Gamepad));
        if (!EnsureControllerInjectionAllowed()) {
            Sleep(16);
            continue;
        }
        PumpRightStickMouse(state.Gamepad);
        if (!EnsureControllerInjectionAllowed()) {
            Sleep(16);
            continue;
        }
        PumpButtons(state.Gamepad);
        Sleep(g_config.pollIntervalMs);
    }

    ReleaseAllOwnedInput();
    ReleaseExclusiveController();
    UninstallInputRecoveryHooks();
    LogLine("TL2TrueControlerSupport d3d9 integrated controller thread exited.");
    return 0;
}

void StartControllerSupportThread() {
    if (InterlockedCompareExchange(&g_controllerEnabledInThisProcess, 1, 1) == 0) return;
    if (InterlockedExchange(&g_supportThreadStarted, 1) != 0) return;
    HANDLE thread = CreateThread(nullptr, 0, ControllerSupportThread, nullptr, 0, nullptr);
    if (thread != nullptr) CloseHandle(thread);
}

} // namespace

extern "C" __declspec(noinline) FARPROC ResolveD3D9Proc(const char* name) {
    if (!LoadRealD3D9()) return nullptr;
    return GetProcAddress(g_realD3D9, name);
}

#define D3D9_PROXY_NAME(internalName, realName) extern "C" const char name_##internalName[] = realName;
#define D3D9_PROXY_STUB(internalName) \
    extern "C" __declspec(naked) void internalName() { \
        __asm { push offset name_##internalName } \
        __asm { call ResolveD3D9Proc } \
        __asm { add esp, 4 } \
        __asm { jmp eax } \
    }

D3D9_PROXY_NAME(Proxy_Direct3DCreate9, "Direct3DCreate9")
D3D9_PROXY_NAME(Proxy_Direct3DCreate9Ex, "Direct3DCreate9Ex")
D3D9_PROXY_NAME(Proxy_Direct3DShaderValidatorCreate9, "Direct3DShaderValidatorCreate9")
D3D9_PROXY_NAME(Proxy_PSGPError, "PSGPError")
D3D9_PROXY_NAME(Proxy_PSGPSampleTexture, "PSGPSampleTexture")
D3D9_PROXY_NAME(Proxy_D3DPERF_BeginEvent, "D3DPERF_BeginEvent")
D3D9_PROXY_NAME(Proxy_D3DPERF_EndEvent, "D3DPERF_EndEvent")
D3D9_PROXY_NAME(Proxy_D3DPERF_GetStatus, "D3DPERF_GetStatus")
D3D9_PROXY_NAME(Proxy_D3DPERF_QueryRepeatFrame, "D3DPERF_QueryRepeatFrame")
D3D9_PROXY_NAME(Proxy_D3DPERF_SetMarker, "D3DPERF_SetMarker")
D3D9_PROXY_NAME(Proxy_D3DPERF_SetOptions, "D3DPERF_SetOptions")
D3D9_PROXY_NAME(Proxy_D3DPERF_SetRegion, "D3DPERF_SetRegion")

D3D9_PROXY_STUB(Proxy_Direct3DCreate9)
D3D9_PROXY_STUB(Proxy_Direct3DCreate9Ex)
D3D9_PROXY_STUB(Proxy_Direct3DShaderValidatorCreate9)
D3D9_PROXY_STUB(Proxy_PSGPError)
D3D9_PROXY_STUB(Proxy_PSGPSampleTexture)
D3D9_PROXY_STUB(Proxy_D3DPERF_BeginEvent)
D3D9_PROXY_STUB(Proxy_D3DPERF_EndEvent)
D3D9_PROXY_STUB(Proxy_D3DPERF_GetStatus)
D3D9_PROXY_STUB(Proxy_D3DPERF_QueryRepeatFrame)
D3D9_PROXY_STUB(Proxy_D3DPERF_SetMarker)
D3D9_PROXY_STUB(Proxy_D3DPERF_SetOptions)
D3D9_PROXY_STUB(Proxy_D3DPERF_SetRegion)

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        g_module = module;
        BuildProxyDirectory(module);
        BuildPath(g_supportIniFile, kSupportIniName);
        BuildPath(g_movementIniFile, kMovementIniName);
        BuildPath(g_logFile, L"TL2TrueControlerSupport.d3d9loader.log");
        char line[160]{};
        wsprintfA(line, "d3d9 controller support loader loaded: %s %s.", kAddonName, kAddonVersion);
        LogLine(line);
        if (CurrentProcessIsTorchlight2()) {
            InterlockedExchange(&g_controllerEnabledInThisProcess, 1);
            StartControllerSupportThread();
        } else {
            LogLine("Process is not Torchlight2.exe; controller support thread not started for launcher/helper process.");
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_running, 0);
        ReleaseAllOwnedInput();
        UninstallInputRecoveryHooks();
        if (g_xinputModule != nullptr) {
            FreeLibrary(g_xinputModule);
            g_xinputModule = nullptr;
            g_xinputGetState = nullptr;
        }
        if (g_realD3D9 != nullptr) {
            FreeLibrary(g_realD3D9);
            g_realD3D9 = nullptr;
        }
        LogLine("d3d9 controller support loader unloading.");
    }
    return TRUE;
}
