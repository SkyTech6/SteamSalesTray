#pragma once

#include <string>

namespace platform {

// Current UTC time as ISO 8601, e.g. "2026-07-20T20:15:03Z".
std::string UtcNowIso8601();

// The current Steam financial date in Pacific time as "YYYY/MM/DD" (§10).
// Uses the Windows time-zone database so DST is respected. Falls back to the
// machine's local date only if the Pacific zone cannot be resolved.
std::string SteamToday();

// A Steam financial date `days` before today (Pacific), as "YYYY/MM/DD".
// SteamDateDaysAgo(0) == SteamToday(). Used to build rolling-window ranges;
// because the format is zero-padded, lexical comparison equals date order.
std::string SteamDateDaysAgo(int days);

// Seconds elapsed from an ISO 8601 UTC timestamp ("...Z") until now. Returns a
// very large value if the input is empty or unparseable (i.e. "treat as old").
long long SecondsSinceUtc(const std::string& iso8601);

}  // namespace platform
