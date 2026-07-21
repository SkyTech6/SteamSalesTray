#include "network/WinHttpClient.h"

#include <string>

#pragma comment(lib, "winhttp.lib")

namespace network {

namespace {
constexpr wchar_t kUserAgent[] = L"SteamSalesTray/1.0";

// Timeouts (milliseconds), per plan §16.
constexpr int kResolveTimeout = 10'000;
constexpr int kConnectTimeout = 10'000;
constexpr int kSendTimeout = 15'000;
constexpr int kReceiveTimeout = 30'000;

constexpr DWORD kReadChunk = 64 * 1024;
}  // namespace

WinHttpClient::WinHttpClient() {
    // Prefer automatic proxy discovery; fall back to no proxy if unavailable.
    HINTERNET session = ::WinHttpOpen(
        kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        session = ::WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
                                0);
    }
    session_.Reset(session);
    if (session_) {
        ::WinHttpSetTimeouts(session_.Get(), kResolveTimeout, kConnectTimeout,
                             kSendTimeout, kReceiveTimeout);
        // Require modern TLS.
        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
        protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
        ::WinHttpSetOption(session_.Get(), WINHTTP_OPTION_SECURE_PROTOCOLS,
                           &protocols, sizeof(protocols));
    }
}

bool WinHttpClient::EnsureConnection(const wchar_t* host, INTERNET_PORT port) {
    if (connection_ && connectedHost_ == host && connectedPort_ == port) {
        return true;
    }
    connection_.Reset();
    HINTERNET conn = ::WinHttpConnect(session_.Get(), host, port, 0);
    if (!conn) {
        return false;
    }
    connection_.Reset(conn);
    connectedHost_ = host;
    connectedPort_ = port;
    return true;
}

HttpResult WinHttpClient::Get(const wchar_t* host, INTERNET_PORT port,
                              const std::wstring& path,
                              const std::wstring& query, size_t maxBytes) {
    HttpResult result;
    if (!session_) {
        result.win32Error = ERROR_INVALID_HANDLE;
        return result;
    }
    if (!EnsureConnection(host, port)) {
        result.win32Error = ::GetLastError();
        return result;
    }

    std::wstring target = path;
    if (!query.empty()) {
        target += L"?";
        target += query;
    }

    WinHttpHandle request(::WinHttpOpenRequest(
        connection_.Get(), L"GET", target.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (!request) {
        result.win32Error = ::GetLastError();
        return result;
    }

    const wchar_t kHeaders[] =
        L"Accept: application/json\r\nCache-Control: no-cache";

    if (!::WinHttpSendRequest(request.Get(), kHeaders, static_cast<DWORD>(-1),
                              WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !::WinHttpReceiveResponse(request.Get(), nullptr)) {
        result.win32Error = ::GetLastError();
        return result;
    }

    // Status code.
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    if (!::WinHttpQueryHeaders(
            request.Get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size,
            WINHTTP_NO_HEADER_INDEX)) {
        result.win32Error = ::GetLastError();
        return result;
    }
    result.transportOk = true;
    result.statusCode = static_cast<int>(statusCode);

    // Body.
    for (;;) {
        DWORD available = 0;
        if (!::WinHttpQueryDataAvailable(request.Get(), &available)) {
            result.transportOk = false;
            result.win32Error = ::GetLastError();
            result.body.clear();
            return result;
        }
        if (available == 0) {
            break;  // end of response
        }
        const DWORD toRead = available < kReadChunk ? available : kReadChunk;
        if (result.body.size() + toRead > maxBytes) {
            result.exceededLimit = true;
            result.body.clear();
            return result;
        }
        const size_t offset = result.body.size();
        result.body.resize(offset + toRead);
        DWORD read = 0;
        if (!::WinHttpReadData(request.Get(), result.body.data() + offset,
                               toRead, &read)) {
            result.transportOk = false;
            result.win32Error = ::GetLastError();
            result.body.clear();
            return result;
        }
        result.body.resize(offset + read);
        if (read == 0) {
            break;
        }
    }

    return result;
}

std::wstring DescribeWinHttpError(DWORD code) {
    // WinHTTP error messages live in winhttp.dll's message table.
    HMODULE module = ::GetModuleHandleW(L"winhttp.dll");
    LPWSTR buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS;
    flags |= (code >= WINHTTP_ERROR_BASE && code <= WINHTTP_ERROR_LAST)
                 ? FORMAT_MESSAGE_FROM_HMODULE
                 : FORMAT_MESSAGE_FROM_SYSTEM;
    const DWORD len = ::FormatMessageW(
        flags, module, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    std::wstring message;
    if (len && buffer) {
        message.assign(buffer, len);
        while (!message.empty() &&
               (message.back() == L'\r' || message.back() == L'\n' ||
                message.back() == L'.' || message.back() == L' ')) {
            message.pop_back();
        }
    }
    if (buffer) {
        ::LocalFree(buffer);
    }
    if (message.empty()) {
        message = L"network error";
    }
    return message + L" (0x" +
           [code] {
               wchar_t hex[16];
               swprintf_s(hex, L"%08X", code);
               return std::wstring(hex);
           }() +
           L")";
}

}  // namespace network
