#pragma once
#include <windows.h>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
namespace suite {
constexpr UINT WM_SUITE_UPDATE = WM_APP+30;
std::filesystem::path userDirectory();
std::filesystem::path executablePath();
bool bootstrapUserInstall(); // true means a managed copy was launched; caller must exit.
bool handleUpdateCommand();
bool preference(const wchar_t* name, bool fallback=true);
void setPreference(const wchar_t* name,bool enabled);
void setAutoRun(bool enabled);
class AutoUpdate {
public:
    struct Result { std::string message; bool ready=false, manual=false; };
    explicit AutoUpdate(HWND owner);
    ~AutoUpdate();
    void check(bool manual=true);
    Result result();
    bool apply();
private:
    HWND owner_;
    std::mutex mutex_;
    std::condition_variable_any wake_;
    bool requested_=false, manual_=false;
    Result result_;
    std::jthread worker_;
    void run(std::stop_token stop);
    Result fetch(bool manual);
};
}
