#include <windows.h>
#include <Xinput.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr const char* kAppName = "TL2TrueControlerSupport";
constexpr const char* kAppVersion = "v0.0.9";
constexpr const char* kAppAuthor = "ERY_Asylum";
constexpr const wchar_t* kSupportIniName = L"TL2TrueControlerSupport.ini";
constexpr const wchar_t* kDefaultMovementIniName = L"TL2TrueKeyboardMove.ini";
constexpr const wchar_t* kTorchlightExeName = L"Torchlight2.exe";
constexpr const wchar_t* kConfigMutexName = L"Local\\TL2TrueControlerSupport_ConfigActive";
constexpr const char* kOverlayClassName = "TL2TrueKeyboardMoveOverlay";
constexpr const char* kOverlayVisibleProp = "TL2TrueKeyboardMove.D3D9OverlayVisible";

constexpr DWORD kConfigPollMs = 500;
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
};

struct ActionBinding {
    ActionKind kind = ActionKind::None;
    WORD vk = 0;
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
    bool leftStickEnabled = true;
    bool rightStickMouse = true;
    bool followKeyboardProfile = true;
    DWORD controllerIndex = 0;
    DWORD pollIntervalMs = 8;
    int leftDeadzone = kDefaultLeftDeadzone;
    int rightDeadzone = kDefaultRightDeadzone;
    int activationThresholdX1000 = 350;
    int releaseThresholdX1000 = 250;
    int mouseSensitivityX = 14;
    int mouseSensitivityY = 14;
    bool invertMouseY = false;
    BYTE triggerThreshold = 30;
    MovementKeys movement{};
};

HMODULE g_xinputModule = nullptr;
XInputGetStateFn g_xinputGetState = nullptr;
wchar_t g_appDir[MAX_PATH]{};
wchar_t g_supportIniFile[MAX_PATH]{};
wchar_t g_movementIniFile[MAX_PATH]{};
wchar_t g_logFile[MAX_PATH]{};
volatile LONG g_running = 1;
Config g_config{};
FILETIME g_lastSupportIniWriteTime{};
FILETIME g_lastMovementIniWriteTime{};
DWORD g_lastConfigPollTick = 0;
int g_ownedMovementMask = 0;
StickLatch g_leftLatch{};
bool g_wasConnected = false;
bool g_loggedDisabled = false;
bool g_loggedMissingXInput = false;
bool g_loggedNoFocus = false;
bool g_loggedConfigToolPause = false;

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

void BuildAppDirectory() {
    const DWORD written = GetModuleFileNameW(nullptr, g_appDir, MAX_PATH);
    if (written == 0 || written >= MAX_PATH) {
        g_appDir[0] = L'\0';
        return;
    }
    for (DWORD i = written; i > 0; --i) {
        if (g_appDir[i - 1] == L'\\' || g_appDir[i - 1] == L'/') {
            g_appDir[i] = L'\0';
            return;
        }
    }
    g_appDir[0] = L'\0';
}

void BuildPath(wchar_t* out, const wchar_t* fileName) {
    wcscpy_s(out, MAX_PATH, g_appDir);
    wcscat_s(out, MAX_PATH, fileName);
}

