#include "Database.h"
#include <algorithm>
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
namespace suite {
namespace {
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db));
    return Statement(raw, sqlite3_finalize);
}
}
Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string error = db_ ? sqlite3_errmsg(db_) : "SQLite allocation failed";
        sqlite3_close(db_); db_ = nullptr;
        throw std::runtime_error(error);
    }
    try {
        sqlite3_busy_timeout(db_, 2000);
        exec("PRAGMA journal_mode=WAL;");
        exec("CREATE TABLE IF NOT EXISTS panel (id INTEGER PRIMARY KEY CHECK(id=1), right_edge INTEGER NOT NULL, threshold INTEGER NOT NULL, speed INTEGER NOT NULL, opacity INTEGER NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, anchored INTEGER NOT NULL);");
        exec("CREATE TABLE IF NOT EXISTS errors (time TEXT DEFAULT CURRENT_TIMESTAMP, code INTEGER, message TEXT);");
    } catch (...) { sqlite3_close(db_); db_ = nullptr; throw; }
}
Database::~Database() { sqlite3_close(db_); }
void Database::exec(const char* sql) {
    if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_));
}
AppConfig Database::load() {
    auto s = prepare(db_, "SELECT right_edge,threshold,speed,opacity,x,y,anchored FROM panel WHERE id=1");
    AppConfig c;
    const int result = sqlite3_step(s.get());
    if (result == SQLITE_ROW) {
        c.right = sqlite3_column_int(s.get(), 0) != 0;
        c.threshold = std::clamp(sqlite3_column_int(s.get(), 1), 1, 32);
        c.speed = std::clamp(sqlite3_column_int(s.get(), 2), 5, 30);
        c.opacity = std::clamp(sqlite3_column_int(s.get(), 3), 80, 255);
        c.anchorX = sqlite3_column_int(s.get(), 4); c.anchorY = sqlite3_column_int(s.get(), 5);
        c.hasAnchor = sqlite3_column_int(s.get(), 6) != 0;
    } else if (result != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
    return c;
}
void Database::save(const AppConfig& c) {
    auto s = prepare(db_, "INSERT OR REPLACE INTO panel VALUES(1,?,?,?,?,?,?,?)");
    int values[] = {c.right, c.threshold, c.speed, c.opacity, c.anchorX, c.anchorY, c.hasAnchor};
    for (int i = 0; i < 7; ++i) sqlite3_bind_int(s.get(), i + 1, values[i]);
    if (sqlite3_step(s.get()) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
}
void Database::log(const std::string& message, long code) noexcept {
    try {
        auto s = prepare(db_, "INSERT INTO errors(code,message) VALUES(?,?)");
        sqlite3_bind_int64(s.get(), 1, code);
        sqlite3_bind_text(s.get(), 2, message.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(s.get());
    } catch (...) { /* Logging must never terminate the message loop. */ }
}
}
