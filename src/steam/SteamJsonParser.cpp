#include "steam/SteamJsonParser.h"

#include <cstdlib>
#include <unordered_map>

#include "yyjson.h"

namespace steam {

namespace {

// ---------------------------------------------------------------------------
// Steam Financial API field names.
//
// Confirmed against a live IPartnerFinancialsService/GetDetailedSales/v001
// response: line-item records live under response.results; each Steam package
// row carries primary_appid, net/gross_units_sold, gross_units_returned. There
// is no per-record app name - names come from the response.app_info lookup
// (an array of {appid, app_name}).
// ---------------------------------------------------------------------------
constexpr const char* kResponse = "response";

// GetChangedDatesForPartner
constexpr const char* kDates = "dates";
constexpr const char* kResultHighwatermark = "result_highwatermark";

// GetDetailedSales
constexpr const char* kMaxId = "max_id";
// Candidate keys for the array of line-item records (first match wins). The
// live API uses "results"; the others are tolerated for safety.
constexpr const char* kRecordArrayKeys[] = {"results", "sales", "line_items"};

// Record fields
constexpr const char* kLineItemType = "line_item_type";
constexpr const char* kPackageSaleType = "package_sale_type";
constexpr const char* kPrimaryAppId = "primary_appid";
constexpr const char* kPrimaryAppName = "primary_app_name";  // legacy/fallback
constexpr const char* kNetUnitsSold = "net_units_sold";
constexpr const char* kGrossUnitsSold = "gross_units_sold";
constexpr const char* kGrossUnitsReturned = "gross_units_returned";

// app_info lookup block: array of { appid, app_name }.
constexpr const char* kAppInfo = "app_info";
constexpr const char* kAppInfoAppId = "appid";
constexpr const char* kAppInfoName = "app_name";

// Common Steam error indicator.
constexpr const char* kError = "error";

std::string GetStr(yyjson_val* obj, const char* key) {
    yyjson_val* v = yyjson_obj_get(obj, key);
    if (v && yyjson_is_str(v)) {
        return std::string(yyjson_get_str(v), yyjson_get_len(v));
    }
    return {};
}

// Reads a value that may be a JSON number or a numeric string as int64.
std::int64_t GetInt64(yyjson_val* obj, const char* key) {
    yyjson_val* v = yyjson_obj_get(obj, key);
    if (!v) {
        return 0;
    }
    if (yyjson_is_sint(v) || yyjson_is_int(v)) {
        return yyjson_get_sint(v);
    }
    if (yyjson_is_uint(v)) {
        return static_cast<std::int64_t>(yyjson_get_uint(v));
    }
    if (yyjson_is_real(v)) {
        return static_cast<std::int64_t>(yyjson_get_real(v));
    }
    if (yyjson_is_str(v)) {
        return std::strtoll(yyjson_get_str(v), nullptr, 10);
    }
    return 0;
}

// Reads a value that may be a number or string, returning it as text. Used for
// unsigned 64-bit high-water marks that can exceed signed range.
std::string GetNumberAsString(yyjson_val* obj, const char* key) {
    yyjson_val* v = yyjson_obj_get(obj, key);
    if (!v) {
        return {};
    }
    if (yyjson_is_str(v)) {
        return std::string(yyjson_get_str(v), yyjson_get_len(v));
    }
    if (yyjson_is_uint(v)) {
        return std::to_string(yyjson_get_uint(v));
    }
    if (yyjson_is_sint(v) || yyjson_is_int(v)) {
        return std::to_string(yyjson_get_sint(v));
    }
    return {};
}

// Returns the "response" object, or nullptr, classifying failures.
yyjson_val* GetResponse(yyjson_doc* doc, ParseResult& result) {
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        result.status = ParseStatus::StructureError;
        result.message = L"response is not a JSON object";
        return nullptr;
    }
    // Surface a top-level Steam error payload if present.
    yyjson_val* err = yyjson_obj_get(root, kError);
    if (err) {
        result.status = ParseStatus::ApiError;
        result.message = L"Steam returned an API error";
        return nullptr;
    }
    yyjson_val* response = yyjson_obj_get(root, kResponse);
    if (!response || !yyjson_is_obj(response)) {
        result.status = ParseStatus::MissingResponse;
        result.message = L"missing \"response\" object";
        return nullptr;
    }
    return response;
}

}  // namespace

