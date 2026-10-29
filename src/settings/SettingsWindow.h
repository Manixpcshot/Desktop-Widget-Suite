#pragma once
#include <windows.h>
#include "common/Database.h"
#include <functional>
namespace suite {
class SettingsWindow {
public:
    SettingsWindow(Database& db,AppConfig& config):db_(db),config_(config) {}
    ~SettingsWindow() { if(window_) DestroyWindow(window_); }
    void show();
    HWND hwnd() const { return window_; }
    std::function<void()> onApply;
private:
    Database& db_; AppConfig& config_;
    HWND window_=nullptr,side_=nullptr,threshold_=nullptr,speed_=nullptr,opacity_=nullptr;
    void controls();
    void apply();
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
};
}
