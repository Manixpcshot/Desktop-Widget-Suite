#include "NativeTools.h"
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <fstream>
#include "update/AutoUpdate.h"
namespace suite {
NativeTools::NativeTools(Database& db):db_(db) {
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_DATE_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSW wc{}; wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"DesktopSuite.Tools"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1); RegisterClassW(&wc);
}
NativeTools::~NativeTools() {
    if(noteDirty_) saveNote();
    if(notes_) DestroyWindow(notes_); if(calendar_) DestroyWindow(calendar_); if(photo_) DestroyWindow(photo_);
}
HWND NativeTools::create(const wchar_t* title,int width,int height) {
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,L"DesktopSuite.Tools",title,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),this);
    if(!window) db_.log("Cannot open native tool window",GetLastError());
    return window;
}
void NativeTools::saveNote() {
    if(!edit_ || !noteDirty_) return;
    int length=GetWindowTextLengthW(edit_);
    std::wstring value(static_cast<size_t>(length)+1,L'\0'); GetWindowTextW(edit_,value.data(),length+1); value.resize(length);
    int size=WideCharToMultiByte(CP_UTF8,0,value.data(),length,nullptr,0,nullptr,nullptr);
    std::string text(size,'\0'); WideCharToMultiByte(CP_UTF8,0,value.data(),length,text.data(),size,nullptr,nullptr);
    try { db_.saveNote(text); noteDirty_=false; } catch(const std::exception& e) { db_.log(e.what()); MessageBoxW(notes_,L"Could not save note.",L"SQLite error",MB_ICONERROR); }
}
void NativeTools::clock() {
    SYSTEMTIME t{}; GetLocalTime(&t); wchar_t label[80];
    swprintf_s(label,L"%04u-%02u-%02u   %02u:%02u:%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    SetWindowTextW(clock_,label);
}
void NativeTools::show(int index,HWND owner) {
    if(index==0) {
        MessageBoxW(owner,L"AI Chat is not implemented in this build. No API key is stored or sent.\nCalendar, quick notes and the image viewer are now available.",L"AI Chat — not available yet",MB_OK|MB_ICONINFORMATION); return;
    }
    HWND window=nullptr;
    if(index==3) {
        if(!notes_) {
            notes_=create(L"Quick Notes — saved automatically",440,380); if(!notes_) return;
            edit_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,0,0,420,340,notes_,nullptr,GetModuleHandleW(nullptr),nullptr);
            SendMessageW(edit_,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
            SendMessageW(edit_,EM_SETLIMITTEXT,1024*1024,0);
            auto text=db_.loadNote();
            int size=MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
            std::wstring value(size,L'\0'); MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),value.data(),size);
            SetWindowTextW(edit_,value.c_str()); SetTimer(notes_,1,1500,nullptr);
        }
        window=notes_;
    } else if(index==2) {
        if(!calendar_) {
            calendar_=create(L"Calendar & Clock — local time",340,310); if(!calendar_) return;
            clock_=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_CENTER,10,12,300,28,calendar_,nullptr,GetModuleHandleW(nullptr),nullptr);
            SendMessageW(clock_,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
            CreateWindowExW(0,MONTHCAL_CLASSW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|MCS_NOTODAYCIRCLE,20,50,280,200,calendar_,nullptr,GetModuleHandleW(nullptr),nullptr);
            clock(); SetTimer(calendar_,1,1000,nullptr);
        }
        window=calendar_;
    } else if(index==1) {
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) return;
        COMDLG_FILTERSPEC filters[]={{L"Images (JPG, PNG, BMP)",L"*.jpg;*.jpeg;*.png;*.bmp"}};
        dialog->SetFileTypes(1,filters); dialog->SetTitle(L"Open image — native viewer (editing is not implemented yet)");
        if(FAILED(dialog->Show(owner))) return;
        Microsoft::WRL::ComPtr<IShellItem> item;
        if(FAILED(dialog->GetResult(&item))) return;
        PWSTR path=nullptr; if(FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) return;
        auto image=std::make_unique<Gdiplus::Image>(path); CoTaskMemFree(path);
        if(image->GetLastStatus()!=Gdiplus::Ok || !image->GetWidth() || !image->GetHeight()) {
            MessageBoxW(owner,L"Cannot decode this image.",L"Image viewer",MB_ICONERROR); return;
        }
        image_=std::move(image);
        if(!photo_) photo_=create(L"Image Viewer — fit to window",800,600);
        window=photo_; if(window) InvalidateRect(window,nullptr,TRUE);
    }
    if(window) { ShowWindow(window,SW_SHOW); SetForegroundWindow(window); }
}
LRESULT CALLBACK NativeTools::proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<NativeTools*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE) { self=static_cast<NativeTools*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
    if(!self) return DefWindowProcW(h,m,w,l);
    if(m==WM_COMMAND && reinterpret_cast<HWND>(l)==self->edit_ && HIWORD(w)==EN_CHANGE) { self->noteDirty_=true; return 0; }
    if(m==WM_TIMER) {
        if(h==self->notes_) self->saveNote();
        if(h==self->calendar_) self->clock(); return 0;
    }
    if(m==WM_CLOSE) { if(h==self->notes_) self->saveNote(); ShowWindow(h,SW_HIDE); return 0; }
    if(m==WM_SIZE) {
        if(h==self->notes_ && self->edit_) MoveWindow(self->edit_,0,0,LOWORD(l),HIWORD(l),TRUE);
        if(h==self->photo_) InvalidateRect(h,nullptr,TRUE); return 0;
    }
    if(m==WM_PAINT && h==self->photo_ && self->image_) {
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT r; GetClientRect(h,&r);
        Gdiplus::Graphics g(dc); g.Clear(Gdiplus::Color(255,18,18,20));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        float iw=static_cast<float>(self->image_->GetWidth()),ih=static_cast<float>(self->image_->GetHeight());
        float scale=std::min(r.right/iw,r.bottom/ih);
        g.DrawImage(self->image_.get(),(r.right-iw*scale)/2,(r.bottom-ih*scale)/2,iw*scale,ih*scale);
        EndPaint(h,&ps); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
}
