#pragma once

#include <string>
#include <windows.h>
#include <shellapi.h>

namespace tray {

// RAII wrapper around a single notification-area icon.
//
// Uses NOTIFYICON_VERSION_4 semantics. Prefers a stable GUID identity, but
// falls back to a uID-based identity if GUID registration fails (which happens
// during development when the executable path changes, since Windows binds the
// GUID to the exe path).
class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Adds the icon and sets version 4. `callbackMessage` is the WM_APP-relative
    // message the shell posts to `owner` for mouse/keyboard events.
    bool Add(HWND owner, UINT callbackMessage, HICON icon,
             const std::wstring& tooltip);

    // Re-adds the icon after an Explorer restart ("TaskbarCreated").
    void Recreate();

    // Updates the hover tooltip (truncated to the szTip limit).
    void SetTooltip(const std::wstring& tooltip);

    // Shows a balloon/banner notification (NIF_INFO). Text is truncated to the
    // szInfo limits.
    void ShowBalloon(const std::wstring& title, const std::wstring& text);

    // Returns keyboard focus to the notification area (NIM_SETFOCUS), as
    // recommended after dismissing a context menu.
    void SetFocus();

    void Remove();

    bool IsAdded() const { return added_; }

private:
    NOTIFYICONDATAW BuildData() const;

    HWND owner_ = nullptr;
    UINT callbackMessage_ = 0;
    HICON icon_ = nullptr;
    std::wstring tooltip_;
    bool added_ = false;
    bool useGuid_ = true;
};

}  // namespace tray
