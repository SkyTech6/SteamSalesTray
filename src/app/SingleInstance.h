#pragma once

#include <string>
#include <windows.h>

namespace app {

// Enforces one running instance per Windows user via a named mutex.
//
// Construction always attempts to create the mutex. IsPrimary() reports whether
// this process is the first (owning) instance. The mutex is held for the
// lifetime of this object and released on destruction.
class SingleInstance {
public:
    SingleInstance();
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    // True if this process created the mutex (i.e. no prior instance existed).
    bool IsPrimary() const { return isPrimary_; }

    // The per-user mutex/name suffix (a SID string, or a fallback token).
    const std::wstring& UserToken() const { return userToken_; }

private:
    HANDLE mutex_ = nullptr;
    bool isPrimary_ = false;
    std::wstring userToken_;
};

// Returns the current user's SID as a string, or a stable fallback token if it
// cannot be determined. Used to scope per-user kernel object names.
std::wstring GetCurrentUserToken();

}  // namespace app
