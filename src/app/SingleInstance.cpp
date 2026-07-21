#include "app/SingleInstance.h"

#include <memory>
#include <sddl.h>

#pragma comment(lib, "advapi32.lib")

namespace app {

std::wstring GetCurrentUserToken() {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return L"default";
    }

    // First call sizes the buffer, second fills it.
    DWORD size = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (size == 0) {
        ::CloseHandle(token);
        return L"default";
    }

    auto buffer = std::make_unique<BYTE[]>(size);
    std::wstring result = L"default";
    if (::GetTokenInformation(token, TokenUser, buffer.get(), size, &size)) {
        const auto* tokenUser = reinterpret_cast<const TOKEN_USER*>(buffer.get());
        LPWSTR sidString = nullptr;
        if (::ConvertSidToStringSidW(tokenUser->User.Sid, &sidString) && sidString) {
            result.assign(sidString);
            ::LocalFree(sidString);
        }
    }
    ::CloseHandle(token);
    return result;
}

SingleInstance::SingleInstance() {
    userToken_ = GetCurrentUserToken();

    // Local\ namespace scopes the mutex to the current session/user, requiring
    // no elevated cross-session permissions.
    const std::wstring name = L"Local\\SteamSalesTray-" + userToken_;

    // Create-or-open: if it already exists, GetLastError reports it.
    mutex_ = ::CreateMutexW(nullptr, FALSE, name.c_str());
    const DWORD err = ::GetLastError();
    isPrimary_ = (mutex_ != nullptr) && (err != ERROR_ALREADY_EXISTS);
}

SingleInstance::~SingleInstance() {
    if (mutex_) {
        ::CloseHandle(mutex_);
        mutex_ = nullptr;
    }
}

}  // namespace app
