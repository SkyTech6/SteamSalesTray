#include "platform/Win32Error.h"

namespace platform {

std::wstring FormatWin32Error(DWORD code) {
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = ::FormatMessageW(
        flags, nullptr, code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    std::wstring result;
    if (len && buffer) {
        result.assign(buffer, len);
        // Trim trailing CR/LF/period that FormatMessage tends to append.
        while (!result.empty() &&
               (result.back() == L'\r' || result.back() == L'\n' ||
                result.back() == L'.' || result.back() == L' ')) {
            result.pop_back();
        }
    }
    if (buffer) {
        ::LocalFree(buffer);
    }
    if (result.empty()) {
        result = L"Win32 error " + std::to_wstring(code);
    } else {
        result += L" (error " + std::to_wstring(code) + L")";
    }
    return result;
}

}  // namespace platform
