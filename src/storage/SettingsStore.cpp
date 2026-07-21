#include "storage/SettingsStore.h"

#include <cstdio>
#include <windows.h>

#pragma comment(lib, "advapi32.lib")

namespace storage::SettingsStore {

namespace {
constexpr wchar_t kKey[] = L"Software\\Bezi\\SteamSalesTray";
constexpr wchar_t kValRefresh[] = L"RefreshIntervalMinutes";
constexpr wchar_t kValNotify[] = L"NotifyOnNewUnits";
constexpr wchar_t kValHideLifetime[] = L"HideLifetime";

constexpr DWORD kDefaultRefresh = 30;

DWORD ReadDword(const wchar_t* name, DWORD fallback) {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return fallback;
    }
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS status = ::RegQueryValueExW(
        key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
    ::RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_DWORD) {
        return fallback;
    }
    return value;
}

void WriteDword(const wchar_t* name, DWORD value) {
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr,
                          REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                          nullptr) != ERROR_SUCCESS) {
        return;
    }
    ::RegSetValueExW(key, name, 0, REG_DWORD,
                     reinterpret_cast<const BYTE*>(&value), sizeof(value));
    ::RegCloseKey(key);
}

bool IsValidInterval(DWORD minutes) {
    switch (minutes) {
        case 0:    // Manual
        case 15:
        case 30:
        case 60:
        case 120:
        case 360:
            return true;
        default:
            return false;
    }
}
}  // namespace

RefreshInterval GetRefreshInterval() {
    DWORD minutes = ReadDword(kValRefresh, kDefaultRefresh);
    if (!IsValidInterval(minutes)) {
        minutes = kDefaultRefresh;
    }
    // Never allow a sub-15-minute automatic cadence.
    if (minutes != 0 && minutes < 15) {
        minutes = 15;
    }
    return static_cast<RefreshInterval>(static_cast<int>(minutes));
}

void SetRefreshInterval(RefreshInterval interval) {
    WriteDword(kValRefresh, static_cast<DWORD>(static_cast<int>(interval)));
}

bool GetNotifyOnNewUnits() {
    return ReadDword(kValNotify, 0) != 0;
}

void SetNotifyOnNewUnits(bool enabled) {
    WriteDword(kValNotify, enabled ? 1u : 0u);
}

bool GetHideLifetime() {
    return ReadDword(kValHideLifetime, 0) != 0;
}

void SetHideLifetime(bool hidden) {
    WriteDword(kValHideLifetime, hidden ? 1u : 0u);
}

namespace {
constexpr wchar_t kValPeriod[] = L"SalesPeriodIndex";
constexpr wchar_t kValRectX[] = L"SalesWindowX";
constexpr wchar_t kValRectY[] = L"SalesWindowY";
constexpr wchar_t kValRectW[] = L"SalesWindowW";
constexpr wchar_t kValRectH[] = L"SalesWindowH";
constexpr DWORD kUnsetSentinel = 0x7FFFFFFF;
}  // namespace

int GetSalesPeriodIndex() {
    return static_cast<int>(ReadDword(kValPeriod, 0));
}

void SetSalesPeriodIndex(int index) {
    WriteDword(kValPeriod, static_cast<DWORD>(index));
}

bool GetSalesWindowRect(int& x, int& y, int& width, int& height) {
    const DWORD w = ReadDword(kValRectW, kUnsetSentinel);
    const DWORD h = ReadDword(kValRectH, kUnsetSentinel);
    if (w == kUnsetSentinel || h == kUnsetSentinel || w == 0 || h == 0) {
        return false;
    }
    x = static_cast<int>(ReadDword(kValRectX, 0));
    y = static_cast<int>(ReadDword(kValRectY, 0));
    width = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

void SetSalesWindowRect(int x, int y, int width, int height) {
    WriteDword(kValRectX, static_cast<DWORD>(x));
    WriteDword(kValRectY, static_cast<DWORD>(y));
    WriteDword(kValRectW, static_cast<DWORD>(width));
    WriteDword(kValRectH, static_cast<DWORD>(height));
}

int GetSalesColumnWidth(int column, int fallback) {
    wchar_t name[32];
    swprintf_s(name, L"SalesColWidth%d", column);
    const DWORD v = ReadDword(name, kUnsetSentinel);
    return v == kUnsetSentinel ? fallback : static_cast<int>(v);
}

void SetSalesColumnWidth(int column, int width) {
    wchar_t name[32];
    swprintf_s(name, L"SalesColWidth%d", column);
    WriteDword(name, static_cast<DWORD>(width));
}

}  // namespace storage::SettingsStore
