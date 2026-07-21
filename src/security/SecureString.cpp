#include "security/SecureString.h"

#include <vector>
#include <windows.h>

namespace security {

SecureString::~SecureString() {
    Wipe();
}

SecureString::SecureString(SecureString&& other) noexcept {
    value_ = std::move(other.value_);
    other.Wipe();
}

SecureString& SecureString::operator=(SecureString&& other) noexcept {
    if (this != &other) {
        Wipe();
        value_ = std::move(other.value_);
        other.Wipe();
    }
    return *this;
}

void SecureString::Assign(const char* data, size_t length) {
    Wipe();
    value_.assign(data, length);
}

void SecureString::Wipe() noexcept {
    if (!value_.empty()) {
        ::SecureZeroMemory(value_.data(), value_.size());
    }
    value_.clear();
    value_.shrink_to_fit();
}

SecureString WideToSecureUtf8(const std::wstring& text) {
    SecureString result;
    if (text.empty()) {
        return result;
    }
    const int needed = ::WideCharToMultiByte(
        CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return result;
    }
    std::vector<char> buffer(static_cast<size_t>(needed));
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                          static_cast<int>(text.size()),
                          buffer.data(), needed, nullptr, nullptr);
    result.Assign(buffer.data(), buffer.size());
    ::SecureZeroMemory(buffer.data(), buffer.size());
    return result;
}

void WipeWString(std::wstring& text) noexcept {
    if (!text.empty()) {
        ::SecureZeroMemory(text.data(), text.size() * sizeof(wchar_t));
    }
    text.clear();
    text.shrink_to_fit();
}

}  // namespace security
