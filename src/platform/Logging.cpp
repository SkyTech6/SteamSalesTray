#include "platform/Logging.h"

#include <mutex>
#include <windows.h>

#include "platform/Paths.h"
#include "platform/StringUtil.h"
#include "platform/TimeUtil.h"

namespace platform {

namespace {
constexpr long long kMaxBytes = 1024 * 1024;  // 1 MB per file (§21)

std::mutex g_mutex;
std::wstring g_path;  // resolved lazily under the mutex

const std::wstring& ResolvePath() {
    if (g_path.empty()) {
        g_path = GetLogPath();
    }
    return g_path;
}

const wchar_t* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Info: return L"INFO ";
        case LogLevel::Warn: return L"WARN ";
        case LogLevel::Error: return L"ERROR";
    }
    return L"INFO ";
}

long long FileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr)) {
        return 0;
    }
    LARGE_INTEGER size;
    size.LowPart = attr.nFileSizeLow;
    size.HighPart = static_cast<LONG>(attr.nFileSizeHigh);
    return size.QuadPart;
}

// Rotates current -> ".1" (deleting the previous ".1") when oversized. Keeps
// two files total (§21).
void RotateIfNeeded(const std::wstring& path) {
    if (FileSize(path) < kMaxBytes) {
        return;
    }
    const std::wstring rotated = path + L".1";
    ::DeleteFileW(rotated.c_str());
    ::MoveFileW(path.c_str(), rotated.c_str());
}

void AppendUtf8(const std::wstring& path, const std::string& utf8) {
    HANDLE file = ::CreateFileW(path.c_str(), FILE_APPEND_DATA,
                                FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    ::WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written,
                nullptr);
    ::CloseHandle(file);
}
}  // namespace

void Log::Init(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_path = path;
}

void Log::Write(LogLevel level, const wchar_t* component,
                const std::wstring& message) {
    std::wstring line = platform::Utf8ToWide(UtcNowIso8601());
    line += L' ';
    line += LevelName(level);
    line += L' ';
    line += component;
    line += L' ';
    line += RedactSecrets(message);
    line += L"\r\n";

    std::lock_guard<std::mutex> lock(g_mutex);
    const std::wstring& path = ResolvePath();
    if (path.empty()) {
        return;
    }
    RotateIfNeeded(path);
    AppendUtf8(path, WideToUtf8(line));
}

std::wstring RedactSecrets(const std::wstring& text) {
    std::wstring out = text;
    const std::wstring needle = L"key=";
    size_t pos = 0;
    while ((pos = out.find(needle, pos)) != std::wstring::npos) {
        const size_t valueStart = pos + needle.size();
        size_t valueEnd = out.find_first_of(L"& \t\r\n\"'", valueStart);
        if (valueEnd == std::wstring::npos) {
            valueEnd = out.size();
        }
        out.replace(valueStart, valueEnd - valueStart, L"REDACTED");
        pos = valueStart + 8;  // past "REDACTED"
    }
    return out;
}

}  // namespace platform
