#include "storage/Database.h"

#include "platform/StringUtil.h"
#include "storage/Migrations.h"
#include "sqlite3.h"

namespace storage {

namespace {

bool ExecSimple(sqlite3* db, const char* sql) {
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void BindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(),
                      static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

std::string ColumnText(sqlite3_stmt* stmt, int col) {
    const auto* text = sqlite3_column_text(stmt, col);
    if (!text) {
        return {};
    }
    const int bytes = sqlite3_column_bytes(stmt, col);
    return std::string(reinterpret_cast<const char*>(text),
                       static_cast<size_t>(bytes));
}

// Runs a query that returns (app_id, display_name, units) rows.
std::vector<ProductUnits> RunProductQuery(sqlite3_stmt* stmt) {
    std::vector<ProductUnits> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ProductUnits row;
        row.appId = sqlite3_column_int64(stmt, 0);
        row.displayName = platform::Utf8ToWide(ColumnText(stmt, 1));
        row.units = sqlite3_column_int64(stmt, 2);
        rows.push_back(std::move(row));
    }
    return rows;
}

std::int64_t RunScalarSum(sqlite3* db, const char* sql,
                          const std::string* a = nullptr,
                          const std::string* b = nullptr) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    int idx = 1;
    if (a) BindText(stmt, idx++, *a);
    if (b) BindText(stmt, idx++, *b);
    std::int64_t total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return total;
}

// Sets a single sync_state text column.
bool SetSyncStateColumn(sqlite3* db, const char* column,
                        const std::string& value) {
    const std::string sql =
        std::string("UPDATE sync_state SET ") + column + " = ? WHERE id = 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    BindText(stmt, 1, value);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

}  // namespace

Database::~Database() {
    Close();
}

bool Database::Open(const std::wstring& path) {
    Close();
    const std::string utf8Path = platform::WideToUtf8(path);
    if (sqlite3_open_v2(utf8Path.c_str(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        nullptr) != SQLITE_OK) {
        Close();
        return false;
    }
    ExecSimple(db_, "PRAGMA foreign_keys = ON;");
    ExecSimple(db_, "PRAGMA journal_mode = WAL;");
    ExecSimple(db_, "PRAGMA synchronous = NORMAL;");
    ExecSimple(db_, "PRAGMA busy_timeout = 5000;");

    if (!ApplyMigrations(db_)) {
        Close();
        return false;
    }
    return true;
}

void Database::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::string Database::GetChangedDatesHighwatermark() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_, "SELECT changed_dates_highwatermark FROM sync_state WHERE id=1;",
            -1, &stmt, nullptr) != SQLITE_OK) {
        return "0";
    }
    std::string value = "0";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = ColumnText(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return value.empty() ? "0" : value;
}

bool Database::SetChangedDatesHighwatermark(const std::string& highwatermark) {
    return SetSyncStateColumn(db_, "changed_dates_highwatermark", highwatermark);
}

bool Database::SetLastSuccessfulCheckUtc(const std::string& iso8601) {
    return SetSyncStateColumn(db_, "last_successful_check_utc", iso8601);
}

bool Database::SetLastSuccessfulSyncUtc(const std::string& iso8601) {
    return SetSyncStateColumn(db_, "last_successful_sync_utc", iso8601);
}

std::string Database::GetLastSuccessfulSyncUtc() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_, "SELECT last_successful_sync_utc FROM sync_state WHERE id=1;",
            -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    std::string value;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = ColumnText(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return value;
}

bool Database::SetLastError(const std::string& message) {
    return SetSyncStateColumn(db_, "last_error", message);
}

std::string Database::GetLastError() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT last_error FROM sync_state WHERE id=1;",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    std::string value;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = ColumnText(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return value;
}

std::string Database::GetBackfillCursor() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_, "SELECT backfill_cursor FROM sync_state WHERE id=1;", -1,
            &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    std::string value;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = ColumnText(stmt, 0);  // NULL -> empty
    }
    sqlite3_finalize(stmt);
    return value;
}

bool Database::SetBackfillCursor(const std::string& date) {
    if (date.empty()) {
        return ExecSimple(
            db_, "UPDATE sync_state SET backfill_cursor=NULL WHERE id=1;");
    }
    return SetSyncStateColumn(db_, "backfill_cursor", date);
}

