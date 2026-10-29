#pragma once
#include "settings/AppConfig.h"
#include <string>
struct sqlite3;
namespace suite {
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    AppConfig load();
    std::string loadNote();
    void saveNote(const std::string& text);
    void save(const AppConfig& config);
    void log(const std::string& message, long code = 0) noexcept;
private:
    sqlite3* db_ = nullptr;
    void exec(const char* sql);
};
}
