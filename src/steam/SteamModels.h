#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace steam {

// Result of GetChangedDatesForPartner.
struct ChangedDates {
    // Steam financial dates ("YYYY/MM/DD", Pacific time) whose data changed.
    std::vector<std::string> dates;
    // Global high-water mark. Documented as an unsigned 64-bit id, sometimes
    // returned as a string, so we keep it as text (see plan §9).
    std::string resultHighwatermark;
};

// One raw line-item record from GetDetailedSales, before category filtering.
// Filtering/aggregation (Package + Steam, group by primary_appid) happens in
// the aggregator (Phase 4) so this stays a faithful representation.
struct DetailedSalesRecord {
    std::string lineItemType;      // e.g. "Package", "MicroTxn"
    std::string packageSaleType;   // e.g. "Steam", "Retail"
    std::int64_t primaryAppId = 0;
    std::string primaryAppName;
    std::int64_t netUnitsSold = 0;
    std::int64_t grossUnitsSold = 0;
    std::int64_t grossUnitsReturned = 0;
};

// One page of GetDetailedSales.
struct DetailedSalesPage {
    std::vector<DetailedSalesRecord> records;
    // Pagination cursor. Requesting with highwatermark_id == this value's
    // previous request terminates the loop (see plan §6).
    std::string maxId;
    bool hasMaxId = false;
};

}  // namespace steam
