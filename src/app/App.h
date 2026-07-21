#pragma once

#include <string>
#include <windows.h>

#include "storage/Database.h"
#include "sync/SalesSyncService.h"
#include "tray/TrayIcon.h"
#include "tray/TrayMenu.h"
#include "ui/SalesWindow.h"

namespace app {

// Owns the invisible main window, the tray icon, and the message loop.
// One instance lives for the lifetime of the process.
class App {
public:
    App() = default;
    ~App() = default;

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Creates the window + tray icon and runs the message loop until Exit.
    // Returns the process exit code.
    int Run(HINSTANCE instance);

private:
    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

    bool RegisterWindowClass();
    bool CreateMainWindow();

    void OnTrayCallback(WPARAM wParam, LPARAM lParam);
    void OnCommand(UINT id);
    void ShowContextMenu(POINT pt);
    void OnShowSales();
    void OnSettings();

    // Sets the tray tooltip to reflect whether an API key is configured.
    void UpdateTooltipForKeyState();

    // Refreshes the tray tooltip from current worker status + DB totals.
    void UpdateTray();

    // Builds the menu summary rows from the database (em dash if unavailable).
    tray::SummaryTotals ComputeSummary();

    // Shows the worker's pending notification balloon, if any.
    void MaybeShowNotification();

    // Registered cross-instance / shell messages resolved at startup.
    void RegisterAppMessages();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HICON icon_ = nullptr;
    tray::TrayIcon tray_;
    storage::Database db_;   // UI-thread read connection (worker has its own)
    bool dbOpen_ = false;
    sync::SalesSyncService sync_;
    ui::SalesWindow salesWindow_;

    UINT msgTaskbarCreated_ = 0;
    UINT msgShowSales_ = 0;
    UINT msgShowSettings_ = 0;
    UINT msgRefresh_ = 0;
};

}  // namespace app
