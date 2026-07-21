#include "tray/TrayIcon.h"

#include <algorithm>

#pragma comment(lib, "shell32.lib")

namespace tray {

namespace {
// Stable identity for the tray icon. Windows binds this GUID to the current
// executable path; if the exe moves, NIM_ADD with the GUID fails and we fall
// back to a uID identity.
// {7C3D9A42-1E5B-4F8A-9C6D-2A0B1E7F4D31}
constexpr GUID kTrayGuid = {
    0x7c3d9a42, 0x1e5b, 0x4f8a,
    {0x9c, 0x6d, 0x2a, 0x0b, 0x1e, 0x7f, 0x4d, 0x31}};

constexpr UINT kTrayUid = 1;
}  // namespace

TrayIcon::~TrayIcon() {
    Remove();
}

NOTIFYICONDATAW TrayIcon::BuildData() const {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner_;
    if (useGuid_) {
        nid.uFlags |= NIF_GUID;
        nid.guidItem = kTrayGuid;
    } else {
        nid.uID = kTrayUid;
    }
    return nid;
}

bool TrayIcon::Add(HWND owner, UINT callbackMessage, HICON icon,
                   const std::wstring& tooltip) {
    owner_ = owner;
    callbackMessage_ = callbackMessage;
    icon_ = icon;
    tooltip_ = tooltip;

    auto tryAdd = [&]() -> bool {
        NOTIFYICONDATAW nid = BuildData();
        nid.uFlags |= NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        nid.uCallbackMessage = callbackMessage_;
        nid.hIcon = icon_;
        wcsncpy_s(nid.szTip, tooltip_.c_str(), _TRUNCATE);

        if (!::Shell_NotifyIconW(NIM_ADD, &nid)) {
            return false;
        }
        // Windows requires NIM_SETVERSION after every NIM_ADD.
        NOTIFYICONDATAW ver = BuildData();
        ver.uVersion = NOTIFYICON_VERSION_4;
        ::Shell_NotifyIconW(NIM_SETVERSION, &ver);
        return true;
    };

    // A stale GUID registration can linger from a prior run at a different path;
    // clear it before adding so the first attempt has a clean slate.
    if (useGuid_) {
        NOTIFYICONDATAW del = BuildData();
        ::Shell_NotifyIconW(NIM_DELETE, &del);
    }

    if (tryAdd()) {
        added_ = true;
        return true;
    }

    // Fall back to a uID-based identity (common in dev builds).
    if (useGuid_) {
        useGuid_ = false;
        if (tryAdd()) {
            added_ = true;
            return true;
        }
    }

    added_ = false;
    return false;
}

void TrayIcon::Recreate() {
    // After an Explorer restart the shell has forgotten our icon; the handle
    // state on our side may still say "added", so reset and re-add.
    added_ = false;
    Add(owner_, callbackMessage_, icon_, tooltip_);
}

void TrayIcon::SetTooltip(const std::wstring& tooltip) {
    tooltip_ = tooltip;
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW nid = BuildData();
    nid.uFlags |= NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(nid.szTip, tooltip_.c_str(), _TRUNCATE);
    ::Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::ShowBalloon(const std::wstring& title,
                           const std::wstring& text) {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW nid = BuildData();
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, text.c_str(), _TRUNCATE);
    ::Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::SetFocus() {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW nid = BuildData();
    ::Shell_NotifyIconW(NIM_SETFOCUS, &nid);
}

void TrayIcon::Remove() {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW nid = BuildData();
    ::Shell_NotifyIconW(NIM_DELETE, &nid);
    added_ = false;
}

}  // namespace tray