void LogLine(const char* text) {
    if (g_logFile[0] == L'\0') {
        return;
    }
    HANDLE file = CreateFileW(
        g_logFile,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
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
    if (path == nullptr || path[0] == L'\0' || writeTimeOut == nullptr) {
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) {
        return false;
    }
    *writeTimeOut = data.ftLastWriteTime;
    return true;
}

bool FileTimesEqual(const FILETIME& left, const FILETIME& right) {
    return left.dwLowDateTime == right.dwLowDateTime &&
        left.dwHighDateTime == right.dwHighDateTime;
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
    if (vk == 0) {
        return;
    }
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (IsExtendedKey(vk)) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    SendInput(1, &input, sizeof(input));
}

void SendMouseButton(ActionKind kind, bool down) {
    DWORD flags = 0;
    switch (kind) {
        case ActionKind::MouseLeft:
            flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case ActionKind::MouseRight:
            flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case ActionKind::MouseMiddle:
            flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
        default:
            return;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(input));
}

void ApplyAction(const ActionBinding& action, bool down) {
    switch (action.kind) {
        case ActionKind::Key:
            SendKey(action.vk, down);
            break;
        case ActionKind::MouseLeft:
        case ActionKind::MouseRight:
        case ActionKind::MouseMiddle:
            SendMouseButton(action.kind, down);
            break;
        default:
            break;
    }
}

void ReleaseButtonBinding(ButtonBinding& binding) {
    if (binding.ownedDown) {
        ApplyAction(binding.action, false);
        binding.ownedDown = false;
    }
}

void ReleaseTriggerBinding(TriggerBinding& binding) {
    if (binding.ownedDown) {
        ApplyAction(binding.action, false);
        binding.ownedDown = false;
    }
}

void ReleaseAllButtons() {
    for (ButtonBinding& button : g_buttons) {
        ReleaseButtonBinding(button);
    }
    ReleaseTriggerBinding(g_leftTrigger);
    ReleaseTriggerBinding(g_rightTrigger);
}

void ApplyMovementKey(int bit, WORD vk, int newMask) {
    const bool wasDown = (g_ownedMovementMask & bit) != 0;
    const bool isDown = (newMask & bit) != 0;
    if (wasDown == isDown) {
        return;
    }
    SendKey(vk, isDown);
}

void ApplyMovementMask(int newMask) {
    ApplyMovementKey(kDirUp, g_config.movement.up, newMask);
    ApplyMovementKey(kDirLeft, g_config.movement.left, newMask);
    ApplyMovementKey(kDirDown, g_config.movement.down, newMask);
    ApplyMovementKey(kDirRight, g_config.movement.right, newMask);
    g_ownedMovementMask = newMask;
}

void ReleaseMovement() {
    ApplyMovementMask(0);
    g_leftLatch = {};
}

void ReleaseAllOwnedInput() {
    ReleaseMovement();
    ReleaseAllButtons();
}

WORD ParseVirtualKeyName(const wchar_t* text, WORD fallback) {
    if (text == nullptr || text[0] == L'\0') {
        return fallback;
    }

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
    if (lstrcmpW(key, L"LALT") == 0) return VK_LMENU;
    if (lstrcmpW(key, L"RALT") == 0) return VK_RMENU;
    if (lstrcmpW(key, L"DELETE") == 0 || lstrcmpW(key, L"DEL") == 0) return VK_DELETE;
    if (lstrcmpW(key, L"END") == 0) return VK_END;
    if (lstrcmpW(key, L"HOME") == 0) return VK_HOME;

    if (key[0] == L'F') {
        const int fNumber = _wtoi(key + 1);
        if (fNumber >= 1 && fNumber <= 12) {
            return static_cast<WORD>(VK_F1 + fNumber - 1);
        }
    }

    if (key[0] == L'V' && key[1] == L'K') {
        const int parsed = static_cast<int>(wcstol(key + 2, nullptr, 16));
        if (parsed > 0 && parsed <= 0xFF) {
            return static_cast<WORD>(parsed);
        }
    }

    return fallback;
}

ActionBinding ParseActionName(const wchar_t* text) {
    ActionBinding action{};
    if (text == nullptr || text[0] == L'\0') {
        return action;
    }

    wchar_t value[64]{};
    lstrcpynW(value, text, 64);
    CharUpperBuffW(value, lstrlenW(value));

    if (lstrcmpW(value, L"NONE") == 0 || lstrcmpW(value, L"DISABLED") == 0) {
        return action;
    }
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

    action.vk = ParseVirtualKeyName(value, 0);
    if (action.vk != 0) {
        action.kind = ActionKind::Key;
    }
    return action;
}

HKL GetForegroundKeyboardLayout() {
    HWND foreground = GetForegroundWindow();
    if (foreground != nullptr) {
        DWORD processId = 0;
        const DWORD threadId = GetWindowThreadProcessId(foreground, &processId);
        if (threadId != 0) {
            HKL layout = GetKeyboardLayout(threadId);
            if (layout != nullptr) {
                return layout;
            }
        }
    }
    return GetKeyboardLayout(0);
}

WORD VirtualKeyFromScanCode(LONG scanCode, WORD fallback) {
    if (scanCode == 0) {
        return fallback;
    }
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
    MovementKeys defaults{};
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
        config->movement.up = VirtualKeyFromScanCode(
            GetPrivateProfileIntW(L"Keys", L"ScanUp", 0x11, g_movementIniFile),
            defaults.up);
        config->movement.left = VirtualKeyFromScanCode(
            GetPrivateProfileIntW(L"Keys", L"ScanLeft", 0x1E, g_movementIniFile),
            defaults.left);
        config->movement.down = VirtualKeyFromScanCode(
            GetPrivateProfileIntW(L"Keys", L"ScanDown", 0x1F, g_movementIniFile),
            defaults.down);
        config->movement.right = VirtualKeyFromScanCode(
            GetPrivateProfileIntW(L"Keys", L"ScanRight", 0x20, g_movementIniFile),
            defaults.right);
        return;
    }

    config->movement.up = ReadMovementKey(L"Up", L"W", defaults.up);
    config->movement.left = ReadMovementKey(L"Left", L"A", defaults.left);
    config->movement.down = ReadMovementKey(L"Down", L"S", defaults.down);
    config->movement.right = ReadMovementKey(L"Right", L"D", defaults.right);
}

