#pragma once

#include <string>

namespace network {

// Percent-encodes a query-string component per RFC 3986. Unreserved characters
// (A-Z a-z 0-9 - _ . ~) pass through; every other byte becomes %XX. Input is
// UTF-8 bytes; output is ASCII, returned as a wide string for use in WinHTTP
// request targets.
std::wstring EncodeComponent(const std::string& utf8Value);

// Convenience overload: encodes a wide string (converted to UTF-8 first).
std::wstring EncodeComponent(const std::wstring& value);

}  // namespace network
