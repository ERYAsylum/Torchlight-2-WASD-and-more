#include <windows.h>
#include <commctrl.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <shlobj.h>
#include <xinput.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr const wchar_t* kWindowClass = L"TL2TrueControlerSupportConfigWindow";
constexpr const wchar_t* kConfigMutexName = L"Local\\TL2TrueControlerSupport_ConfigActive";
constexpr const wchar_t* kIniName = L"TL2TrueControlerSupport.ini";
constexpr const wchar_t* kKeyBindingsName = L"KeyBindings.dat";
constexpr UINT_PTR kPollTimerId = 1;
constexpr DWORD kTriggerThreshold = 45;
constexpr DWORD kDesktopSuppressMs = 120;
constexpr SHORT kDesktopStickThreshold = 6000;

enum ControlId {
    IDC_ACTION_LIST = 1001,
    IDC_STATUS,
    IDC_CAPTURE,
    IDC_CAPTURE_COMBO,
    IDC_SKIP,
    IDC_CLEAR,
    IDC_DEFAULTS,
    IDC_SAVE,
    IDC_CLOSE,
};

struct GameAction {
    std::wstring label;
    std::wstring name;
    std::wstring outputValue;
    int key = -1;
    int modifier = -1;
    std::wstring assignedControl;
    bool movementOnly = false;
};

struct ControlRow {
    const wchar_t* iniName;
    const wchar_t* label;
};

const ControlRow kControls[] = {
    {L"A", L"A"},
    {L"B", L"B"},
    {L"X", L"X"},
    {L"Y", L"Y"},
    {L"LeftShoulder", L"LB"},
    {L"RightShoulder", L"RB"},
    {L"Back", L"Back"},
    {L"Start", L"Start"},
    {L"LeftThumb", L"L3"},
    {L"RightThumb", L"R3"},
    {L"DPadUp", L"D-pad Up"},
    {L"DPadRight", L"D-pad Right"},
    {L"DPadDown", L"D-pad Down"},
    {L"DPadLeft", L"D-pad Left"},
    {L"LeftTrigger", L"LT"},
    {L"RightTrigger", L"RT"},
    {L"LeftStickUp", L"Left Stick Up"},
    {L"LeftStickRight", L"Left Stick Right"},
    {L"LeftStickDown", L"Left Stick Down"},
    {L"LeftStickLeft", L"Left Stick Left"},
};

const wchar_t* kDefaultManagedChordControls[] = {
    L"LeftTrigger+A",
    L"LeftTrigger+X",
    L"LeftTrigger+B",
    L"LeftTrigger+Y",
    L"RightShoulder+DPadUp",
    L"RightShoulder+DPadRight",
    L"RightShoulder+DPadDown",
    L"RightShoulder+DPadLeft",
    L"RightShoulder+Y",
    L"RightShoulder+B",
    L"RightShoulder+A",
};

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_list = nullptr;
HWND g_status = nullptr;
HWND g_captureButton = nullptr;
HWND g_captureComboButton = nullptr;
HWND g_skipButton = nullptr;
HWND g_clearButton = nullptr;
HWND g_defaultsButton = nullptr;
HWND g_saveButton = nullptr;
HWND g_closeButton = nullptr;

std::wstring g_appDir;
std::wstring g_iniPath;
std::wstring g_keyBindingsPath;
std::vector<GameAction> g_actions;
std::map<std::wstring, std::wstring> g_originalButtonValues;
bool g_captureMode = false;
bool g_comboCaptureMode = false;
DWORD g_previousButtons = 0;
bool g_previousLeftTrigger = false;
bool g_previousRightTrigger = false;
HMODULE g_xinput = nullptr;
XInputGetStateFn g_xInputGetState = nullptr;
HANDLE g_configMutex = nullptr;
IDirectInput8W* g_directInput = nullptr;
IDirectInputDevice8W* g_exclusiveDevice = nullptr;
bool g_exclusiveAttempted = false;
bool g_exclusiveAcquired = false;
HHOOK g_keyboardBlockHook = nullptr;
HHOOK g_mouseBlockHook = nullptr;
volatile LONG g_desktopSuppressUntilTick = 0;

std::wstring AppDirectory() {
    wchar_t path[MAX_PATH]{};
    const DWORD written = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (written == 0 || written >= MAX_PATH) return L"";
    for (DWORD i = written; i > 0; --i) {
        if (path[i - 1] == L'\\' || path[i - 1] == L'/') {
            path[i] = L'\0';
            return path;
        }
    }
    return L"";
}

std::wstring JoinPath(const std::wstring& dir, const std::wstring& name) {
    return dir + name;
}

bool QueryWriteTime(const std::wstring& path, FILETIME* writeTimeOut) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (writeTimeOut == nullptr ||
        !GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return false;
    }
    *writeTimeOut = data.ftLastWriteTime;
    return true;
}

bool ReadUtf16File(const std::wstring& path, std::wstring* out) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size < 2 || size > 2 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }

    std::wstring text;
    text.resize(size / sizeof(wchar_t));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, &text[0], size, &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) return false;
    if (!text.empty() && text[0] == 0xFEFF) {
        text.erase(text.begin());
    }
    *out = text;
    return true;
}