void ReadActionBinding(const wchar_t* iniName, const wchar_t* fallback, ActionBinding* actionOut) {
    wchar_t value[64]{};
    GetPrivateProfileStringW(
        L"Buttons",
        iniName,
        fallback,
        value,
        64,
        g_supportIniFile);
    *actionOut = ParseActionName(value);
}

void ResolveMovementIniPath() {
    wchar_t movementIniName[MAX_PATH]{};
    GetPrivateProfileStringW(
        L"General",
        L"MovementProfileIni",
        kDefaultMovementIniName,
        movementIniName,
        MAX_PATH,
        g_supportIniFile);
    if (movementIniName[0] == L'\0') {
        BuildPath(g_movementIniFile, kDefaultMovementIniName);
        return;
    }
    if (wcschr(movementIniName, L'\\') != nullptr || wcschr(movementIniName, L':') != nullptr) {
        wcscpy_s(g_movementIniFile, movementIniName);
    } else {
        BuildPath(g_movementIniFile, movementIniName);
    }
}

void LoadConfig() {
    ReleaseAllOwnedInput();
    ResolveMovementIniPath();

    Config next{};
    next.enable = GetPrivateProfileIntW(L"General", L"Enable", 1, g_supportIniFile) != 0;
    next.controllerIndex = static_cast<DWORD>(ClampLong(
        GetPrivateProfileIntW(L"General", L"ControllerIndex", 0, g_supportIniFile),
        0,
        3));
    next.pollIntervalMs = static_cast<DWORD>(ClampLong(
        GetPrivateProfileIntW(L"General", L"PollIntervalMs", 8, g_supportIniFile),
        4,
        33));
    next.focusRequired = GetPrivateProfileIntW(L"General", L"FocusRequired", 1, g_supportIniFile) != 0;
    next.suspendWhileOverlayVisible =
        GetPrivateProfileIntW(L"General", L"SuspendWhileOverlayVisible", 1, g_supportIniFile) != 0;
    next.leftStickEnabled = GetPrivateProfileIntW(L"Movement", L"LeftStickEnabled", 1, g_supportIniFile) != 0;
    next.rightStickMouse = GetPrivateProfileIntW(L"Mouse", L"RightStickMouse", 1, g_supportIniFile) != 0;
    next.followKeyboardProfile =
        GetPrivateProfileIntW(L"Movement", L"FollowKeyboardProfile", 1, g_supportIniFile) != 0;
    next.leftDeadzone = ClampLong(
        GetPrivateProfileIntW(L"Movement", L"Deadzone", kDefaultLeftDeadzone, g_supportIniFile),
        0,
        32767);
    next.rightDeadzone = ClampLong(
        GetPrivateProfileIntW(L"Mouse", L"Deadzone", kDefaultRightDeadzone, g_supportIniFile),
        0,
        32767);
    next.activationThresholdX1000 = ClampLong(
        GetPrivateProfileIntW(L"Movement", L"ActivationThresholdX1000", 350, g_supportIniFile),
        1,
        1000);
    next.releaseThresholdX1000 = ClampLong(
        GetPrivateProfileIntW(L"Movement", L"ReleaseThresholdX1000", 250, g_supportIniFile),
        0,
        next.activationThresholdX1000);
    next.mouseSensitivityX = ClampLong(
        GetPrivateProfileIntW(L"Mouse", L"SensitivityX", 14, g_supportIniFile),
        1,
        80);
    next.mouseSensitivityY = ClampLong(
        GetPrivateProfileIntW(L"Mouse", L"SensitivityY", 14, g_supportIniFile),
        1,
        80);
    next.invertMouseY = GetPrivateProfileIntW(L"Mouse", L"InvertY", 0, g_supportIniFile) != 0;
    next.triggerThreshold = static_cast<BYTE>(ClampLong(
        GetPrivateProfileIntW(L"Buttons", L"TriggerThreshold", 30, g_supportIniFile),
        1,
        255));

    LoadMovementBindings(&next);

    ReadActionBinding(L"A", L"MouseLeft", &g_buttons[0].action);
    ReadActionBinding(L"B", L"Escape", &g_buttons[1].action);
    ReadActionBinding(L"X", L"MouseRight", &g_buttons[2].action);
    ReadActionBinding(L"Y", L"Space", &g_buttons[3].action);
    ReadActionBinding(L"LeftShoulder", L"Shift", &g_buttons[4].action);
    ReadActionBinding(L"RightShoulder", L"Ctrl", &g_buttons[5].action);
    ReadActionBinding(L"Back", L"Tab", &g_buttons[6].action);
    ReadActionBinding(L"Start", L"Escape", &g_buttons[7].action);
    ReadActionBinding(L"LeftThumb", L"None", &g_buttons[8].action);
    ReadActionBinding(L"RightThumb", L"None", &g_buttons[9].action);
    ReadActionBinding(L"DPadUp", L"1", &g_buttons[10].action);
    ReadActionBinding(L"DPadRight", L"2", &g_buttons[11].action);
    ReadActionBinding(L"DPadDown", L"3", &g_buttons[12].action);
    ReadActionBinding(L"DPadLeft", L"4", &g_buttons[13].action);
    ReadActionBinding(L"LeftTrigger", L"MouseLeft", &g_leftTrigger.action);
    ReadActionBinding(L"RightTrigger", L"MouseRight", &g_rightTrigger.action);

    g_config = next;
    QueryWriteTime(g_supportIniFile, &g_lastSupportIniWriteTime);
    QueryWriteTime(g_movementIniFile, &g_lastMovementIniWriteTime);

    char line[448]{};
    wsprintfA(
        line,
        "Config loaded: enabled=%d controller=%lu movementIni=%ls leftStick=%d rightMouse=%d movementVK=(%02X,%02X,%02X,%02X) mouseSensitivity=(%d,%d).",
        g_config.enable ? 1 : 0,
        g_config.controllerIndex,
        g_movementIniFile,
        g_config.leftStickEnabled ? 1 : 0,
        g_config.rightStickMouse ? 1 : 0,
        g_config.movement.up,
        g_config.movement.left,
        g_config.movement.down,
        g_config.movement.right,
        g_config.mouseSensitivityX,
        g_config.mouseSensitivityY);
    LogLine(line);
}

