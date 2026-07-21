#include <string>
#include <string_view>
#include <windows.h>
#include <shellapi.h>

#include "app/App.h"
#include "app/Messages.h"
#include "app/SingleInstance.h"

namespace {

enum class LaunchAction { Normal, Settings, Refresh };

struct LaunchOptions {
    LaunchAction action = LaunchAction::Normal;
    bool background = false;  // launched from the startup Run entry
};

LaunchOptions ParseCommandLine() {
    LaunchOptions opts;
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv) {
        return opts;
    }
    for (int i = 1; i < argc; ++i) {
        std::wstring_view arg = argv[i];
        if (arg == L"--settings") {
            opts.action = LaunchAction::Settings;
        } else if (arg == L"--refresh") {
            opts.action = LaunchAction::Refresh;
        } else if (arg == L"--background") {
            opts.background = true;
        }
    }
    ::LocalFree(argv);
    return opts;
}

// Forwards the requested action to an already-running instance and returns true
// on success. Locates the primary instance by its (invisible) main window.
bool SignalExistingInstance(LaunchAction action) {
    HWND existing = ::FindWindowW(app::kWindowClass, app::kWindowTitle);
    if (!existing) {
        return false;
    }
    UINT message = 0;
    switch (action) {
        case LaunchAction::Settings:
            message = ::RegisterWindowMessageW(app::kRegMsgShowSettings);
            break;
        case LaunchAction::Refresh:
            message = ::RegisterWindowMessageW(app::kRegMsgRefresh);
            break;
        case LaunchAction::Normal:
        default:
            message = ::RegisterWindowMessageW(app::kRegMsgShowSales);
            break;
    }
    if (message == 0) {
        return false;
    }
    ::PostMessageW(existing, message, 0, 0);
    ::SetForegroundWindow(existing);
    return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const LaunchOptions opts = ParseCommandLine();

    app::SingleInstance guard;
    if (!guard.IsPrimary()) {
        // Another instance owns the tray; hand off the requested action.
        SignalExistingInstance(opts.action);
        return 0;
    }

    app::App application;
    return application.Run(instance);
}
