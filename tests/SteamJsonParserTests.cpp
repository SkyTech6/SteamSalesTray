// Unit tests for the Steam JSON parser and URL encoder (Phase 3).
// Standalone: compile with SteamJsonParser.cpp, UrlEncoder.cpp, yyjson.c.
#include <cstdio>
#include <string>

#include "network/UrlEncoder.h"
#include "steam/SteamJsonParser.h"

static int g_fail = 0;
#define CHECK(cond, msg)                                          \
    do {                                                          \
        if (cond) {                                               \
            std::printf("  PASS: %s\n", msg);                     \
        } else {                                                  \
            std::printf("  FAIL: %s\n", msg);                     \
            ++g_fail;                                             \
        }                                                         \
    } while (0)

using namespace steam;

static void TestChangedDates() {
    std::printf("[ChangedDates]\n");
    {
        ChangedDates out;
        auto r = ParseChangedDates(
            R"({"response":{"dates":["2026/07/18","2026/07/19"],)"
            R"("result_highwatermark":"1841518117865"}})",
            out);
        CHECK(r.ok(), "normal parses ok");
        CHECK(out.dates.size() == 2, "two dates");
        CHECK(out.dates.size() == 2 && out.dates[0] == "2026/07/18",
              "first date preserved verbatim");
        CHECK(out.resultHighwatermark == "1841518117865", "hwm as string");
    }
    {
        ChangedDates out;
        auto r = ParseChangedDates(
            R"({"response":{"result_highwatermark":"5"}})", out);
        CHECK(r.ok() && out.dates.empty(), "empty (missing dates) is valid");
    }
    {
        ChangedDates out;  // hwm as JSON number, incl. beyond int64 range
        auto r = ParseChangedDates(
            R"({"response":{"dates":[],"result_highwatermark":18446744073709551615}})",
            out);
        CHECK(r.ok() && out.resultHighwatermark == "18446744073709551615",
              "unsigned 64-bit hwm beyond int64 preserved");
    }
    {
        ChangedDates out;
        auto r = ParseChangedDates(R"({"something":1})", out);
        CHECK(r.status == ParseStatus::MissingResponse, "missing response");
    }
    {
        ChangedDates out;
        auto r = ParseChangedDates(R"({"error":{"code":403}})", out);
        CHECK(r.status == ParseStatus::ApiError, "api error payload detected");
    }
    {
        ChangedDates out;
        auto r = ParseChangedDates("{not valid json", out);
        CHECK(r.status == ParseStatus::InvalidJson, "invalid json");
    }
}

static void TestDetailedSales() {
    std::printf("[DetailedSales]\n");
    {
        DetailedSalesPage out;
        auto r = ParseDetailedSalesPage(
            R"({"response":{"max_id":"900","sales":[)"
            R"({"line_item_type":"Package","package_sale_type":"Steam",)"
            R"("primary_appid":480,"primary_app_name":"git gud",)"
            R"("net_units_sold":2,"gross_units_sold":3,"gross_units_returned":1},)"
            R"({"line_item_type":"MicroTxn","package_sale_type":"Steam",)"
            R"("primary_appid":481,"net_units_sold":99},)"
            R"({"line_item_type":"Package","package_sale_type":"Retail",)"
            R"("primary_appid":482,"net_units_sold":-3})"
            R"(]}})",
            out);
        CHECK(r.ok(), "detailed page parses ok");
        CHECK(out.records.size() == 3, "all three records returned unfiltered");
        CHECK(out.maxId == "900" && out.hasMaxId, "max_id captured");
        if (out.records.size() == 3) {
            CHECK(out.records[0].lineItemType == "Package" &&
                      out.records[0].packageSaleType == "Steam" &&
                      out.records[0].primaryAppId == 480 &&
                      out.records[0].primaryAppName == "git gud" &&
                      out.records[0].netUnitsSold == 2 &&
                      out.records[0].grossUnitsSold == 3 &&
                      out.records[0].grossUnitsReturned == 1,
                  "package/steam record fields");
            CHECK(out.records[2].netUnitsSold == -3,
                  "negative net units preserved (refund/correction)");
        }
    }
    {
        DetailedSalesPage out;  // alternate array key + numeric max_id + string net
        auto r = ParseDetailedSalesPage(
            R"({"response":{"max_id":1234567890123,"line_items":[)"
            R"({"line_item_type":"Package","package_sale_type":"Steam",)"
            R"("primary_appid":"730","net_units_sold":"-5"}]}})",
            out);
        CHECK(r.ok() && out.records.size() == 1, "alternate 'line_items' key");
        CHECK(out.maxId == "1234567890123", "numeric max_id -> string");
        if (out.records.size() == 1) {
            CHECK(out.records[0].primaryAppId == 730,
                  "numeric-string app id parsed");
            CHECK(out.records[0].netUnitsSold == -5,
                  "numeric-string negative net parsed");
        }
    }
    {
        DetailedSalesPage out;  // trailing empty page
        auto r = ParseDetailedSalesPage(R"({"response":{"max_id":"0"}})", out);
        CHECK(r.ok() && out.records.empty() && out.maxId == "0",
              "empty page valid, cursor read");
    }
    {
        // Real API shape: records under "results", no per-record app name,
        // names resolved from the "app_info" lookup array.
        DetailedSalesPage out;
        auto r = ParseDetailedSalesPage(
            R"({"response":{"max_id":"5","app_info":[)"
            R"({"appid":1490570,"app_name":"git gud"}],)"
            R"("results":[)"
            R"({"line_item_type":"Package","package_sale_type":"Steam",)"
            R"("primary_appid":1490570,"net_units_sold":2,)"
            R"("gross_units_sold":3,"gross_units_returned":1})"
            R"(]}})",
            out);
        CHECK(r.ok() && out.records.size() == 1, "real-shape results parsed");
        if (out.records.size() == 1) {
            CHECK(out.records[0].primaryAppId == 1490570 &&
                      out.records[0].netUnitsSold == 2,
                  "real-shape metric fields parsed");
            CHECK(out.records[0].primaryAppName == "git gud",
                  "name resolved from app_info lookup");
        }
    }
}

static void TestUrlEncoder() {
    std::printf("[UrlEncoder]\n");
    using network::EncodeComponent;
    CHECK(EncodeComponent(std::string("2026/07/18")) == L"2026%2F07%2F18",
          "slashes encoded");
    CHECK(EncodeComponent(std::string("ABC-abc_123.~")) == L"ABC-abc_123.~",
          "unreserved pass through");
    CHECK(EncodeComponent(std::string("a b&c=d")) == L"a%20b%26c%3Dd",
          "space, ampersand, equals encoded");
}

int main() {
    TestChangedDates();
    TestDetailedSales();
    TestUrlEncoder();
    std::printf("\n%s (failures=%d)\n",
                g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail);
    return g_fail ? 1 : 0;
}