std::wstring DetectKeyBindingsPath() {
    wchar_t configured[MAX_PATH]{};
    GetPrivateProfileStringW(
        L"General",
        L"KeyBindingsDat",
        L"AUTO",
        configured,
        MAX_PATH,
        g_iniPath.c_str());

    if (configured[0] != L'\0' &&
        _wcsicmp(configured, L"AUTO") != 0 &&
        _wcsicmp(configured, L"DEFAULT") != 0) {
        return configured;
    }

    wchar_t documents[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents) != S_OK) {
        return L"";
    }

    const std::wstring rootKeyBindings = std::wstring(documents) +
        L"\\My Games\\runic games\\torchlight 2\\" +
        kKeyBindingsName;
    FILETIME rootTime{};
    if (QueryWriteTime(rootKeyBindings, &rootTime)) {
        return rootKeyBindings;
    }

    const std::wstring saveRoot = std::wstring(documents) +
        L"\\My Games\\runic games\\torchlight 2\\save\\*";
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(saveRoot.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return L"";

    std::wstring best;
    FILETIME bestTime{};
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            wcscmp(data.cFileName, L".") == 0 ||
            wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }

        const std::wstring candidate = std::wstring(documents) +
            L"\\My Games\\runic games\\torchlight 2\\save\\" +
            data.cFileName +
            L"\\" +
            kKeyBindingsName;
        FILETIME writeTime{};
        if (QueryWriteTime(candidate, &writeTime) &&
            (best.empty() || CompareFileTime(&writeTime, &bestTime) > 0)) {
            best = candidate;
            bestTime = writeTime;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return best;
}

std::wstring Trim(const std::wstring& value) {
    const size_t begin = value.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) return L"";
    const size_t end = value.find_last_not_of(L" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool ExtractField(const std::wstring& block, const wchar_t* marker, std::wstring* out) {
    const size_t markerPos = block.find(marker);
    if (markerPos == std::wstring::npos) return false;
    const size_t valueStart = markerPos + wcslen(marker);
    const size_t lineEnd = block.find_first_of(L"\r\n", valueStart);
    *out = Trim(block.substr(
        valueStart,
        lineEnd == std::wstring::npos ? std::wstring::npos : lineEnd - valueStart));
    return true;
}

bool IsKeyboardBindable(const GameAction& action) {
    return action.key > 0;
}

const wchar_t* InGameLabelForAction(const std::wstring& name) {
    if (_wcsicmp(name.c_str(), L"MOVE/ATTACK") == 0) return L"Move/Attack";
    if (_wcsicmp(name.c_str(), L"MOVE") == 0) return L"Move";
    if (_wcsicmp(name.c_str(), L"CASTSKILL") == 0) return L"Cast Active Skill";
    if (_wcsicmp(name.c_str(), L"INVENTORYMENU") == 0) return L"Inventory";
    if (_wcsicmp(name.c_str(), L"PETMENU") == 0) return L"Pet Inventory";
    if (_wcsicmp(name.c_str(), L"HOLDPOSITION") == 0) return L"Hold Position";
    if (_wcsicmp(name.c_str(), L"ATTACKONLYTARGETS") == 0) return L"Attack Only Valid Targets";
    if (_wcsicmp(name.c_str(), L"AUTOMAP") == 0) return L"Automap";
    if (_wcsicmp(name.c_str(), L"AUTOMAPRESET") == 0) return L"Automap Reset";
    if (_wcsicmp(name.c_str(), L"AUTOMAPVISIBLE") == 0) return L"Map Visibility";
    if (_wcsicmp(name.c_str(), L"SKILLMENU") == 0) return L"Skill Menu";
    if (_wcsicmp(name.c_str(), L"STATMENU") == 0) return L"Character Stats Menu";
    if (_wcsicmp(name.c_str(), L"OPTIONSMENU") == 0) return L"Options Menu";
    if (_wcsicmp(name.c_str(), L"CLOSEALL") == 0) return L"Close All Menus";
    if (_wcsicmp(name.c_str(), L"JOURNAL") == 0) return L"Arcane Statistics";
    if (_wcsicmp(name.c_str(), L"WEAPONSWAP") == 0) return L"Weapon Swap";
    if (_wcsicmp(name.c_str(), L"SKILLSWAP") == 0) return L"Active Skill Swap";
    if (_wcsicmp(name.c_str(), L"QUESTMENU") == 0) return L"Quest Menu";
    if (_wcsicmp(name.c_str(), L"SHOWITEMS") == 0) return L"Show Items";
    if (_wcsicmp(name.c_str(), L"CYCLESKILLDOWN") == 0) return L"Cycle Skills Down";
    if (_wcsicmp(name.c_str(), L"CYCLESKILLUP") == 0) return L"Cycle Skills Up";
    if (_wcsicmp(name.c_str(), L"AUTOMAPZOOMOUT") == 0) return L"Automap Zoom Out";
    if (_wcsicmp(name.c_str(), L"AUTOMAPZOOMIN") == 0) return L"Automap Zoom In";
    if (_wcsicmp(name.c_str(), L"CAMERAZOOMOUT") == 0) return L"Camera Zoom Out";
    if (_wcsicmp(name.c_str(), L"CAMERAZOOMIN") == 0) return L"Camera Zoom In";
    if (_wcsicmp(name.c_str(), L"RESETCAMERA") == 0) return L"Reset Camera";
    if (_wcsicmp(name.c_str(), L"QUICKSAVE") == 0) return L"Quick Save";
    if (_wcsicmp(name.c_str(), L"QK1") == 0) return L"Quickslot 1";
    if (_wcsicmp(name.c_str(), L"QK2") == 0) return L"Quickslot 2";
    if (_wcsicmp(name.c_str(), L"QK3") == 0) return L"Quickslot 3";
    if (_wcsicmp(name.c_str(), L"QK4") == 0) return L"Quickslot 4";
    if (_wcsicmp(name.c_str(), L"QK5") == 0) return L"Quickslot 5";
    if (_wcsicmp(name.c_str(), L"QK6") == 0) return L"Quickslot 6";
    if (_wcsicmp(name.c_str(), L"QK7") == 0) return L"Quickslot 7";
    if (_wcsicmp(name.c_str(), L"QK8") == 0) return L"Quickslot 8";
    if (_wcsicmp(name.c_str(), L"QK9") == 0) return L"Quickslot 9";
    if (_wcsicmp(name.c_str(), L"QK0") == 0) return L"Quickslot 0";
    if (_wcsicmp(name.c_str(), L"BESTPOTION") == 0) return L"Best Health Potion";
    if (_wcsicmp(name.c_str(), L"BESTMANA") == 0) return L"Best Mana Potion";
    if (_wcsicmp(name.c_str(), L"BESTPETPOTION") == 0) return L"Best Pet Health Potion";
    if (_wcsicmp(name.c_str(), L"BESTPETMANA") == 0) return L"Best Pet Mana Potion";
    return nullptr;
}

