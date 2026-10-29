#pragma once
#include "common/Animation.h"
#include <windows.h>
#include "common/Database.h"
#include <chrono>
#include <functional>
namespace suite {
constexpr UINT WM_SUITE_MOUSE = WM_APP+1;
constexpr UINT WM_SUITE_SETTINGS = WM_APP+2;
class EdgePanel {
public:
    EdgePanel(Database& db, AppConfig& config);
    ~EdgePanel();
    HWND hwnd() const { return window_; }
    void show();
    std::function<void()> onSettings;
    std::function<void()> onUpdate;
    std::function<void(int)> onTool;
    void reconfigure();
private:
    HWND window_ = nullptr;
    HHOOK hook_ = nullptr;
    Database& db_;
    AppConfig& config_;
    ULONG_PTR graphicsToken_=0;
    Spring bubbles_[5], hover_[5];
    double revealSeconds_=0;
    int hovered_=-1;
    int hit(POINT client) const;
    Spring spring_;
    bool open_=false, pinned_=false;
    int focus_=0;
    RECT work_{};
    DWORD lastInside_=0;
    std::chrono::steady_clock::time_point last_;
    void mouse(POINT p);
    void tick();
    void place();
    void paint();
    void activate(int index);
    void remember();
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK hook(int, WPARAM, LPARAM);
    static HWND hookTarget_;
};
}
