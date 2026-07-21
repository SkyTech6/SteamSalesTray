#pragma once

#include <chrono>
#include <string>

namespace sync {

// High-level state of the sync worker (plan §19).
enum class SyncStatus {
    MissingApiKey,
    Idle,
    Syncing,
    Offline,
    AuthenticationFailed,
    ApiError,
    ParseError,
    DatabaseError,
};

// A snapshot of worker state, copied under lock for the UI thread.
struct AppStatus {
    SyncStatus status = SyncStatus::Idle;
    std::wstring message;
    std::chrono::system_clock::time_point lastAttempt{};
    std::chrono::system_clock::time_point lastSuccess{};
    int completedDates = 0;
    int totalDates = 0;

    // Consolidated "new units detected" text for a balloon, set by the worker
    // after a successful incremental sync when notifications are enabled and
    // there were positive increases. Empty means "nothing to notify".
    std::wstring notification;
};

}  // namespace sync