bool Database::ReplaceDate(const std::string& date,
                           const std::vector<DailyProductRow>& rows,
                           const std::string& nowUtc) {
    if (!db_) {
        return false;
    }
    if (!ExecSimple(db_, "BEGIN IMMEDIATE;")) {
        return false;
    }

    auto fail = [&]() {
        ExecSimple(db_, "ROLLBACK;");
        return false;
    };

    // 1) Remove the date's existing rows.
    {
        sqlite3_stmt* del = nullptr;
        if (sqlite3_prepare_v2(
                db_, "DELETE FROM daily_product_sales WHERE financial_date=?;",
                -1, &del, nullptr) != SQLITE_OK) {
            return fail();
        }
        BindText(del, 1, date);
        const bool ok = sqlite3_step(del) == SQLITE_DONE;
        sqlite3_finalize(del);
        if (!ok) {
            return fail();
        }
    }

    // 2) Upsert products + insert new daily rows (reuse prepared statements).
    sqlite3_stmt* upsertProduct = nullptr;
    sqlite3_stmt* insertDaily = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "INSERT INTO products (app_id, display_name, last_seen_utc) "
            "VALUES (?,?,?) "
            "ON CONFLICT(app_id) DO UPDATE SET "
            "display_name=excluded.display_name, "
            "last_seen_utc=excluded.last_seen_utc;",
            -1, &upsertProduct, nullptr) != SQLITE_OK ||
        sqlite3_prepare_v2(
            db_,
            "INSERT INTO daily_product_sales "
            "(financial_date, app_id, gross_units_sold, gross_units_returned, "
            "net_units_sold) VALUES (?,?,?,?,?);",
            -1, &insertDaily, nullptr) != SQLITE_OK) {
        if (upsertProduct) sqlite3_finalize(upsertProduct);
        if (insertDaily) sqlite3_finalize(insertDaily);
        return fail();
    }

    bool ok = true;
    for (const DailyProductRow& row : rows) {
        const std::string name =
            row.displayName.empty() ? ("App " + std::to_string(row.appId))
                                     : row.displayName;

        sqlite3_reset(upsertProduct);
        sqlite3_bind_int64(upsertProduct, 1, row.appId);
        BindText(upsertProduct, 2, name);
        BindText(upsertProduct, 3, nowUtc);
        if (sqlite3_step(upsertProduct) != SQLITE_DONE) {
            ok = false;
            break;
        }

        sqlite3_reset(insertDaily);
        BindText(insertDaily, 1, date);
        sqlite3_bind_int64(insertDaily, 2, row.appId);
        sqlite3_bind_int64(insertDaily, 3, row.grossUnitsSold);
        sqlite3_bind_int64(insertDaily, 4, row.grossUnitsReturned);
        sqlite3_bind_int64(insertDaily, 5, row.netUnitsSold);
        if (sqlite3_step(insertDaily) != SQLITE_DONE) {
            ok = false;
            break;
        }
    }

    sqlite3_finalize(upsertProduct);
    sqlite3_finalize(insertDaily);

    if (!ok) {
        return fail();
    }
    return ExecSimple(db_, "COMMIT;");
}

bool Database::ClearAllSales() {
    if (!db_) {
        return false;
    }
    if (!ExecSimple(db_, "BEGIN IMMEDIATE;")) {
        return false;
    }
    if (!ExecSimple(db_, "DELETE FROM daily_product_sales;") ||
        !ExecSimple(db_, "DELETE FROM products;") ||
        !ExecSimple(db_,
                    "UPDATE sync_state SET changed_dates_highwatermark='0', "
                    "last_successful_sync_utc=NULL, backfill_cursor=NULL "
                    "WHERE id=1;")) {
        ExecSimple(db_, "ROLLBACK;");
        return false;
    }
    return ExecSimple(db_, "COMMIT;");
}

std::vector<ProductUnits> Database::QueryDate(const std::string& date) {
    // Today view: list every product, LEFT JOIN this date's sales.
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT p.app_id, p.display_name, "
            "COALESCE(s.net_units_sold, 0) AS units "
            "FROM products p "
            "LEFT JOIN daily_product_sales s "
            "  ON s.app_id = p.app_id AND s.financial_date = ? "
            "ORDER BY units DESC, p.display_name COLLATE NOCASE;",
            -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    BindText(stmt, 1, date);
    auto rows = RunProductQuery(stmt);
    sqlite3_finalize(stmt);
    return rows;
}

std::vector<ProductUnits> Database::QueryRange(const std::string& startDate,
                                               const std::string& endDate) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT p.app_id, p.display_name, SUM(s.net_units_sold) AS units "
            "FROM daily_product_sales s "
            "JOIN products p ON p.app_id = s.app_id "
            "WHERE s.financial_date BETWEEN ? AND ? "
            "GROUP BY p.app_id, p.display_name "
            "ORDER BY units DESC, p.display_name COLLATE NOCASE;",
            -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    BindText(stmt, 1, startDate);
    BindText(stmt, 2, endDate);
    auto rows = RunProductQuery(stmt);
    sqlite3_finalize(stmt);
    return rows;
}

std::vector<ProductUnits> Database::QueryLifetime() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT p.app_id, p.display_name, SUM(s.net_units_sold) AS units "
            "FROM daily_product_sales s "
            "JOIN products p ON p.app_id = s.app_id "
            "GROUP BY p.app_id, p.display_name "
            "ORDER BY units DESC, p.display_name COLLATE NOCASE;",
            -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    auto rows = RunProductQuery(stmt);
    sqlite3_finalize(stmt);
    return rows;
}

std::int64_t Database::SumForDate(const std::string& date) {
    return RunScalarSum(
        db_,
        "SELECT COALESCE(SUM(net_units_sold),0) FROM daily_product_sales "
        "WHERE financial_date = ?;",
        &date);
}

std::int64_t Database::SumForRange(const std::string& startDate,
                                   const std::string& endDate) {
    return RunScalarSum(
        db_,
        "SELECT COALESCE(SUM(net_units_sold),0) FROM daily_product_sales "
        "WHERE financial_date BETWEEN ? AND ?;",
        &startDate, &endDate);
}

std::int64_t Database::SumLifetime() {
    return RunScalarSum(
        db_,
        "SELECT COALESCE(SUM(net_units_sold),0) FROM daily_product_sales;");
}

}  // namespace storage