std::wstring DisplayLabelForAction(const std::wstring& name) {
    const wchar_t* known = InGameLabelForAction(name);
    if (known != nullptr) return known;
    if (name.size() >= 2 && name[0] == L'F') {
        const int fNumber = _wtoi(name.c_str() + 1);
        if (fNumber >= 1 && fNumber <= 12) return L"Function " + std::to_wstring(fNumber);
    }
    return name;
}

int InGameOrder(const std::wstring& name) {
    const wchar_t* order[] = {
        L"MOVE/ATTACK", L"MOVE", L"CASTSKILL", L"INVENTORYMENU", L"PETMENU",
        L"HOLDPOSITION", L"ATTACKONLYTARGETS", L"AUTOMAP", L"AUTOMAPRESET",
        L"AUTOMAPVISIBLE", L"SKILLMENU", L"STATMENU", L"OPTIONSMENU", L"CLOSEALL",
        L"JOURNAL", L"WEAPONSWAP", L"SKILLSWAP", L"QUESTMENU", L"SHOWITEMS",
        L"CYCLESKILLDOWN", L"CYCLESKILLUP", L"AUTOMAPZOOMOUT", L"AUTOMAPZOOMIN",
        L"CAMERAZOOMOUT", L"CAMERAZOOMIN", L"RESETCAMERA", L"F1", L"F2", L"F3",
        L"F4", L"F5", L"F6", L"F7", L"F8", L"F9", L"F10", L"F11", L"F12",
        L"QUICKSAVE", L"QK1", L"QK2", L"QK3", L"QK4", L"QK5", L"QK6", L"QK7",
        L"QK8", L"QK9", L"QK0", L"BESTPOTION", L"BESTMANA", L"BESTPETPOTION",
        L"BESTPETMANA",
    };
    for (int i = 0; i < static_cast<int>(sizeof(order) / sizeof(order[0])); ++i) {
        if (_wcsicmp(name.c_str(), order[i]) == 0) return i;
    }
    return 1000;
}

std::vector<GameAction> LoadGameActions(const std::wstring& keyBindingsPath) {
    std::wstring text;
    std::vector<GameAction> actions;
    if (!ReadUtf16File(keyBindingsPath, &text)) return actions;

    size_t search = 0;
    while (true) {
        const size_t start = text.find(L"[KEYBINDING]", search);
        if (start == std::wstring::npos) break;
        const size_t end = text.find(L"[/KEYBINDINGS]", start);
        if (end == std::wstring::npos) break;
        const std::wstring block = text.substr(start, end - start);
        search = end + 1;

        std::wstring name;
        std::wstring key;
        std::wstring modifier;
        if (!ExtractField(block, L"<STRING>NAME:", &name) ||
            !ExtractField(block, L"<INTEGER>KEY:", &key) ||
            !ExtractField(block, L"<INTEGER>MODIFIERKEY:", &modifier)) {
            continue;
        }

        GameAction action;
        action.name = name;
        action.label = DisplayLabelForAction(name);
        action.outputValue = L"Game:" + name;
        action.key = _wtoi(key.c_str());
        action.modifier = _wtoi(modifier.c_str());
        if (IsKeyboardBindable(action)) actions.push_back(action);
    }

    std::sort(actions.begin(), actions.end(), [](const GameAction& a, const GameAction& b) {
        const int orderA = InGameOrder(a.name);
        const int orderB = InGameOrder(b.name);
        if (orderA != orderB) return orderA < orderB;
        return a.name < b.name;
    });
    return actions;
}

void AddBuiltInAction(
    std::vector<GameAction>* actions,
    const wchar_t* label,
    const wchar_t* outputValue,
    const wchar_t* defaultControl,
    bool movementOnly = false) {
    GameAction action;
    action.label = label;
    action.name = label;
    action.outputValue = outputValue;
    action.assignedControl = defaultControl;
    action.movementOnly = movementOnly;
    actions->push_back(action);
}

void AddBuiltInActions(std::vector<GameAction>* actions) {
    std::vector<GameAction> builtIns;
    AddBuiltInAction(&builtIns, L"Move Up", L"Move:Up", L"LeftStickUp", true);
    AddBuiltInAction(&builtIns, L"Move Right", L"Move:Right", L"LeftStickRight", true);
    AddBuiltInAction(&builtIns, L"Move Down", L"Move:Down", L"LeftStickDown", true);
    AddBuiltInAction(&builtIns, L"Move Left", L"Move:Left", L"LeftStickLeft", true);
    AddBuiltInAction(&builtIns, L"Toggle Right Stick Mouse Mode", L"Addon:ToggleRightStickMouseMode", L"RightThumb");
    builtIns.insert(builtIns.end(), actions->begin(), actions->end());
    *actions = builtIns;
}

