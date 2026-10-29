#include "SettingsWindow.h"
#include <string>
#include "update/AutoUpdate.h"
namespace suite {
void SettingsWindow::show() {
    if(!window_) {
        WNDCLASSW wc{}; wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpszClassName=L"DesktopSuite.Settings"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1); RegisterClassW(&wc);
        window_=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"Desktop Suite — Settings",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
            CW_USEDEFAULT,CW_USEDEFAULT,440,450,nullptr,nullptr,wc.hInstance,this);
        if(!window_) { db_.log("Cannot create settings window",GetLastError()); return; }
        controls();
    }
    SendMessageW(side_,CB_SETCURSEL,config_.right?1:0,0);
    SetWindowTextW(threshold_,std::to_wstring(config_.threshold).c_str());
    SendMessageW(speed_,CB_SETCURSEL,config_.speed<12?0:config_.speed<20?1:2,0);
    SetWindowTextW(opacity_,std::to_wstring(config_.opacity).c_str());
    CheckDlgButton(window_,14,preference(L"AutoRun")?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(window_,15,preference(L"AutoUpdate")?BST_CHECKED:BST_UNCHECKED);
    ShowWindow(window_,SW_SHOW); SetForegroundWindow(window_);
}
void SettingsWindow::controls() {
    auto make=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id) {
        HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE); return c;
    };
    make(L"STATIC",L"Screen edge",0,20,20,150,22,0);
    side_=make(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,180,20,180,150,10);
    SendMessageW(side_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Left"));
    SendMessageW(side_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Right"));
    make(L"STATIC",L"Trigger pixels (1–32)",0,20,65,155,22,0);
    threshold_=make(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,180,65,180,24,11);
    make(L"STATIC",L"Spring speed",0,20,110,150,22,0);
    speed_=make(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,180,110,180,150,12);
    for(auto label:{L"Slow",L"Medium",L"Fast"}) SendMessageW(speed_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    make(L"STATIC",L"Opacity (80–255)",0,20,155,150,22,0);
    opacity_=make(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,180,155,180,24,13);
    make(L"BUTTON",L"Run automatically when I sign in",BS_AUTOCHECKBOX|WS_TABSTOP,20,205,370,26,14);
    make(L"BUTTON",L"Automatically install stable GitHub updates",BS_AUTOCHECKBOX|WS_TABSTOP,20,240,390,26,15);
    make(L"BUTTON",L"Check for updates now",WS_TABSTOP,20,290,180,32,16);
    make(L"BUTTON",L"Apply",BS_DEFPUSHBUTTON|WS_TABSTOP,220,290,180,32,1);
    make(L"STATIC",L"Updates are checked on launch and hourly.\nVerified updates restart the app automatically. v" SUITE_VERSION_W,0,20,337,380,45,0);
}
void SettingsWindow::apply() {
    BOOL a=FALSE,b=FALSE;
    UINT t=GetDlgItemInt(window_,11,&a,FALSE),o=GetDlgItemInt(window_,13,&b,FALSE);
    if(!a || !b || t<1 || t>32 || o<80 || o>255) {
        MessageBoxW(window_,L"Trigger must be 1–32 and opacity 80–255.",L"Invalid settings",MB_ICONWARNING); return;
    }
    auto next=config_; next.right=SendMessageW(side_,CB_GETCURSEL,0,0)==1;
    next.threshold=t; next.opacity=o;
    int speeds[]={8,15,24}; auto i=SendMessageW(speed_,CB_GETCURSEL,0,0);
    next.speed=speeds[i>=0 && i<3?i:1];
    try {
        setAutoRun(IsDlgButtonChecked(window_,14)==BST_CHECKED);
        setPreference(L"AutoUpdate",IsDlgButtonChecked(window_,15)==BST_CHECKED);
        db_.save(next); config_=next; if(onApply) onApply();
    }
    catch(const std::exception& e) { db_.log(e.what()); MessageBoxW(window_,L"Could not save settings.",L"SQLite error",MB_ICONERROR); }
}
LRESULT CALLBACK SettingsWindow::proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto* s=reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE) {
        s=static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
    }
    if(s && m==WM_COMMAND && LOWORD(w)==16) { if(s->onCheckUpdate) s->onCheckUpdate(); return 0; }
    if(s && m==WM_COMMAND && LOWORD(w)==1) { s->apply(); return 0; }
    if(m==WM_CLOSE) { ShowWindow(h,SW_HIDE); return 0; }
    return DefWindowProcW(h,m,w,l);
}
}
