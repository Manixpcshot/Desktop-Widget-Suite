#include "common/Database.h"
#include <filesystem>
#include <stdexcept>
#include <iostream>
int main() {
    const auto path = std::filesystem::temp_directory_path() / "desktop-suite-test.db";
    std::filesystem::remove(path);
    {
        suite::Database db(path.string());
        if (!db.load().right) throw std::runtime_error("defaults");
        auto c=db.load(); c.right=false; c.anchorX=-1920; c.anchorY=200;
        c.hasAnchor=true; c.opacity=180; db.save(c); db.log("test", 42);
    }
    {
        suite::Database db(path.string()); auto c=db.load();
        if(c.right || c.anchorX!=-1920 || c.anchorY!=200 || !c.hasAnchor || c.opacity!=180)
            throw std::runtime_error("persistence");
    }
    std::filesystem::remove(path);
    std::cout << "SQLite round-trip passed\n";
}
