#include "TrayIcon.h"
#include <stdexcept>
namespace suite {
TrayIcon::TrayIcon() {
    WNDCLASSW wc{}; wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"DesktopSuite.Tray";
    RegisterClassW(&wc);
    // A hidden top-level window receives TaskbarCreated broadcasts (not HWND_MESSAGE).
    window_=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,this);
    if(!window_) throw std::runtime_error("Cannot create tray message window");
    restart_=RegisterWindowMessageW(L"TaskbarCreated");
    data_.cbSize=sizeof(data_); data_.hWnd=window_; data_.uID=1;
    data_.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
    data_.uCallbackMessage=WM_APP+10; data_.hIcon=LoadIcon(nullptr,IDI_APPLICATION);
    lstrcpynW(data_.szTip,L"Desktop Widget Suite",128);
    add();
}
void TrayIcon::add() { Shell_NotifyIconW(NIM_ADD,&data_); }
TrayIcon::~TrayIcon() { Shell_NotifyIconW(NIM_DELETE,&data_); DestroyWindow(window_); }
void TrayIcon::menu() {
    HMENU m=CreatePopupMenu();
    AppendMenuW(m,MF_STRING,1,L"Show / نمایش");
    AppendMenuW(m,MF_STRING,2,L"Settings / تنظیمات");
    AppendMenuW(m,MF_STRING,4,L"Check for updates / بررسی آپدیت");
    AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    AppendMenuW(m,MF_STRING,3,L"Quit / خروج");
    POINT p; GetCursorPos(&p); SetForegroundWindow(window_);
    int cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window_,nullptr);
    DestroyMenu(m); PostMessageW(window_,WM_NULL,0,0);
    if(cmd && onCommand) onCommand(cmd);
}
LRESULT CALLBACK TrayIcon::proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto* s=reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE) {
        s=static_cast<TrayIcon*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
    }
    if(s) {
        if(s->restart_ && m==s->restart_) { s->add(); return 0; }
        if(m==WM_APP+10) {
            if(l==WM_RBUTTONUP || l==WM_CONTEXTMENU) s->menu();
            if(l==WM_LBUTTONDBLCLK && s->onCommand) s->onCommand(1);
            return 0;
        }
    }
    return DefWindowProcW(h,m,w,l);
}
}