std::wstring ReadButtonValue(const wchar_t* key) {
    wchar_t value[256]{};
    GetPrivateProfileStringW(L"Buttons", key, L"", value, 256, g_iniPath.c_str());
    return value;
}

bool StartsWithGamePrefix(const std::wstring& value) {
    return value.size() > 5 && _wcsnicmp(value.c_str(), L"Game:", 5) == 0;
}

bool StartsWithMovePrefix(const std::wstring& value) {
    return value.size() > 5 && _wcsnicmp(value.c_str(), L"Move:", 5) == 0;
}

bool IsManagedActionValue(const std::wstring& value) {
    return StartsWithGamePrefix(value) ||
        StartsWithMovePrefix(value) ||
        _wcsicmp(value.c_str(), L"Addon:ToggleRightStickMouseMode") == 0 ||
        _wcsicmp(value.c_str(), L"MouseLeft") == 0 ||
        _wcsicmp(value.c_str(), L"MouseRight") == 0 ||
        _wcsicmp(value.c_str(), L"MouseMiddle") == 0;
}

bool IsVirtualStickControl(const std::wstring& controlName) {
    return _wcsicmp(controlName.c_str(), L"LeftStickUp") == 0 ||
        _wcsicmp(controlName.c_str(), L"LeftStickRight") == 0 ||
        _wcsicmp(controlName.c_str(), L"LeftStickDown") == 0 ||
        _wcsicmp(controlName.c_str(), L"LeftStickLeft") == 0;
}

std::wstring DisplayControlName(const std::wstring& iniName) {
    if (iniName.empty()) return L"";
    if (_wcsicmp(iniName.c_str(), L"None") == 0) return L"(unassigned)";
    for (const ControlRow& control : kControls) {
        if (_wcsicmp(control.iniName, iniName.c_str()) == 0) return control.label;
    }
    return iniName;
}

void RememberIniAssignment(const std::wstring& controlName, const std::wstring& value) {
    g_originalButtonValues[controlName] = value;
    if (!IsManagedActionValue(value)) return;

    for (GameAction& action : g_actions) {
        if (_wcsicmp(action.outputValue.c_str(), value.c_str()) == 0) {
            action.assignedControl = controlName;
            return;
        }
    }
}

void LoadExistingAssignments() {
    for (GameAction& action : g_actions) {
        if (!action.movementOnly) action.assignedControl.clear();
    }

    g_originalButtonValues.clear();
    for (const ControlRow& control : kControls) {
        const std::wstring value = ReadButtonValue(control.iniName);
        if (!IsVirtualStickControl(control.iniName)) {
            RememberIniAssignment(control.iniName, value);
        }
    }

    wchar_t section[32767]{};
    const DWORD chars = GetPrivateProfileSectionW(L"Buttons", section, 32767, g_iniPath.c_str());
    if (chars == 0 || chars >= 32766) return;
    for (wchar_t* entry = section; *entry != L'\0'; entry += lstrlenW(entry) + 1) {
        wchar_t* equals = wcschr(entry, L'=');
        if (equals == nullptr) continue;
        *equals = L'\0';
        const std::wstring key = entry;
        const std::wstring value = equals + 1;
        if (key.find(L'+') != std::wstring::npos) {
            RememberIniAssignment(key, value);
        }
    }
}

std::wstring StatusText() {
    if (g_keyBindingsPath.empty()) {
        return L"KeyBindings.dat not found. Place this tool next to the game or set KeyBindingsDat in the INI.";
    }
    if (g_exclusiveAttempted && !g_exclusiveAcquired) {
        return L"Capture is available, but DirectInput exclusive lock failed. Close desktop controller tools if they interfere.";
    }
    if (g_captureMode) {
        return g_comboCaptureMode
            ? L"Combo capture: hold LB/RB/LT/RT, then press the final button to confirm."
            : L"Simple capture: press one gamepad button. The tool will move to the next action.";
    }
    return L"Select an action, then use Capture Simple or Capture Combo. Double-click starts simple capture.";
}

void SetStatus(const std::wstring& text) {
    SetWindowTextW(g_status, text.c_str());
}

bool SuppressionActive() {
    const LONG until = InterlockedCompareExchange(&g_desktopSuppressUntilTick, 0, 0);
    return until != 0 && static_cast<LONG>(GetTickCount() - static_cast<DWORD>(until)) < 0;
}

bool IsNavigationKey(DWORD vk) {
    return
        (vk >= VK_GAMEPAD_A && vk <= VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT) ||
        vk == VK_UP ||
        vk == VK_DOWN ||
        vk == VK_LEFT ||
        vk == VK_RIGHT ||
        vk == VK_TAB ||
        vk == VK_RETURN ||
        vk == VK_SPACE ||
        vk == VK_ESCAPE;
}

LRESULT CALLBACK KeyboardBlockProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && SuppressionActive()) {
        const KBDLLHOOKSTRUCT* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (event != nullptr && IsNavigationKey(event->vkCode)) {
            return 1;
        }
    }
    return CallNextHookEx(g_keyboardBlockHook, code, wParam, lParam);
}

