#include "network/UrlEncoder.h"

#include <vector>
#include <windows.h>

namespace network {

namespace {
bool IsUnreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
           c == '~';
}

wchar_t HexDigit(unsigned value) {
    return static_cast<wchar_t>(value < 10 ? L'0' + value : L'A' + (value - 10));
}
}  // namespace

std::wstring EncodeComponent(const std::string& utf8Value) {
    std::wstring out;
    out.reserve(utf8Value.size() * 3);
    for (unsigned char c : utf8Value) {
        if (IsUnreserved(c)) {
            out.push_back(static_cast<wchar_t>(c));
        } else {
            out.push_back(L'%');
            out.push_back(HexDigit(c >> 4));
            out.push_back(HexDigit(c & 0x0F));
        }
    }
    return out;
}

std::wstring EncodeComponent(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int needed = ::WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
        nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::vector<char> buffer(static_cast<size_t>(needed));
    ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                          static_cast<int>(value.size()), buffer.data(), needed,
                          nullptr, nullptr);
    return EncodeComponent(std::string(buffer.data(), buffer.size()));
}

}  // namespace network
