#pragma once

#include <string>

namespace security {

// Holds secret bytes (e.g. the decrypted API key as UTF-8/ASCII) and zeroes its
// backing store with SecureZeroMemory on destruction or clear.
//
// This minimizes how long plaintext lingers in memory; it does not guarantee no
// copies ever existed (e.g. small-string moves), consistent with the plan's
// "minimize exposure" goal rather than an absolute guarantee.
class SecureString {
public:
    SecureString() = default;
    ~SecureString();

    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;

    SecureString(SecureString&& other) noexcept;
    SecureString& operator=(SecureString&& other) noexcept;

    void Assign(const char* data, size_t length);
    void Wipe() noexcept;

    const char* Data() const { return value_.empty() ? "" : value_.data(); }
    size_t Size() const { return value_.size(); }
    bool Empty() const { return value_.empty(); }

private:
    std::string value_;
};

// Converts a wide string to UTF-8 bytes in a SecureString. Any intermediate
// buffer is wiped; the caller remains responsible for wiping `text`.
SecureString WideToSecureUtf8(const std::wstring& text);

// Overwrites a std::wstring's backing buffer with zeros, then clears it. Use on
// plaintext read from edit controls once it is no longer needed.
void WipeWString(std::wstring& text) noexcept;

}  // namespace security