LRESULT CALLBACK MouseBlockProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && SuppressionActive()) {
        switch (wParam) {
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return 1;
        default:
            break;
        }
    }
    return CallNextHookEx(g_mouseBlockHook, code, wParam, lParam);
}

void InstallDesktopControllerBlockers() {
    if (g_keyboardBlockHook == nullptr) {
        g_keyboardBlockHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardBlockProc, g_instance, 0);
    }
    if (g_mouseBlockHook == nullptr) {
        g_mouseBlockHook = SetWindowsHookExW(WH_MOUSE_LL, MouseBlockProc, g_instance, 0);
    }
}

void UninstallDesktopControllerBlockers() {
    InterlockedExchange(&g_desktopSuppressUntilTick, 0);
    if (g_keyboardBlockHook != nullptr) {
        UnhookWindowsHookEx(g_keyboardBlockHook);
        g_keyboardBlockHook = nullptr;
    }
    if (g_mouseBlockHook != nullptr) {
        UnhookWindowsHookEx(g_mouseBlockHook);
        g_mouseBlockHook = nullptr;
    }
}

int SelectedIndex() {
    return ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
}

void SelectIndex(int index) {
    if (index < 0 || index >= static_cast<int>(g_actions.size())) return;
    ListView_SetItemState(g_list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(g_list, index, FALSE);
}

void RefreshRow(int index) {
    if (index < 0 || index >= static_cast<int>(g_actions.size())) return;
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.iSubItem = 1;
    const std::wstring label = g_actions[index].assignedControl.empty()
        ? L"(unassigned)"
        : DisplayControlName(g_actions[index].assignedControl);
    item.pszText = const_cast<wchar_t*>(label.c_str());
    ListView_SetItem(g_list, &item);
}

void PopulateList() {
    ListView_DeleteAllItems(g_list);
    for (int i = 0; i < static_cast<int>(g_actions.size()); ++i) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = const_cast<wchar_t*>(g_actions[i].label.c_str());
        ListView_InsertItem(g_list, &item);

        RefreshRow(i);

        const std::wstring keyText = g_actions[i].key > 0
            ? std::to_wstring(g_actions[i].key)
            : g_actions[i].outputValue;
        ListView_SetItemText(g_list, i, 2, const_cast<wchar_t*>(keyText.c_str()));
    }
    if (!g_actions.empty()) SelectIndex(0);
}

void StartCapture(bool comboMode) {
    if (g_actions.empty()) {
        SetStatus(L"No bindable action was detected.");
        return;
    }
    if (SelectedIndex() < 0) SelectIndex(0);
    g_captureMode = true;
    g_comboCaptureMode = comboMode;
    SetWindowTextW(g_captureButton, comboMode ? L"Capture Simple" : L"Stop");
    SetWindowTextW(g_captureComboButton, comboMode ? L"Stop" : L"Capture Combo");
    SetStatus(StatusText());
}

void StopCapture() {
    g_captureMode = false;
    g_comboCaptureMode = false;
    SetWindowTextW(g_captureButton, L"Capture Simple");
    SetWindowTextW(g_captureComboButton, L"Capture Combo");
    SetStatus(StatusText());
}

void MoveToNextAction() {
    const int selected = SelectedIndex();
    const int next = selected + 1;
    if (next >= 0 && next < static_cast<int>(g_actions.size())) {
        SelectIndex(next);
    } else {
        StopCapture();
        SetStatus(L"End of the list. Save to write the INI.");
    }
}

void ClearSelectedAction() {
    const int selected = SelectedIndex();
    if (selected < 0) return;
    g_actions[selected].assignedControl.clear();
    RefreshRow(selected);
}

void AssignSelectedAction(const std::wstring& controlName) {
    const int selected = SelectedIndex();
    if (selected < 0 || controlName.empty()) return;
    if (IsVirtualStickControl(controlName) && !StartsWithMovePrefix(g_actions[selected].outputValue)) {
        SetStatus(L"Left stick directions are reserved for movement actions.");
        return;
    }

    for (int i = 0; i < static_cast<int>(g_actions.size()); ++i) {
        if (_wcsicmp(g_actions[i].assignedControl.c_str(), controlName.c_str()) == 0) {
            g_actions[i].assignedControl.clear();
            RefreshRow(i);
        }
    }

    g_actions[selected].assignedControl = controlName;
    RefreshRow(selected);
    MoveToNextAction();
}

void AssignDefault(const wchar_t* outputValue, const wchar_t* controlName) {
    for (GameAction& action : g_actions) {
        if (_wcsicmp(action.outputValue.c_str(), outputValue) == 0) {
            action.assignedControl = controlName;
            return;
        }
    }
}

