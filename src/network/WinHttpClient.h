#pragma once

#include <string>
#include <windows.h>
#include <winhttp.h>

namespace network {

// RAII wrapper around a WinHTTP HINTERNET handle.
class WinHttpHandle {
public:
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : handle_(handle) {}
    ~WinHttpHandle() { Close(); }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
        if (this != &other) {
            Close();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    void Reset(HINTERNET handle = nullptr) {
        if (handle_ != handle) {
            Close();
            handle_ = handle;
        }
    }
    HINTERNET Get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    void Close() {
        if (handle_) {
            ::WinHttpCloseHandle(handle_);
            handle_ = nullptr;
        }
    }
    HINTERNET handle_ = nullptr;
};

// Outcome of a single HTTPS GET.
struct HttpResult {
    bool transportOk = false;  // request completed and a status was read
    DWORD win32Error = 0;      // WinHTTP/Win32 error when transportOk is false
    int statusCode = 0;        // HTTP status when transportOk is true
    bool exceededLimit = false;  // body hit the size cap
    std::string body;          // response body (empty if exceededLimit)
};

// One reusable WinHTTP session + connection to a single host. Synchronous;
// intended to be owned by the sync worker thread.
class WinHttpClient {
public:
    WinHttpClient();
    ~WinHttpClient() = default;

    WinHttpClient(const WinHttpClient&) = delete;
    WinHttpClient& operator=(const WinHttpClient&) = delete;

    bool IsValid() const { return static_cast<bool>(session_); }

    // Performs GET https://<host>:<port><path>?<query> over TLS.
    // `maxBytes` caps the body; exceeding it aborts the read (exceededLimit).
    HttpResult Get(const wchar_t* host, INTERNET_PORT port,
                   const std::wstring& path, const std::wstring& query,
                   size_t maxBytes);

private:
    bool EnsureConnection(const wchar_t* host, INTERNET_PORT port);

    WinHttpHandle session_;
    WinHttpHandle connection_;
    std::wstring connectedHost_;
    INTERNET_PORT connectedPort_ = 0;
};

// Formats a WinHTTP error code into a sanitized, human-readable message.
// Never includes any URL or key material.
std::wstring DescribeWinHttpError(DWORD code);

}  // namespace network
