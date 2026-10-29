#pragma once
#include "common/Animation.h"
#include "common/D2DRenderer.h"
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
    void reconfigure();
private:
    HWND window_ = nullptr;
    HHOOK hook_ = nullptr;
    Database& db_;
    AppConfig& config_;
    D2DRenderer renderer_;
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