void ResetDefaultProfile() {
    for (GameAction& action : g_actions) {
        action.assignedControl.clear();
    }

    AssignDefault(L"Move:Up", L"LeftStickUp");
    AssignDefault(L"Move:Right", L"LeftStickRight");
    AssignDefault(L"Move:Down", L"LeftStickDown");
    AssignDefault(L"Move:Left", L"LeftStickLeft");
    AssignDefault(L"Addon:ToggleRightStickMouseMode", L"RightThumb");
    AssignDefault(L"Game:MOVE/ATTACK", L"A");
    AssignDefault(L"Game:CASTSKILL", L"B");
    AssignDefault(L"Game:AUTOMAP", L"Back");
    AssignDefault(L"Game:OPTIONSMENU", L"Start");
    AssignDefault(L"Game:HOLDPOSITION", L"LeftThumb");
    AssignDefault(L"Game:WEAPONSWAP", L"LeftShoulder");
    AssignDefault(L"Game:QK1", L"DPadLeft");
    AssignDefault(L"Game:QK2", L"DPadRight");
    AssignDefault(L"Game:QK3", L"DPadUp");
    AssignDefault(L"Game:QK4", L"DPadDown");
    AssignDefault(L"Game:QK5", L"X");
    AssignDefault(L"Game:QK6", L"Y");
    AssignDefault(L"Game:QK8", L"RightTrigger");
    AssignDefault(L"Game:QK9", L"LeftTrigger+A");
    AssignDefault(L"Game:QK0", L"LeftTrigger+B");
    AssignDefault(L"Game:SKILLMENU", L"RightShoulder+DPadUp");
    AssignDefault(L"Game:INVENTORYMENU", L"RightShoulder+DPadRight");
    AssignDefault(L"Game:QUESTMENU", L"RightShoulder+DPadDown");
    AssignDefault(L"Game:PETMENU", L"RightShoulder+DPadLeft");
    AssignDefault(L"Game:CLOSEALL", L"RightShoulder+B");
    AssignDefault(L"Game:SHOWITEMS", L"RightShoulder+A");

    PopulateList();
    SetStatus(L"Default profile restored in the window. Save to write the INI.");
}

void SaveAssignments() {
    std::set<std::wstring> managedControls;
    for (const ControlRow& control : kControls) {
        if (!IsVirtualStickControl(control.iniName)) managedControls.insert(control.iniName);
    }
    for (const wchar_t* chord : kDefaultManagedChordControls) {
        managedControls.insert(chord);
    }
    for (const auto& original : g_originalButtonValues) {
        if (original.first.find(L'+') != std::wstring::npos) managedControls.insert(original.first);
    }
    for (const GameAction& action : g_actions) {
        if (!action.assignedControl.empty() && !IsVirtualStickControl(action.assignedControl)) {
            managedControls.insert(action.assignedControl);
        }
    }

    for (const std::wstring& control : managedControls) {
        std::wstring value = L"None";
        for (const GameAction& action : g_actions) {
            if (!action.assignedControl.empty() &&
                !IsVirtualStickControl(action.assignedControl) &&
                _wcsicmp(action.assignedControl.c_str(), control.c_str()) == 0) {
                value = action.outputValue;
                break;
            }
        }

        WritePrivateProfileStringW(L"Buttons", control.c_str(), value.c_str(), g_iniPath.c_str());
    }

    WritePrivateProfileStringW(L"Movement", L"LeftStickEnabled", L"1", g_iniPath.c_str());
    WritePrivateProfileStringW(L"Movement", L"FollowKeyboardProfile", L"1", g_iniPath.c_str());

    if (!g_keyBindingsPath.empty()) {
        WritePrivateProfileStringW(L"General", L"KeyBindingsDat", L"AUTO", g_iniPath.c_str());
    }

    LoadExistingAssignments();
    PopulateList();
    SetStatus(L"INI saved. The d3d9 runtime reloads the configuration automatically in game.");
}

bool LoadXInput() {
    const wchar_t* candidates[] = {
        L"xinput1_4.dll",
        L"xinput1_3.dll",
        L"xinput9_1_0.dll",
    };
    for (const wchar_t* candidate : candidates) {
        g_xinput = LoadLibraryW(candidate);
        if (g_xinput == nullptr) continue;
        g_xInputGetState = reinterpret_cast<XInputGetStateFn>(
            GetProcAddress(g_xinput, "XInputGetState"));
        if (g_xInputGetState != nullptr) return true;
        FreeLibrary(g_xinput);
        g_xinput = nullptr;
    }
    return false;
}

BOOL CALLBACK DirectInputEnumCallback(const DIDEVICEINSTANCEW* instance, void*) {
    if (g_directInput == nullptr || g_exclusiveDevice != nullptr) return DIENUM_STOP;

    IDirectInputDevice8W* device = nullptr;
    if (FAILED(g_directInput->CreateDevice(instance->guidInstance, &device, nullptr))) {
        return DIENUM_CONTINUE;
    }

    if (FAILED(device->SetDataFormat(&c_dfDIJoystick2))) {
        device->Release();
        return DIENUM_CONTINUE;
    }

    if (FAILED(device->SetCooperativeLevel(g_mainWindow, DISCL_FOREGROUND | DISCL_EXCLUSIVE))) {
        device->Release();
        return DIENUM_CONTINUE;
    }

    g_exclusiveDevice = device;
    return DIENUM_STOP;
}

void AcquireExclusiveController() {
    g_exclusiveAttempted = true;
    if (g_mainWindow == nullptr) return;

    if (g_directInput == nullptr) {
        if (FAILED(DirectInput8Create(
                g_instance,
                DIRECTINPUT_VERSION,
                IID_IDirectInput8W,
                reinterpret_cast<void**>(&g_directInput),
                nullptr))) {
            g_exclusiveAcquired = false;
            return;
        }
    }

    if (g_exclusiveDevice == nullptr) {
        g_directInput->EnumDevices(
            DI8DEVCLASS_GAMECTRL,
            DirectInputEnumCallback,
            nullptr,
            DIEDFL_ATTACHEDONLY);
    }

    if (g_exclusiveDevice == nullptr) {
        g_exclusiveAcquired = false;
        return;
    }

    const HRESULT result = g_exclusiveDevice->Acquire();
    g_exclusiveAcquired =
        result == DI_OK ||
        result == S_FALSE ||
        result == DI_NOEFFECT;
}

