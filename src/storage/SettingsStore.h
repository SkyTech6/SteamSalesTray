#pragma once

namespace storage {

// Non-secret user preferences, persisted under
// HKCU\Software\Bezi\SteamSalesTray. The API key is NEVER stored here (see
// plan §5); it lives only in the DPAPI-protected key file.

// Refresh cadence. The stored value is the interval in minutes; 0 means manual.
enum class RefreshInterval : int {
    Manual = 0,
    Min15 = 15,
    Min30 = 30,
    Min60 = 60,
    Hours2 = 120,
    Hours6 = 360,
};

namespace SettingsStore {

// Refresh interval (default: 30 minutes). Values below 15 minutes (other than
// Manual) are clamped up to 15 on read.
RefreshInterval GetRefreshInterval();
void SetRefreshInterval(RefreshInterval interval);

// Notify when new units are detected (default: false).
bool GetNotifyOnNewUnits();
void SetNotifyOnNewUnits(bool enabled);

// Hide all lifetime totals (tooltip, menu row, Sales window period). Default
// false. Display-only; does not affect syncing or stored data.
bool GetHideLifetime();
void SetHideLifetime(bool hidden);

// --- Sales window UI state (persisted layout/selection) -------------------

// Selected period index in the Product Sales window (default 0 = Today).
int GetSalesPeriodIndex();
void SetSalesPeriodIndex(int index);

// Saved window rectangle. Returns false if none stored (use defaults).
bool GetSalesWindowRect(int& x, int& y, int& width, int& height);
void SetSalesWindowRect(int x, int y, int width, int height);

// ListView column widths (col 0..2). Returns `fallback` if unset.
int GetSalesColumnWidth(int column, int fallback);
void SetSalesColumnWidth(int column, int width);

}  // namespace SettingsStore
}  // namespace storage
