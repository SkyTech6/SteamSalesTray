#include "platform/StartupManager.h"

#include <string>
#include <windows.h>

#include "platform/Paths.h"

#pragma comment(lib, "advapi32.lib")

namespace platform::StartupManager {

namespace {
constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"SteamSalesTray";

// The command written to the Run key: the quoted exe path plus --background.
std::wstring BuildCommand() {
    const std::wstring exe = GetExecutablePath();
    if (exe.empty()) {
        return {};
    }
    return L"\"" + exe + L"\" --background";
}
}  // namespace

bool IsEnabled() {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = ::RegQueryValueExW(key, kValueName, nullptr, &type, nullptr,
                                        &bytes);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) ||
        bytes == 0) {
        ::RegCloseKey(key);
        return false;
    }

    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    status = ::RegQueryValueExW(key, kValueName, nullptr, nullptr,
                                reinterpret_cast<BYTE*>(value.data()), &bytes);
    ::RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    // Trim any trailing NULs the API may include.
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }

    // Consider startup enabled only if it points at this executable.
    const std::wstring exe = GetExecutablePath();
    return !exe.empty() &&
           value.find(exe) != std::wstring::npos;
}

bool Enable() {
    const std::wstring command = BuildCommand();
    if (command.empty()) {
        return false;
    }
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr,
                          REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                          nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const DWORD bytes =
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    const LSTATUS status = ::RegSetValueExW(
        key, kValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), bytes);
    ::RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

bool Disable() {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) !=
        ERROR_SUCCESS) {
        return true;  // key absent => already disabled
    }
    const LSTATUS status = ::RegDeleteValueW(key, kValueName);
    ::RegCloseKey(key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

}  // namespace platform::StartupManager