void ReleaseExclusiveController() {
    g_exclusiveAcquired = false;
    if (g_exclusiveDevice != nullptr) {
        g_exclusiveDevice->Unacquire();
        g_exclusiveDevice->Release();
        g_exclusiveDevice = nullptr;
    }
    if (g_directInput != nullptr) {
        g_directInput->Release();
        g_directInput = nullptr;
    }
}

std::wstring FirstPressedControl(const XINPUT_STATE& state) {
    const DWORD current = state.Gamepad.wButtons;
    const DWORD pressed = current & ~g_previousButtons;

    struct ButtonName {
        WORD bit;
        const wchar_t* name;
    };
    const ButtonName buttons[] = {
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

    for (const ButtonName& button : buttons) {
        if ((pressed & button.bit) != 0) return button.name;
    }

    const bool leftTrigger = state.Gamepad.bLeftTrigger >= kTriggerThreshold;
    const bool rightTrigger = state.Gamepad.bRightTrigger >= kTriggerThreshold;
    if (leftTrigger && !g_previousLeftTrigger) return L"LeftTrigger";
    if (rightTrigger && !g_previousRightTrigger) return L"RightTrigger";
    return L"";
}

bool IsModifierControlName(const std::wstring& name) {
    return _wcsicmp(name.c_str(), L"LeftShoulder") == 0 ||
        _wcsicmp(name.c_str(), L"RightShoulder") == 0 ||
        _wcsicmp(name.c_str(), L"LeftTrigger") == 0 ||
        _wcsicmp(name.c_str(), L"RightTrigger") == 0;
}

std::wstring ActiveModifierPrefix(const XINPUT_GAMEPAD& gamepad) {
    std::wstring prefix;
    if ((gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0) prefix += L"LeftShoulder+";
    if ((gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0) prefix += L"RightShoulder+";
    if (gamepad.bLeftTrigger >= kTriggerThreshold) prefix += L"LeftTrigger+";
    if (gamepad.bRightTrigger >= kTriggerThreshold) prefix += L"RightTrigger+";
    return prefix;
}

std::wstring FirstPressedStickDirection(const XINPUT_STATE& state) {
    const SHORT threshold = 18000;
    if (state.Gamepad.sThumbLY >= threshold) return L"LeftStickUp";
    if (state.Gamepad.sThumbLY <= -threshold) return L"LeftStickDown";
    if (state.Gamepad.sThumbLX >= threshold) return L"LeftStickRight";
    if (state.Gamepad.sThumbLX <= -threshold) return L"LeftStickLeft";
    return L"";
}

std::wstring CaptureControlName(const XINPUT_STATE& state) {
    const std::wstring pressed = FirstPressedControl(state);
    if (!pressed.empty()) {
        if (!g_comboCaptureMode) return pressed;
        if (IsModifierControlName(pressed)) return L"";
        const std::wstring modifiers = ActiveModifierPrefix(state.Gamepad);
        if (modifiers.empty()) return L"";
        return modifiers + pressed;
    }

    if (!g_comboCaptureMode) {
        return FirstPressedStickDirection(state);
    }
    return L"";
}

bool GamepadHasDesktopActivity(const XINPUT_GAMEPAD& gamepad) {
    return gamepad.wButtons != 0 ||
        gamepad.bLeftTrigger >= kTriggerThreshold ||
        gamepad.bRightTrigger >= kTriggerThreshold ||
        std::abs(static_cast<int>(gamepad.sThumbLX)) >= kDesktopStickThreshold ||
        std::abs(static_cast<int>(gamepad.sThumbLY)) >= kDesktopStickThreshold ||
        std::abs(static_cast<int>(gamepad.sThumbRX)) >= kDesktopStickThreshold ||
        std::abs(static_cast<int>(gamepad.sThumbRY)) >= kDesktopStickThreshold;
}

void PollXInput() {
    if (g_xInputGetState == nullptr) return;

    XINPUT_STATE state{};
    if (g_xInputGetState(0, &state) != ERROR_SUCCESS) {
        g_previousButtons = 0;
        g_previousLeftTrigger = false;
        g_previousRightTrigger = false;
        if (g_captureMode) SetStatus(L"Aucune manette XInput detectee.");
        return;
    }

    if (GamepadHasDesktopActivity(state.Gamepad)) {
        InterlockedExchange(
            &g_desktopSuppressUntilTick,
            static_cast<LONG>(GetTickCount() + kDesktopSuppressMs));
    }

    const std::wstring pressed = CaptureControlName(state);
    g_previousButtons = state.Gamepad.wButtons;
    g_previousLeftTrigger = state.Gamepad.bLeftTrigger >= kTriggerThreshold;
    g_previousRightTrigger = state.Gamepad.bRightTrigger >= kTriggerThreshold;

    if (g_captureMode && !pressed.empty()) {
        AssignSelectedAction(pressed);
    }
}

void ResizeControls(HWND hwnd) {
    RECT rect{};
    GetClientRect(hwnd, &rect);
    const int margin = 12;
    const int buttonHeight = 30;
    const int statusHeight = 42;
    const int bottomTop = rect.bottom - margin - buttonHeight;
    const int listTop = margin + statusHeight + 8;
    const int listBottom = bottomTop - 10;
    const int width = rect.right - rect.left;

    MoveWindow(g_status, margin, margin, width - margin * 2, statusHeight, TRUE);
    MoveWindow(g_list, margin, listTop, width - margin * 2, listBottom - listTop, TRUE);

    const int buttonWidth = 118;
    int x = margin;
    MoveWindow(g_captureButton, x, bottomTop, buttonWidth, buttonHeight, TRUE);
    x += buttonWidth + 8;
    MoveWindow(g_captureComboButton, x, bottomTop, 128, buttonHeight, TRUE);
    x += 136;
    MoveWindow(g_skipButton, x, bottomTop, 90, buttonHeight, TRUE);
    x += 98;
    MoveWindow(g_clearButton, x, bottomTop, 90, buttonHeight, TRUE);
    x += 98;
    MoveWindow(g_defaultsButton, x, bottomTop, 118, buttonHeight, TRUE);
    MoveWindow(g_saveButton, width - margin - 220, bottomTop, 104, buttonHeight, TRUE);
    MoveWindow(g_closeButton, width - margin - 108, bottomTop, 108, buttonHeight, TRUE);

    const int listWidth = width - margin * 2;
    ListView_SetColumnWidth(g_list, 0, (listWidth * 48) / 100);
    ListView_SetColumnWidth(g_list, 1, (listWidth * 32) / 100);
    ListView_SetColumnWidth(g_list, 2, LVSCW_AUTOSIZE_USEHEADER);
}

void CreateColumns() {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<wchar_t*>(L"In-game action");
    column.cx = 360;
    column.iSubItem = 0;
    ListView_InsertColumn(g_list, 0, &column);

    column.pszText = const_cast<wchar_t*>(L"Gamepad bind");
    column.cx = 190;
    column.iSubItem = 1;
    ListView_InsertColumn(g_list, 1, &column);

    column.pszText = const_cast<wchar_t*>(L"Output");
    column.cx = 90;
    column.iSubItem = 2;
    ListView_InsertColumn(g_list, 2, &column);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_mainWindow = hwnd;
        g_status = CreateWindowExW(
            0, L"STATIC", StatusText().c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_STATUS), g_instance, nullptr);

        g_list = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_ACTION_LIST), g_instance, nullptr);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        CreateColumns();
        PopulateList();

        g_captureButton = CreateWindowExW(0, L"BUTTON", L"Capture Simple", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CAPTURE), g_instance, nullptr);
        g_captureComboButton = CreateWindowExW(0, L"BUTTON", L"Capture Combo", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CAPTURE_COMBO), g_instance, nullptr);
        g_skipButton = CreateWindowExW(0, L"BUTTON", L"Skip", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_SKIP), g_instance, nullptr);
        g_clearButton = CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CLEAR), g_instance, nullptr);
        g_defaultsButton = CreateWindowExW(0, L"BUTTON", L"Defaults", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_DEFAULTS), g_instance, nullptr);
        g_saveButton = CreateWindowExW(0, L"BUTTON", L"Save INI", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_SAVE), g_instance, nullptr);
        g_closeButton = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CLOSE), g_instance, nullptr);

        InstallDesktopControllerBlockers();
        SetTimer(hwnd, kPollTimerId, 30, nullptr);
        AcquireExclusiveController();
        SetStatus(StatusText());
        ResizeControls(hwnd);
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) {
            AcquireExclusiveController();
            SetStatus(StatusText());
        }
        return 0;

    case WM_SIZE:
        ResizeControls(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == kPollTimerId) {
            if (!g_exclusiveAcquired) {
                AcquireExclusiveController();
            }
            PollXInput();
            return 0;
        }
        break;

    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR*>(lParam)->idFrom == IDC_ACTION_LIST &&
            reinterpret_cast<NMHDR*>(lParam)->code == NM_DBLCLK) {
            StartCapture(false);
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CAPTURE:
            if (g_captureMode && !g_comboCaptureMode) StopCapture(); else StartCapture(false);
            return 0;
        case IDC_CAPTURE_COMBO:
            if (g_captureMode && g_comboCaptureMode) StopCapture(); else StartCapture(true);
            return 0;
        case IDC_SKIP:
            MoveToNextAction();
            return 0;
        case IDC_CLEAR:
            ClearSelectedAction();
            return 0;
        case IDC_DEFAULTS:
            ResetDefaultProfile();
            return 0;
        case IDC_SAVE:
            SaveAssignments();
            return 0;
        case IDC_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, kPollTimerId);
        UninstallDesktopControllerBlockers();
        ReleaseExclusiveController();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RegisterMainWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    return RegisterClassExW(&wc) != 0;
}

