// Unit tests for aggregation + SQLite persistence (Phase 4).
// Standalone: compile with Database.cpp, Migrations.cpp, SalesAggregator.cpp,
// StringUtil.cpp, sqlite3.c.
#include <cstdio>
#include <string>
#include <windows.h>

#include "storage/Database.h"
#include "sync/SalesAggregator.h"

static int g_fail = 0;
#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (cond) { std::printf("  PASS: %s\n", msg); }                      \
        else { std::printf("  FAIL: %s\n", msg); ++g_fail; }                 \
    } while (0)

using steam::DetailedSalesRecord;

static DetailedSalesRecord Rec(const char* type, const char* sale,
                               std::int64_t app, const char* name,
                               std::int64_t net, std::int64_t gross = 0,
                               std::int64_t ret = 0) {
    DetailedSalesRecord r;
    r.lineItemType = type;
    r.packageSaleType = sale;
    r.primaryAppId = app;
    r.primaryAppName = name;
    r.netUnitsSold = net;
    r.grossUnitsSold = gross;
    r.grossUnitsReturned = ret;
    return r;
}

static std::int64_t UnitsFor(const std::vector<storage::ProductUnits>& v,
                             std::int64_t app) {
    for (const auto& p : v)
        if (p.appId == app) return p.units;
    return -999999;  // sentinel: not present
}

static void TestAggregation() {
    std::printf("[Aggregation]\n");
    std::vector<DetailedSalesRecord> recs = {
        Rec("Package", "Steam", 480, "git gud", 2, 3, 1),
        Rec("Package", "Steam", 480, "", 1, 1, 0),        // 2nd pkg, same app
        Rec("MicroTxn", "Steam", 481, "MTX", 99),          // excluded (MicroTxn)
        Rec("Package", "Retail", 482, "CDKey", 5),         // excluded (Retail)
        Rec("Package", "Steam", 0, "orphan", 7),           // excluded (app 0)
        Rec("Package", "Steam", 490, "Deluxe", -3, 0, 3),  // negative net
    };
    auto rows = sync::AggregateDate(recs);
    CHECK(rows.size() == 2, "only Steam packages with app id kept (2 groups)");

    std::int64_t net480 = 0, net490 = 0;
    std::string name480;
    for (auto& r : rows) {
        if (r.appId == 480) { net480 = r.netUnitsSold; name480 = r.displayName; }
        if (r.appId == 490) net490 = r.netUnitsSold;
    }
    CHECK(net480 == 3, "two packages for app 480 summed (2+1=3)");
    CHECK(name480 == "git gud", "kept non-empty name across records");
    CHECK(net490 == -3, "negative net preserved");
}

static void TestDatabase() {
    std::printf("[Database]\n");
    wchar_t tmp[MAX_PATH]; ::GetTempPathW(MAX_PATH, tmp);
    std::wstring path = std::wstring(tmp) + L"sst_test_sales.db";
    ::DeleteFileW(path.c_str());
    ::DeleteFileW((path + L"-wal").c_str());
    ::DeleteFileW((path + L"-shm").c_str());

    storage::Database db;
    CHECK(db.Open(path), "Open() creates + migrates");
    CHECK(db.GetChangedDatesHighwatermark() == "0", "fresh hwm == 0");

    const std::string now = "2026-07-20T00:00:00Z";
    const std::string d1 = "2026/07/18";
    const std::string d2 = "2026/07/19";

    // Initial fill for d1.
    std::vector<storage::DailyProductRow> a = {
        {480, "git gud", 3, 4, 1}, {490, "Deluxe", -3, 0, 3}};
    CHECK(db.ReplaceDate(d1, a, now), "ReplaceDate d1");
    CHECK(db.SumLifetime() == 0, "lifetime sum 3 + (-3) = 0");
    CHECK(UnitsFor(db.QueryLifetime(), 490) == -3, "negative persisted");

    // Re-sync d1 (recalculated): must REPLACE, not accumulate.
    std::vector<storage::DailyProductRow> a2 = {{480, "git gud", 10, 10, 0}};
    CHECK(db.ReplaceDate(d1, a2, now), "ReplaceDate d1 again (recalc)");
    CHECK(db.SumForDate(d1) == 10, "date fully replaced (10, not 3+10 or 7)");
    CHECK(UnitsFor(db.QueryLifetime(), 490) == -999999,
          "app 490's d1 row removed by replacement");

    // Second date.
    std::vector<storage::DailyProductRow> b = {{480, "git gud", 5, 5, 0}};
    CHECK(db.ReplaceDate(d2, b, now), "ReplaceDate d2");
    CHECK(db.SumForRange(d1, d2) == 15, "range sum 10 + 5 = 15");
    CHECK(UnitsFor(db.QueryRange(d1, d2), 480) == 15, "range groups per app");

    // Today view (LEFT JOIN): product with no sales that date shows 0.
    auto today = db.QueryDate(d2);
    CHECK(UnitsFor(today, 480) == 5, "QueryDate shows units for d2");
    CHECK(UnitsFor(today, 490) == 0,
          "QueryDate lists product with 0 units for the date");

    // Huge unsigned high-water mark round-trips as text.
    CHECK(db.SetChangedDatesHighwatermark("18446744073709551615"),
          "set huge hwm");
    CHECK(db.GetChangedDatesHighwatermark() == "18446744073709551615",
          "huge hwm round-trips (beyond signed 64-bit)");

    // Timestamps.
    CHECK(db.SetLastSuccessfulSyncUtc(now), "set last sync utc");
    CHECK(db.GetLastSuccessfulSyncUtc() == now, "get last sync utc");

    // Backfill checkpoint (schema v2).
    CHECK(db.GetBackfillCursor().empty(), "fresh backfill cursor empty");
    CHECK(db.SetBackfillCursor("2026/07/10"), "set backfill cursor");
    CHECK(db.GetBackfillCursor() == "2026/07/10", "backfill cursor round-trips");
    CHECK(db.SetBackfillCursor(""), "clear backfill cursor");
    CHECK(db.GetBackfillCursor().empty(), "backfill cursor cleared");

    // Clear cache resets sales + hwm + cursor, keeps sync_state row.
    db.SetBackfillCursor("2026/07/11");
    CHECK(db.ClearAllSales(), "ClearAllSales");
    CHECK(db.SumLifetime() == 0 && db.QueryLifetime().empty(),
          "sales cleared");
    CHECK(db.GetChangedDatesHighwatermark() == "0", "hwm reset to 0");
    CHECK(db.GetBackfillCursor().empty(), "ClearAllSales clears backfill cursor");

    db.Close();
    ::DeleteFileW(path.c_str());
    ::DeleteFileW((path + L"-wal").c_str());
    ::DeleteFileW((path + L"-shm").c_str());
}

int main() {
    TestAggregation();
    TestDatabase();
    std::printf("\n%s (failures=%d)\n",
                g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail);
    return g_fail ? 1 : 0;
}
