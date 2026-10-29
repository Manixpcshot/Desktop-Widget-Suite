#include "EdgePanel.h"
#include <windowsx.h>
#include <stdexcept>
namespace suite {
HWND EdgePanel::hookTarget_=nullptr;
EdgePanel::EdgePanel(Database& db,AppConfig& c):db_(db),config_(c) {
    WNDCLASSW wc{}; wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"DesktopSuite.Edge"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    RegisterClassW(&wc);
    window_=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"Desktop Suite",WS_POPUP,
        0,0,104,480,nullptr,nullptr,wc.hInstance,this);
    if(!window_) throw std::runtime_error("Cannot create edge panel");
    reconfigure(); hookTarget_=window_;
    hook_=SetWindowsHookExW(WH_MOUSE_LL,hook,GetModuleHandleW(nullptr),0);
    if(!hook_) db_.log("Mouse hook unavailable; timer-based cursor detection enabled",GetLastError());
    last_=std::chrono::steady_clock::now();
    if(!SetTimer(window_,1,16,nullptr)) {
        if(hook_) UnhookWindowsHookEx(hook_);
        hookTarget_=nullptr; DestroyWindow(window_); window_=nullptr;
        throw std::runtime_error("Cannot start animation timer");
    }
}
EdgePanel::~EdgePanel() {
    if(hook_) UnhookWindowsHookEx(hook_);
    hookTarget_=nullptr;
    if(window_) DestroyWindow(window_);
}
void EdgePanel::remember() {
    config_.anchorX=(work_.left+work_.right)/2; config_.anchorY=(work_.top+work_.bottom)/2;
    config_.hasAnchor=true;
    try { db_.save(config_); } catch(const std::exception& e) { db_.log(e.what()); }
}
void EdgePanel::reconfigure() {
    POINT p{config_.anchorX,config_.anchorY};
    HMONITOR monitor=MonitorFromPoint(p,config_.hasAnchor?MONITOR_DEFAULTTONEAREST:MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(monitor,&mi); work_=mi.rcWork;
    SetLayeredWindowAttributes(window_,0,static_cast<BYTE>(config_.opacity),LWA_ALPHA);
    place();
}
void EdgePanel::show() {
    open_=true; pinned_=true; ShowWindow(window_,SW_SHOW); SetForegroundWindow(window_);
}
LRESULT CALLBACK EdgePanel::hook(int code,WPARAM w,LPARAM l) {
    if(code==HC_ACTION && w==WM_MOUSEMOVE && hookTarget_) {
        auto* m=reinterpret_cast<MSLLHOOKSTRUCT*>(l);
        // No database, drawing, or blocking work in the low-level hook.
        PostMessageW(hookTarget_,WM_SUITE_MOUSE,static_cast<WPARAM>(m->pt.x),static_cast<LPARAM>(m->pt.y));
    }
    return CallNextHookEx(nullptr,code,w,l);
}
void EdgePanel::mouse(POINT p) {
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&mi);
    Rect r{mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right,mi.rcMonitor.bottom};
    if(atEdge(p.x,p.y,r,config_.right,config_.threshold)) {
        if(!open_ && spring_.settled(0)) { work_=mi.rcWork; remember(); }
        open_=true; lastInside_=GetTickCount();
    }
    RECT bounds{}; GetWindowRect(window_,&bounds);
    if(IsWindowVisible(window_) && PtInRect(&bounds,p)) lastInside_=GetTickCount();
    else if(!pinned_ && GetTickCount()-lastInside_>450) open_=false;
}
void EdgePanel::tick() {
    POINT p; if(GetCursorPos(&p)) mouse(p); // Fallback if a hook is removed by Windows.
    auto now=std::chrono::steady_clock::now();
    spring_.advance(open_?1:0,std::chrono::duration<double>(now-last_).count(),config_.speed); last_=now;
    place(); if(IsWindowVisible(window_)) InvalidateRect(window_,nullptr,FALSE);
}
void EdgePanel::place() {
    const UINT dpi=GetDpiForWindow(window_);
    int width=MulDiv(104,dpi,96), height=std::min(MulDiv(480,dpi,96),static_cast<int>(work_.bottom-work_.top));
    const double progress=std::clamp(spring_.value,0.0,1.04);
    int x=config_.right?work_.right-static_cast<int>(width*progress):work_.left-width+static_cast<int>(width*progress);
    SetWindowPos(window_,HWND_TOPMOST,x,work_.top+(work_.bottom-work_.top-height)/2,width,height,SWP_NOACTIVATE);
    ShowWindow(window_,spring_.settled(0)&&!open_?SW_HIDE:SW_SHOWNOACTIVATE);
}
void EdgePanel::paint() {
    PAINTSTRUCT ps; BeginPaint(window_,&ps);
    auto hr=renderer_.begin(window_);
    if(SUCCEEDED(hr)) {
        const wchar_t* labels[]={L"AI Chat",L"Photo",L"Calendar",L"Widgets",L"Settings"};
        const UINT32 colors[]={0x5389f5,0x37b892,0xe9b752,0xee747d,0x9d8aeb};
        for(int i=0;i<5;++i) {
            float a=static_cast<float>(stagger(spring_.value,i));
            float y=48.f+i*94.f;
            auto color=D2D1::ColorF(colors[i],a);
            renderer_.circle(52,y,30*a,color);
            renderer_.text(labels[i],D2D1::RectF(4,y-20,100,y+20),D2D1::ColorF(0xffffff,a));
            if(pinned_ && focus_==i) renderer_.circle(52,y+36,2,D2D1::ColorF(0xffffff));
        }
        hr=renderer_.end();
    }
    if(FAILED(hr)) db_.log("Direct2D render failure",hr);
    EndPaint(window_,&ps);
}
void EdgePanel::activate(int index) {
    if(index==4) PostMessageW(window_,WM_SUITE_SETTINGS,0,0);
    else MessageBoxW(window_,L"This tool is planned for a later milestone.\nMilestone 1 implements the native edge panel.",L"Desktop Suite",MB_OK|MB_ICONINFORMATION);
}
LRESULT CALLBACK EdgePanel::proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<EdgePanel*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE) {
        self=static_cast<EdgePanel*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(!self) return DefWindowProcW(h,m,w,l);
    switch(m) {
    case WM_SUITE_SETTINGS: if(self->onSettings) self->onSettings(); return 0;
    case WM_SUITE_MOUSE: self->mouse(POINT{static_cast<LONG>(w),static_cast<LONG>(l)}); return 0;
    case WM_TIMER: self->tick(); return 0;
    case WM_PAINT: self->paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DISPLAYCHANGE: self->reconfigure(); return 0;
    case WM_DPICHANGED: self->renderer_.reset(); self->place(); return 0;
    case WM_LBUTTONDOWN: self->pinned_=true; SetForegroundWindow(h); SetFocus(h); return 0;
    case WM_LBUTTONUP: {
        int y=MulDiv(GET_Y_LPARAM(l),96,GetDpiForWindow(h));
        int i=(y-1)/94; if(i>=0 && i<5) self->activate(i); return 0;
    }
    case WM_KEYDOWN:
        if(w==VK_ESCAPE) { self->pinned_=false; self->open_=false; }
        if(w==VK_TAB || w==VK_DOWN) self->focus_=(self->focus_+1)%5;
        if(w==VK_UP) self->focus_=(self->focus_+4)%5;
        if(w==VK_RETURN || w==VK_SPACE) self->activate(self->focus_);
        return 0;
    case WM_ACTIVATE: if(LOWORD(w)==WA_INACTIVE) self->pinned_=false; return 0;
    case WM_CLOSE: self->pinned_=false; self->open_=false; return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
}