ParseResult ParseChangedDates(const std::string& json, ChangedDates& out) {
    out = {};
    ParseResult result;

    yyjson_doc* doc = yyjson_read(json.data(), json.size(), 0);
    if (!doc) {
        result.status = ParseStatus::InvalidJson;
        result.message = L"could not parse JSON";
        return result;
    }

    yyjson_val* response = GetResponse(doc, result);
    if (!response) {
        yyjson_doc_free(doc);
        return result;
    }

    yyjson_val* dates = yyjson_obj_get(response, kDates);
    if (dates && yyjson_is_arr(dates)) {
        size_t idx, max;
        yyjson_val* item;
        yyjson_arr_foreach(dates, idx, max, item) {
            if (yyjson_is_str(item)) {
                out.dates.emplace_back(yyjson_get_str(item),
                                       yyjson_get_len(item));
            }
        }
    }
    // A missing/empty dates array is valid (no changes).

    out.resultHighwatermark = GetNumberAsString(response, kResultHighwatermark);

    result.status = ParseStatus::Ok;
    yyjson_doc_free(doc);
    return result;
}

ParseResult ParseDetailedSalesPage(const std::string& json,
                                   DetailedSalesPage& out) {
    out = {};
    ParseResult result;

    yyjson_doc* doc = yyjson_read(json.data(), json.size(), 0);
    if (!doc) {
        result.status = ParseStatus::InvalidJson;
        result.message = L"could not parse JSON";
        return result;
    }

    yyjson_val* response = GetResponse(doc, result);
    if (!response) {
        yyjson_doc_free(doc);
        return result;
    }

    out.maxId = GetNumberAsString(response, kMaxId);
    out.hasMaxId = !out.maxId.empty();

    // Build appid -> app name from the app_info lookup block (names are not on
    // the records themselves).
    std::unordered_map<std::int64_t, std::string> appNames;
    yyjson_val* appInfo = yyjson_obj_get(response, kAppInfo);
    if (appInfo && yyjson_is_arr(appInfo)) {
        size_t i, n;
        yyjson_val* entry;
        yyjson_arr_foreach(appInfo, i, n, entry) {
            if (!yyjson_is_obj(entry)) {
                continue;
            }
            const std::int64_t id = GetInt64(entry, kAppInfoAppId);
            std::string name = GetStr(entry, kAppInfoName);
            if (id != 0 && !name.empty()) {
                appNames[id] = std::move(name);
            }
        }
    }

    // Locate the records array under the first matching candidate key.
    yyjson_val* records = nullptr;
    for (const char* key : kRecordArrayKeys) {
        yyjson_val* candidate = yyjson_obj_get(response, key);
        if (candidate && yyjson_is_arr(candidate)) {
            records = candidate;
            break;
        }
    }

    if (records) {
        size_t idx, max;
        yyjson_val* item;
        yyjson_arr_foreach(records, idx, max, item) {
            if (!yyjson_is_obj(item)) {
                continue;
            }
            DetailedSalesRecord rec;
            rec.lineItemType = GetStr(item, kLineItemType);
            rec.packageSaleType = GetStr(item, kPackageSaleType);
            rec.primaryAppId = GetInt64(item, kPrimaryAppId);
            rec.primaryAppName = GetStr(item, kPrimaryAppName);
            rec.netUnitsSold = GetInt64(item, kNetUnitsSold);
            rec.grossUnitsSold = GetInt64(item, kGrossUnitsSold);
            rec.grossUnitsReturned = GetInt64(item, kGrossUnitsReturned);
            // Resolve the name from app_info when the record has none.
            if (rec.primaryAppName.empty()) {
                auto it = appNames.find(rec.primaryAppId);
                if (it != appNames.end()) {
                    rec.primaryAppName = it->second;
                }
            }
            out.records.push_back(std::move(rec));
        }
    }
    // No records array is valid (e.g. a trailing empty page).

    result.status = ParseStatus::Ok;
    yyjson_doc_free(doc);
    return result;
}

}  // namespace steam