void PollConfigReload(DWORD now) {
    if (now - g_lastConfigPollTick < kConfigPollMs) {
        return;
    }
    g_lastConfigPollTick = now;

    FILETIME supportTime{};
    FILETIME movementTime{};
    const bool supportChanged =
        QueryWriteTime(g_supportIniFile, &supportTime) &&
        !FileTimesEqual(supportTime, g_lastSupportIniWriteTime);
    const bool movementChanged =
        QueryWriteTime(g_movementIniFile, &movementTime) &&
        !FileTimesEqual(movementTime, g_lastMovementIniWriteTime);

    if (supportChanged || movementChanged) {
        LoadConfig();
    }
}

bool LoadXInput() {
    if (g_xinputGetState != nullptr) {
        return true;
    }

    const wchar_t* candidates[] = {
        L"xinput1_4.dll",
        L"xinput1_3.dll",
        L"xinput9_1_0.dll",
    };
    for (const wchar_t* candidate : candidates) {
        g_xinputModule = LoadLibraryW(candidate);
        if (g_xinputModule == nullptr) {
            continue;
        }
        g_xinputGetState = reinterpret_cast<XInputGetStateFn>(
            GetProcAddress(g_xinputModule, "XInputGetState"));
        if (g_xinputGetState != nullptr) {
            char line[128]{};
            wsprintfA(line, "XInput backend loaded from %ls.", candidate);
            LogLine(line);
            return true;
        }
        FreeLibrary(g_xinputModule);
        g_xinputModule = nullptr;
    }
    return false;
}

