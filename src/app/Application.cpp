#include "Application.h"
#include "TrayIcon.h"
#include "edge_panel/EdgePanel.h"
#include "settings/SettingsWindow.h"
#include <filesystem>
#include <shlobj.h>
#include "update/AutoUpdate.h"
#include "tools/NativeTools.h"
namespace suite {
int runApplication() {
    PWSTR raw=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&raw)))
        throw std::runtime_error("Cannot locate LocalAppData");
    std::filesystem::path dir(raw); CoTaskMemFree(raw);
    dir/=L"DesktopSuite"; std::filesystem::create_directories(dir);
    auto path=(dir/L"suite.db").u8string();
    Database db(std::string(reinterpret_cast<const char*>(path.data()),path.size()));
    auto config=db.load();
    EdgePanel panel(db,config);
    SettingsWindow settings(db,config);
    NativeTools tools(db);
    panel.onTool=[&](int index) { tools.show(index,panel.hwnd()); };
    TrayIcon tray;
    try { setAutoRun(preference(L"AutoRun")); } catch(const std::exception& e) { db.log(e.what()); }
    AutoUpdate updater(panel.hwnd());
    settings.onCheckUpdate=[&] { updater.check(); };
    panel.onUpdate=[&] {
        auto status=updater.result();
        db.log(status.message);
        if(status.ready) {
            if(!status.manual || MessageBoxW(panel.hwnd(),L"A verified update is ready. Restart and install now?",L"Desktop Suite update",MB_YESNO|MB_ICONINFORMATION)==IDYES) {
                if(updater.apply()) PostQuitMessage(0);
                else MessageBoxW(panel.hwnd(),L"Could not start update helper. Current version is unchanged.",L"Update failed",MB_ICONERROR);
            }
        } else if(status.manual) MessageBoxA(panel.hwnd(),status.message.c_str(),"Desktop Suite update",MB_OK|MB_ICONINFORMATION);
    };
    panel.show(); // Discoverable at first launch instead of an invisible tray-only startup.
    panel.onSettings=[&] { settings.show(); };
    settings.onApply=[&] { panel.reconfigure(); };
    tray.onCommand=[&](int cmd) {
        if(cmd==1) panel.show();
        if(cmd==2) settings.show();
        if(cmd==3) PostQuitMessage(0);
        if(cmd==4) updater.check();
    };
    MSG msg{}; int result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0) {
        HWND root=GetAncestor(msg.hwnd,GA_ROOT);
        if(root && root==settings.hwnd() && IsDialogMessageW(root,&msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if(result==-1) { db.log("GetMessage failed",GetLastError()); return 1; }
    return static_cast<int>(msg.wParam);
}
}
