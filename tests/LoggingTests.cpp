// Unit tests for logging: secret redaction + size-based rotation.
#include <cstdio>
#include <string>
#include <windows.h>

#include "platform/Logging.h"

static int g_fail = 0;
#define CHECK(cond, msg)                                       \
    do {                                                       \
        if (cond) { std::wprintf(L"  PASS: %s\n", msg); }      \
        else { std::wprintf(L"  FAIL: %s\n", msg); ++g_fail; } \
    } while (0)

static long long FileSize(const std::wstring& p) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!::GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &a)) return -1;
    LARGE_INTEGER s;
    s.LowPart = a.nFileSizeLow;
    s.HighPart = (LONG)a.nFileSizeHigh;
    return s.QuadPart;
}

int main() {
    using platform::RedactSecrets;

    std::wprintf(L"[Redaction]\n");
    CHECK(RedactSecrets(L"GET /x?key=ABC123&date=2026/07/18") ==
              L"GET /x?key=REDACTED&date=2026/07/18",
          L"key value redacted, other params kept");
    CHECK(RedactSecrets(L"trailing key=SECRETVALUE") == L"trailing key=REDACTED",
          L"key at end redacted");
    CHECK(RedactSecrets(L"no secrets here") == L"no secrets here",
          L"non-secret text unchanged");

    std::wprintf(L"[Rotation]\n");
    wchar_t tmp[MAX_PATH];
    ::GetTempPathW(MAX_PATH, tmp);
    std::wstring path = std::wstring(tmp) + L"sst_ut_log.log";
    std::wstring rotated = path + L".1";
    ::DeleteFileW(path.c_str());
    ::DeleteFileW(rotated.c_str());

    platform::Log::Init(path);
    const std::wstring big(100 * 1024, L'x');  // ~100 KB per line
    for (int i = 0; i < 14; ++i) {             // ~1.4 MB total
        platform::Log::Info(L"Test", big);
    }
    CHECK(FileSize(rotated) > 0, L"rotated file .1 created after exceeding 1 MB");
    CHECK(FileSize(path) >= 0 && FileSize(path) < 2 * 1024 * 1024,
          L"current file bounded (rotation occurred)");

    ::DeleteFileW(path.c_str());
    ::DeleteFileW(rotated.c_str());

    std::wprintf(L"\n%s (failures=%d)\n",
                 g_fail ? L"TESTS FAILED" : L"ALL TESTS PASSED", g_fail);
    return g_fail ? 1 : 0;
}