const wchar_t* BaseName(const wchar_t* path) {
    if (path == nullptr) {
        return L"";
    }
    const wchar_t* base = path;
    for (const wchar_t* p = path; *p != L'\0'; ++p) {
        if (*p == L'\\' || *p == L'/') {
            base = p + 1;
        }
    }
    return base;
}

bool ForegroundIsTorchlight2() {
    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    if (processId == 0) {
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return false;
    }

    wchar_t imagePath[MAX_PATH]{};
    DWORD length = MAX_PATH;
    const bool ok = QueryFullProcessImageNameW(process, 0, imagePath, &length) != 0;
    CloseHandle(process);
    if (!ok) {
        return false;
    }

    return _wcsicmp(BaseName(imagePath), kTorchlightExeName) == 0;
}

bool OverlayLooksVisible() {
    HWND overlay = FindWindowA(kOverlayClassName, nullptr);
    if (overlay == nullptr) {
        return false;
    }
    if (IsWindowVisible(overlay)) {
        return true;
    }
    return GetPropA(overlay, kOverlayVisibleProp) != nullptr;
}

float NormalizeThumbAxis(SHORT value, int deadzone) {
    const float raw = static_cast<float>(value);
    const float sign = raw < 0.0f ? -1.0f : 1.0f;
    const float magnitude = fabsf(raw);
    if (magnitude <= static_cast<float>(deadzone)) {
        return 0.0f;
    }
    const float normalized =
        (magnitude - static_cast<float>(deadzone)) /
        (32767.0f - static_cast<float>(deadzone));
    const float clamped = normalized > 1.0f ? 1.0f : normalized;
    return clamped * sign;
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
    if (!g_config.leftStickEnabled) {
        return 0;
    }

    const float x = NormalizeThumbAxis(gamepad.sThumbLX, g_config.leftDeadzone);
    const float y = NormalizeThumbAxis(gamepad.sThumbLY, g_config.leftDeadzone);

    g_leftLatch.left = LatchAxisNegative(
        x,
        g_leftLatch.left,
        g_config.activationThresholdX1000,
        g_config.releaseThresholdX1000);
    g_leftLatch.right = LatchAxisPositive(
        x,
        g_leftLatch.right,
        g_config.activationThresholdX1000,
        g_config.releaseThresholdX1000);
    g_leftLatch.up = LatchAxisPositive(
        y,
        g_leftLatch.up,
        g_config.activationThresholdX1000,
        g_config.releaseThresholdX1000);
    g_leftLatch.down = LatchAxisNegative(
        y,
        g_leftLatch.down,
        g_config.activationThresholdX1000,
        g_config.releaseThresholdX1000);

    int mask = 0;
    if (g_leftLatch.up) mask |= kDirUp;
    if (g_leftLatch.left) mask |= kDirLeft;
    if (g_leftLatch.down) mask |= kDirDown;
    if (g_leftLatch.right) mask |= kDirRight;
    return mask;
}

void PumpRightStickMouse(const XINPUT_GAMEPAD& gamepad) {
    if (!g_config.rightStickMouse) {
        return;
    }

    const float x = NormalizeThumbAxis(gamepad.sThumbRX, g_config.rightDeadzone);
    const float y = NormalizeThumbAxis(gamepad.sThumbRY, g_config.rightDeadzone);
    const LONG dx = static_cast<LONG>(x * static_cast<float>(g_config.mouseSensitivityX));
    const LONG dy = static_cast<LONG>(
        (g_config.invertMouseY ? y : -y) * static_cast<float>(g_config.mouseSensitivityY));
    if (dx == 0 && dy == 0) {
        return;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(input));
}

void PumpButton(ButtonBinding& binding, WORD pressedButtons) {
    const bool isDown = (pressedButtons & binding.xinputButton) != 0;
    if (isDown == binding.ownedDown) {
        return;
    }
    ApplyAction(binding.action, isDown);
    binding.ownedDown = isDown;
}

void PumpTrigger(TriggerBinding& binding, BYTE value) {
    const bool isDown = value >= g_config.triggerThreshold;
    if (isDown == binding.ownedDown) {
        return;
    }
    ApplyAction(binding.action, isDown);
    binding.ownedDown = isDown;
}