void InitializeAppData() {
    g_appDir = AppDirectory();
    g_iniPath = JoinPath(g_appDir, kIniName);
    g_keyBindingsPath = DetectKeyBindingsPath();
    g_actions = g_keyBindingsPath.empty()
        ? std::vector<GameAction>{}
        : LoadGameActions(g_keyBindingsPath);
    AddBuiltInActions(&g_actions);
    LoadExistingAssignments();
}

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand) {
    g_instance = instance;
    g_configMutex = CreateMutexW(nullptr, TRUE, kConfigMutexName);
    InitializeAppData();
    LoadXInput();

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    if (!RegisterMainWindowClass()) return 1;

    g_mainWindow = CreateWindowExW(
        0,
        kWindowClass,
        L"TL2TrueControlerSupportConfig v0.0.9",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        860,
        620,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (g_mainWindow == nullptr) return 1;

    ShowWindow(g_mainWindow, showCommand);
    UpdateWindow(g_mainWindow);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_xinput != nullptr) {
        FreeLibrary(g_xinput);
        g_xinput = nullptr;
    }
    if (g_configMutex != nullptr) {
        ReleaseMutex(g_configMutex);
        CloseHandle(g_configMutex);
        g_configMutex = nullptr;
    }

    return static_cast<int>(message.wParam);
}
