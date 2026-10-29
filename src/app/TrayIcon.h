#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>
namespace suite {
class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();
    std::function<void(int)> onCommand;
private:
    HWND window_=nullptr;
    NOTIFYICONDATAW data_{};
    UINT restart_=0;
    void add();
    void menu();
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
};
}