void PumpButtons(const XINPUT_GAMEPAD& gamepad) {
    for (ButtonBinding& button : g_buttons) {
        PumpButton(button, gamepad.wButtons);
    }
    PumpTrigger(g_leftTrigger, gamepad.bLeftTrigger);
    PumpTrigger(g_rightTrigger, gamepad.bRightTrigger);
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

BOOL WINAPI ConsoleControlHandler(DWORD controlType) {
    switch (controlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            InterlockedExchange(&g_running, 0);
            ReleaseAllOwnedInput();
            return TRUE;
        default:
            return FALSE;
    }
}

void RunLoop() {
    if (!LoadXInput()) {
        LogLine("XInput backend unavailable; support will stay inactive.");
        g_loggedMissingXInput = true;
    }

    LoadConfig();

    while (InterlockedCompareExchange(&g_running, 1, 1) != 0) {
        const DWORD now = GetTickCount();
        PollConfigReload(now);

        if (ConfigToolActive()) {
            ReleaseAllOwnedInput();
            if (!g_loggedConfigToolPause) {
                g_loggedConfigToolPause = true;
                LogLine("Configurator active; controller injection paused.");
                puts("Configurator active; controller injection paused.");
            }
            Sleep(33);
            continue;
        }
        g_loggedConfigToolPause = false;

        if (!g_config.enable) {
            ReleaseAllOwnedInput();
            if (!g_loggedDisabled) {
                g_loggedDisabled = true;
                LogLine("Support disabled by ini.");
                puts("TL2TrueControlerSupport is disabled in the ini.");
            }
            Sleep(100);
            continue;
        }
        g_loggedDisabled = false;

        if (g_config.focusRequired && !ForegroundIsTorchlight2()) {
            ReleaseAllOwnedInput();
            if (!g_loggedNoFocus) {
                g_loggedNoFocus = true;
                LogLine("Waiting for Torchlight2.exe foreground focus.");
            }
            Sleep(33);
            continue;
        }
        g_loggedNoFocus = false;

        if (g_config.suspendWhileOverlayVisible && OverlayLooksVisible()) {
            ReleaseAllOwnedInput();
            Sleep(33);
            continue;
        }

        if (!LoadXInput()) {
            if (!g_loggedMissingXInput) {
                g_loggedMissingXInput = true;
                LogLine("XInput backend unavailable; support inactive.");
            }
            ReleaseAllOwnedInput();
            Sleep(250);
            continue;
        }

        XINPUT_STATE state{};
        const DWORD result = g_xinputGetState(g_config.controllerIndex, &state);
        if (result != ERROR_SUCCESS) {
            if (g_wasConnected) {
                LogLine("XInput controller disconnected; released owned input.");
                puts("Controller disconnected.");
            }
            g_wasConnected = false;
            ReleaseAllOwnedInput();
            Sleep(100);
            continue;
        }

        if (!g_wasConnected) {
            g_wasConnected = true;
            LogLine("XInput controller connected.");
            puts("Controller connected.");
        }

        ApplyMovementMask(MovementMaskFromLeftStick(state.Gamepad));
        PumpRightStickMouse(state.Gamepad);
        PumpButtons(state.Gamepad);

        Sleep(g_config.pollIntervalMs);
    }

    ReleaseAllOwnedInput();
}

} // namespace

int wmain() {
    HANDLE singleInstance = CreateMutexW(
        nullptr,
        TRUE,
        L"Local\\TL2TrueControlerSupport_v0_0_1");
    if (singleInstance != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        puts("TL2TrueControlerSupport is already running.");
        CloseHandle(singleInstance);
        return 0;
    }

    BuildAppDirectory();
    BuildPath(g_supportIniFile, kSupportIniName);
    BuildPath(g_movementIniFile, kDefaultMovementIniName);
    BuildPath(g_logFile, L"TL2TrueControlerSupport.log");
    SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);

    char startupLine[192]{};
    wsprintfA(
        startupLine,
        "%s %s started. Author: %s.",
        kAppName,
        kAppVersion,
        kAppAuthor);
    LogLine(startupLine);

    printf("%s %s\n", kAppName, kAppVersion);
    puts("Independent controller companion. Close this window to stop it.");
    puts("Waiting for Torchlight2.exe focus and an XInput controller...");

    RunLoop();

    if (g_xinputModule != nullptr) {
        FreeLibrary(g_xinputModule);
        g_xinputModule = nullptr;
        g_xinputGetState = nullptr;
    }
    LogLine("TL2TrueControlerSupport exited cleanly.");
    if (singleInstance != nullptr) {
        ReleaseMutex(singleInstance);
        CloseHandle(singleInstance);
    }
    return 0;
}
