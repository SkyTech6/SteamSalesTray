#include "platform/StringUtil.h"

#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>

namespace platform {

std::wstring FormatThousands(long long value) {
    const bool negative = value < 0;
    // Compute magnitude without overflow at LLONG_MIN.
    unsigned long long magnitude =
        negative ? static_cast<unsigned long long>(-(value + 1)) + 1ULL
                 : static_cast<unsigned long long>(value);
    const std::wstring digits = std::to_wstring(magnitude);
    std::wstring out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count && count % 3 == 0) {
            out.push_back(L',');
        }
        out.push_back(*it);
        ++count;
    }
    if (negative) {
        out.push_back(L'-');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                          static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int needed = ::WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
        nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                          static_cast<int>(wide.size()), out.data(), needed,
                          nullptr, nullptr);
    return out;
}

}  // namespace platform
