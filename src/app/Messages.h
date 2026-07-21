#pragma once

#include <windows.h>

namespace app {

// Window class of the invisible top-level main window. It must be a normal
// (not message-only) top-level window so it can receive the broadcast
// "TaskbarCreated" message and be located by other instances via FindWindow.
inline constexpr wchar_t kWindowClass[] = L"SteamSalesTrayMainWindow";
inline constexpr wchar_t kWindowTitle[] = L"SteamSalesTray";

// Custom message the tray icon posts to the main window (WM_APP-relative).
inline constexpr UINT WM_APP_TRAY = WM_APP + 1;

// Worker -> UI-thread notifications. Introduced now, wired up in Phase 5.
inline constexpr UINT WM_APP_SYNC_STARTED   = WM_APP + 10;
inline constexpr UINT WM_APP_SYNC_PROGRESS  = WM_APP + 11;
inline constexpr UINT WM_APP_SYNC_COMPLETED = WM_APP + 12;
inline constexpr UINT WM_APP_SYNC_FAILED    = WM_APP + 13;
inline constexpr UINT WM_APP_TOTALS_CHANGED = WM_APP + 14;

// Names for RegisterWindowMessage(). A registered message maps a string to a
// system-wide unique UINT, so separate instances agree on the value.
inline constexpr wchar_t kRegMsgShowSales[]    = L"SteamSalesTray.ShowSales.v1";
inline constexpr wchar_t kRegMsgShowSettings[] = L"SteamSalesTray.ShowSettings.v1";
inline constexpr wchar_t kRegMsgRefresh[]      = L"SteamSalesTray.Refresh.v1";

// Broadcast by the shell when Explorer (re)starts; we must re-add the tray icon.
inline constexpr wchar_t kRegMsgTaskbarCreated[] = L"TaskbarCreated";

// Steamworks partner financials page (opened from the tray menu).
inline constexpr wchar_t kSteamworksFinancialsUrl[] =
    L"https://partner.steamgames.com/financials/";

}  // namespace app
