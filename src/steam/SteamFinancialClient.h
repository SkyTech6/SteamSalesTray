#pragma once

#include <string>
#include <vector>

#include "network/WinHttpClient.h"
#include "security/SecureString.h"
#include "steam/SteamModels.h"

namespace steam {

// Classification of a request outcome, driving the UI/error policy (§20).
enum class ResultKind {
    Success,
    Offline,        // DNS/connect/timeout — preserve cache, retry later
    Auth,           // 401/403 or rejected key — stop retrying, prompt user
    RateLimited,    // 429
    ServerError,    // 5xx
    TooLarge,       // body exceeded the size cap
    Parse,          // 2xx but body unparseable / wrong shape
    Other,
};

struct FinancialResult {
    ResultKind kind = ResultKind::Other;
    int httpStatus = 0;
    // Sanitized: never contains the API key, query string, or full URL.
    std::wstring message;

    bool ok() const { return kind == ResultKind::Success; }
};

// Talks to IPartnerFinancialsService over HTTPS. Owns one WinHTTP session; keep
// one instance on the sync worker thread.
class SteamFinancialClient {
public:
    SteamFinancialClient() = default;

    bool IsValid() const { return http_.IsValid(); }

    // GetChangedDatesForPartner with the stored global high-water mark.
    FinancialResult GetChangedDates(const security::SecureString& keyUtf8,
                                    const std::string& highwatermark,
                                    ChangedDates& out);

    // GetDetailedSales: a single page for `date` at `highwatermarkId`.
    FinancialResult GetDetailedSales(const security::SecureString& keyUtf8,
                                     const std::string& date,
                                     const std::string& highwatermarkId,
                                     DetailedSalesPage& out);

    // Fetches every page for `date`, following the max_id cursor until it stops
    // advancing (plan §6), collecting all raw records. Stops early and returns
    // the failing result on any page error.
    FinancialResult GetAllDetailedSalesForDate(
        const security::SecureString& keyUtf8, const std::string& date,
        std::vector<DetailedSalesRecord>& out);

private:
    network::WinHttpClient http_;
};

}  // namespace steam
