#pragma once

#include <string>

namespace platform {

enum class LogLevel { Info, Warn, Error };

namespace Log {

// Optional explicit init (path + enable). If never called, the log lazily uses
// platform::GetLogPath(). Logging is thread-safe.
void Init(const std::wstring& path);

// Writes one line: "<utc> <LEVEL> <component> <message>". The message is passed
// through secret redaction as a safety net; callers must still never pass keys,
// full URLs, or financial payloads (plan §21).
void Write(LogLevel level, const wchar_t* component, const std::wstring& message);

inline void Info(const wchar_t* component, const std::wstring& msg) {
    Write(LogLevel::Info, component, msg);
}
inline void Warn(const wchar_t* component, const std::wstring& msg) {
    Write(LogLevel::Warn, component, msg);
}
inline void Error(const wchar_t* component, const std::wstring& msg) {
    Write(LogLevel::Error, component, msg);
}

}  // namespace Log

// Redacts obvious secrets from a string: any "key=<value>" query parameter has
// its value replaced with "REDACTED". Exposed for testing.
std::wstring RedactSecrets(const std::wstring& text);

}  // namespace platform
