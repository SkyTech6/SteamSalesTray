#include "platform/TimeUtil.h"

#include <cstdio>
#include <windows.h>

namespace platform {

namespace {

// Locates the "Pacific Standard Time" dynamic time-zone entry (which carries
// the full DST rule set). Returns false if not found.
bool GetPacificTimeZone(DYNAMIC_TIME_ZONE_INFORMATION& out) {
    DYNAMIC_TIME_ZONE_INFORMATION dtzi;
    for (DWORD i = 0;
         ::EnumDynamicTimeZoneInformation(i, &dtzi) == ERROR_SUCCESS; ++i) {
        if (wcscmp(dtzi.TimeZoneKeyName, L"Pacific Standard Time") == 0) {
            out = dtzi;
            return true;
        }
    }
    return false;
}

// Current time expressed in Pacific local time.
SYSTEMTIME PacificNow() {
    SYSTEMTIME utc;
    ::GetSystemTime(&utc);
    DYNAMIC_TIME_ZONE_INFORMATION tz;
    SYSTEMTIME pacific;
    if (GetPacificTimeZone(tz) &&
        ::SystemTimeToTzSpecificLocalTimeEx(&tz, &utc, &pacific)) {
        return pacific;
    }
    // Fallback: machine local time.
    SYSTEMTIME local;
    ::GetLocalTime(&local);
    return local;
}

std::string FormatSteamDate(const SYSTEMTIME& st) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04u/%02u/%02u", st.wYear, st.wMonth,
                  st.wDay);
    return buf;
}

}  // namespace

std::string UtcNowIso8601() {
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02uZ", st.wYear,
                  st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::string SteamToday() {
    return FormatSteamDate(PacificNow());
}

long long SecondsSinceUtc(const std::string& iso8601) {
    constexpr long long kVeryOld = 1'000'000'000LL;  // ~31 years
    unsigned y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (std::sscanf(iso8601.c_str(), "%u-%u-%uT%u:%u:%uZ", &y, &mo, &d, &h, &mi,
                    &s) != 6) {
        return kVeryOld;
    }
    SYSTEMTIME st = {};
    st.wYear = static_cast<WORD>(y);
    st.wMonth = static_cast<WORD>(mo);
    st.wDay = static_cast<WORD>(d);
    st.wHour = static_cast<WORD>(h);
    st.wMinute = static_cast<WORD>(mi);
    st.wSecond = static_cast<WORD>(s);

    FILETIME ftThen, ftNow;
    SYSTEMTIME now;
    ::GetSystemTime(&now);
    if (!::SystemTimeToFileTime(&st, &ftThen) ||
        !::SystemTimeToFileTime(&now, &ftNow)) {
        return kVeryOld;
    }
    ULARGE_INTEGER then, cur;
    then.LowPart = ftThen.dwLowDateTime;
    then.HighPart = ftThen.dwHighDateTime;
    cur.LowPart = ftNow.dwLowDateTime;
    cur.HighPart = ftNow.dwHighDateTime;
    if (cur.QuadPart < then.QuadPart) {
        return 0;  // timestamp in the future; treat as fresh
    }
    return static_cast<long long>((cur.QuadPart - then.QuadPart) / 10'000'000ULL);
}

std::string SteamDateDaysAgo(int days) {
    SYSTEMTIME st = PacificNow();
    // Normalize to midnight so day subtraction is calendar-clean.
    st.wHour = st.wMinute = st.wSecond = st.wMilliseconds = 0;

    FILETIME ft;
    if (!::SystemTimeToFileTime(&st, &ft)) {
        return FormatSteamDate(st);
    }
    ULARGE_INTEGER v;
    v.LowPart = ft.dwLowDateTime;
    v.HighPart = ft.dwHighDateTime;
    // 100-ns ticks per day = 864000000000.
    const unsigned long long ticksPerDay = 864000000000ULL;
    if (days > 0) {
        v.QuadPart -= static_cast<unsigned long long>(days) * ticksPerDay;
    }
    ft.dwLowDateTime = v.LowPart;
    ft.dwHighDateTime = v.HighPart;

    SYSTEMTIME result;
    if (!::FileTimeToSystemTime(&ft, &result)) {
        return FormatSteamDate(st);
    }
    return FormatSteamDate(result);
}

}  // namespace platform
