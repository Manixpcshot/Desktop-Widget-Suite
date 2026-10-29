#include <windows.h>
#include <objbase.h>
#include <stdexcept>
#include "app/Application.h"
#include "update/AutoUpdate.h"
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try {
        if(suite::handleUpdateCommand() || suite::bootstrapUserInstall()) return 0;
    } catch(const std::exception& e) {
        MessageBoxA(nullptr,e.what(),"Desktop Suite install/update error",MB_OK|MB_ICONERROR); return 1;
    }
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\DesktopSuite.Milestone1");
    if(!singleton) return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        if(HWND panel=FindWindowW(L"DesktopSuite.Edge",nullptr)) PostMessageW(panel,WM_APP+3,0,0);
        CloseHandle(singleton); return 0;
    }
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=1;
    try {
        if(FAILED(com)) throw std::runtime_error("COM initialization failed");
        result=suite::runApplication();
    } catch(const std::exception& e) {
        MessageBoxA(nullptr,e.what(),"Desktop Suite startup error",MB_OK|MB_ICONERROR);
    }
    if(SUCCEEDED(com)) CoUninitialize();
    CloseHandle(singleton); return result;
}
