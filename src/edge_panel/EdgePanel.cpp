#include "EdgePanel.h"
#include <windowsx.h>
#include <stdexcept>
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include "update/AutoUpdate.h"
namespace suite {
HWND EdgePanel::hookTarget_=nullptr;
EdgePanel::EdgePanel(Database& db,AppConfig& c):db_(db),config_(c) {
    Gdiplus::GdiplusStartupInput input;
    if(Gdiplus::GdiplusStartup(&graphicsToken_,&input,nullptr)!=Gdiplus::Ok)
        throw std::runtime_error("Cannot initialize transparent bubble graphics");
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
    if(graphicsToken_) Gdiplus::GdiplusShutdown(graphicsToken_);
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
    // UpdateLayeredWindow supplies per-pixel alpha; never mix it with SetLayeredWindowAttributes.
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
int EdgePanel::hit(POINT client) const {
    double scale=GetDpiForWindow(window_)/96.0;
    double x=client.x/scale,y=client.y/scale;
    for(int i=0;i<5;++i) {
        double progress=std::clamp(bubbles_[i].value,0.0,1.08);
        double cx=56+(config_.right?1:-1)*(1-progress)*112;
        double cy=48+i*94;
        double radius=30*progress+hover_[i].value*4;
        if(progress>.1 && (x-cx)*(x-cx)+(y-cy)*(y-cy)<=radius*radius) return i;
    }
    return -1;
}
void EdgePanel::tick() {
    POINT p{}; if(GetCursorPos(&p)) mouse(p);
    POINT local=p; ScreenToClient(window_,&local); hovered_=hit(local);
    auto now=std::chrono::steady_clock::now();
    double dt=std::min(std::chrono::duration<double>(now-last_).count(),.05); last_=now;
    revealSeconds_=open_?revealSeconds_+dt:0;
    spring_.advance(open_?1:0,dt,config_.speed);
    for(int i=0;i<5;++i) {
        bubbles_[i].advance(open_ && revealSeconds_>=i*.055?1:0,dt,config_.speed);
        hover_[i].advance(hovered_==i?1:0,dt,22);
    }
    place();
    if(IsWindowVisible(window_)) paint();
}
void EdgePanel::place() {
    const UINT dpi=GetDpiForWindow(window_);
    int width=MulDiv(112,dpi,96), height=MulDiv(480,dpi,96);
    int x=config_.right?work_.right-width:work_.left;
    SetWindowPos(window_,HWND_TOPMOST,x,work_.top+(work_.bottom-work_.top-height)/2,width,height,SWP_NOACTIVATE);
    bool hidden=!open_;
    for(const auto& bubble:bubbles_) hidden=hidden && bubble.settled(0);
    ShowWindow(window_,hidden?SW_HIDE:SW_SHOWNOACTIVATE);
}
void EdgePanel::paint() {
    using namespace Gdiplus;
    RECT rect{}; GetWindowRect(window_,&rect);
    int width=rect.right-rect.left,height=rect.bottom-rect.top;
    if(width<=0 || height<=0) return;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr; HBITMAP dib=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!dib || !dc) { if(dib) DeleteObject(dib); if(dc) DeleteDC(dc); ReleaseDC(nullptr,screen); return; }
    HGDIOBJ previous=SelectObject(dc,dib);
    {
        Bitmap bitmap(width,height,width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(pixels));
        Graphics g(&bitmap); g.Clear(Color(0,0,0,0));
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        float dpi=GetDpiForWindow(window_)/96.f; g.ScaleTransform(dpi,dpi);
        for(int i=0;i<5;++i) {
            float a=static_cast<float>(std::clamp(bubbles_[i].value,0.0,1.0));
            if(a<.005f) continue;
            float progress=static_cast<float>(std::clamp(bubbles_[i].value,0.0,1.08));
            float x=56+(config_.right?1.f:-1.f)*(1-progress)*112;
            float y=48.f+i*94.f, radius=30*progress+static_cast<float>(hover_[i].value)*4;
            // Soft shadows are separate for every orb; all gaps remain alpha-zero.
            for(int ring=9;ring>=1;--ring) {
                float r=radius+ring;
                SolidBrush shadow(Color(static_cast<BYTE>(a*(10-ring)),0,0,0));
                g.FillEllipse(&shadow,x-r,y-r+3,r*2,r*2);
            }
            SolidBrush black(Color(static_cast<BYTE>(a*255),12,12,14));
            g.FillEllipse(&black,x-radius,y-radius,radius*2,radius*2);
            Pen rim(Color(static_cast<BYTE>(a*(50+hover_[i].value*80)),130,130,140),1.f);
            g.DrawEllipse(&rim,x-radius+.5f,y-radius+.5f,radius*2-1,radius*2-1);
            auto saved=g.Save(); g.TranslateTransform(x,y); g.ScaleTransform(a,a);
            Color inkColor(static_cast<BYTE>(a*235),245,245,248);
            Pen icon(inkColor,1.7f);
            icon.SetStartCap(LineCapRound); icon.SetEndCap(LineCapRound);
            if(i==0) {
                g.DrawRectangle(&icon,-12.f,-10.f,24.f,17.f);
                g.DrawLine(&icon,-7.f,7.f,-7.f,12.f); g.DrawLine(&icon,-7.f,12.f,-1.f,7.f);
                for(int dot=-1;dot<=1;++dot) { SolidBrush ink(inkColor); g.FillEllipse(&ink,dot*7.f-1.5f,-3.f,3.f,3.f); }
            } else if(i==1) {
                g.DrawRectangle(&icon,-12.f,-11.f,24.f,22.f);
                g.DrawEllipse(&icon,3.f,-7.f,4.f,4.f);
                g.DrawLine(&icon,-10.f,8.f,-3.f,-1.f); g.DrawLine(&icon,-3.f,-1.f,4.f,7.f); g.DrawLine(&icon,4.f,7.f,9.f,2.f);
            } else if(i==2) {
                g.DrawRectangle(&icon,-11.f,-10.f,22.f,22.f);
                g.DrawLine(&icon,-11.f,-4.f,11.f,-4.f);
                g.DrawLine(&icon,-6.f,-13.f,-6.f,-7.f); g.DrawLine(&icon,6.f,-13.f,6.f,-7.f);
                for(int n=0;n<6;++n) { SolidBrush ink(inkColor); g.FillEllipse(&ink,-7.f+(n%3)*6,0.f+(n/3)*6,2.f,2.f); }
            } else if(i==3) {
                for(int n=0;n<4;++n) g.DrawRectangle(&icon,-11.f+(n%2)*13,-11.f+(n/2)*13,9.f,9.f);
            } else {
                g.DrawEllipse(&icon,-9.f,-9.f,18.f,18.f); g.DrawEllipse(&icon,-3.f,-3.f,6.f,6.f);
                for(int n=0;n<8;++n) { auto rotation=g.Save(); g.RotateTransform(n*45.f); g.DrawLine(&icon,0.f,-9.f,0.f,-13.f); g.Restore(rotation); }
            }
            g.Restore(saved);
            if(pinned_ && focus_==i) { SolidBrush focus(Color(static_cast<BYTE>(a*240),255,255,255)); g.FillEllipse(&focus,x-2,y+radius+7,4.f,4.f); }
        }
        g.Flush(FlushIntentionSync);
        POINT dest{rect.left,rect.top},source{}; SIZE size{width,height};
        BLENDFUNCTION blend{AC_SRC_OVER,0,static_cast<BYTE>(config_.opacity),AC_SRC_ALPHA};
        if(!UpdateLayeredWindow(window_,screen,&dest,&size,dc,&source,0,&blend,ULW_ALPHA))
            db_.log("Per-pixel bubble presentation failed",GetLastError());
    }
    SelectObject(dc,previous); DeleteObject(dib); DeleteDC(dc); ReleaseDC(nullptr,screen);
}
void EdgePanel::activate(int index) {
    if(index==4) PostMessageW(window_,WM_SUITE_SETTINGS,0,0);
    else if(onTool) onTool(index);
}
LRESULT CALLBACK EdgePanel::proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<EdgePanel*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE) {
        self=static_cast<EdgePanel*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(!self) return DefWindowProcW(h,m,w,l);
    switch(m) {
    case WM_SUITE_UPDATE: if(self->onUpdate) self->onUpdate(); return 0;
    case WM_APP+3: self->show(); return 0;
    case WM_SUITE_SETTINGS: if(self->onSettings) self->onSettings(); return 0;
    case WM_SUITE_MOUSE: self->mouse(POINT{static_cast<LONG>(w),static_cast<LONG>(l)}); return 0;
    case WM_TIMER: self->tick(); return 0;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(h,&ps); EndPaint(h,&ps); self->paint(); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_DISPLAYCHANGE: self->reconfigure(); return 0;
    case WM_DPICHANGED: self->place(); return 0;
    case WM_RBUTTONUP: if(self->onSettings) self->onSettings(); return 0;
    case WM_LBUTTONDOWN: self->pinned_=true; SetForegroundWindow(h); SetFocus(h); return 0;
    case WM_LBUTTONUP: {
        int i=self->hit(POINT{GET_X_LPARAM(l),GET_Y_LPARAM(l)});
        if(i>=0) self->activate(i); return 0;
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
