#include "tray/TrayMenu.h"

#include "resources/Resource.h"

namespace tray {

namespace {
// Adds a disabled, display-only row. A tab right-aligns the count column.
void AppendSummaryRow(HMENU menu, UINT id, const wchar_t* label,
                      const std::wstring& value) {
    std::wstring text = std::wstring(label) + L"\t" + value;
    ::AppendMenuW(menu, MF_STRING, id, text.c_str());
    ::EnableMenuItem(menu, id, MF_BYCOMMAND | MF_GRAYED);
}
}  // namespace

UINT ShowTrayMenu(HWND owner, POINT pt, const SummaryTotals& totals) {
    HMENU menu = ::CreatePopupMenu();
    if (!menu) {
        return 0;
    }

    // Header (disabled, bold-ish by convention of being the default item).
    ::AppendMenuW(menu, MF_STRING, 0, L"Steam Sales");
    ::EnableMenuItem(menu, 0, MF_BYPOSITION | MF_GRAYED);
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendSummaryRow(menu, IDM_SUMMARY_TODAY,    L"Today",        totals.today);
    AppendSummaryRow(menu, IDM_SUMMARY_7DAY,     L"Last 7 Days",  totals.week);
    AppendSummaryRow(menu, IDM_SUMMARY_30DAY,    L"Last 30 Days", totals.month);
    if (totals.showLifetime) {
        AppendSummaryRow(menu, IDM_SUMMARY_LIFETIME, L"Lifetime",
                         totals.lifetime);
    }

    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, IDM_VIEW_SALES, L"View Product Sales…");
    ::AppendMenuW(menu, MF_STRING, IDM_REFRESH_NOW, L"Refresh Now");
    ::AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings…");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, IDM_OPEN_FINANCIALS,
                  L"Open Steamworks Financials");
    ::AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"About");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

    // Double-click default action = View Product Sales.
    ::SetMenuDefaultItem(menu, IDM_VIEW_SALES, FALSE);

    // Required so the menu dismisses when the user clicks elsewhere.
    ::SetForegroundWindow(owner);

    const UINT cmd = static_cast<UINT>(::TrackPopupMenuEx(
        menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, owner, nullptr));

    ::DestroyMenu(menu);
    return cmd;
}

}  // namespace tray
