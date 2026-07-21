#pragma once

#include <string>
#include <windows.h>

namespace platform {

// Formats a Win32 error code (e.g. from GetLastError()) into a human-readable
// message. Never throws; falls back to the numeric code on failure.
std::wstring FormatWin32Error(DWORD code);

// Convenience: format the current thread's last error.
inline std::wstring LastErrorMessage() {
    return FormatWin32Error(::GetLastError());
}

}  // namespace platform
