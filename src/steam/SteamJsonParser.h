#pragma once

#include <string>

#include "steam/SteamModels.h"

namespace steam {

enum class ParseStatus {
    Ok,
    InvalidJson,       // not parseable as JSON
    MissingResponse,   // no top-level "response" object
    ApiError,          // Steam returned a recognizable error payload
    StructureError,    // response present but required fields malformed/absent
};

struct ParseResult {
    ParseStatus status = ParseStatus::StructureError;
    std::wstring message;  // sanitized; safe to log/display
    bool ok() const { return status == ParseStatus::Ok; }
};

// Parses a GetChangedDatesForPartner response body. An empty "dates" array is
// valid (no changes). Requires a top-level "response" object.
ParseResult ParseChangedDates(const std::string& json, ChangedDates& out);

// Parses one GetDetailedSales page. Records are returned unfiltered.
ParseResult ParseDetailedSalesPage(const std::string& json,
                                   DetailedSalesPage& out);

}  // namespace steam
