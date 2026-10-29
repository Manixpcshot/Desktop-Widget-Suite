#include "Application.h"
#include "TrayIcon.h"
#include "edge_panel/EdgePanel.h"
#include "settings/SettingsWindow.h"
#include <filesystem>
#include <shlobj.h>
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
    TrayIcon tray;
    panel.onSettings=[&] { settings.show(); };
    settings.onApply=[&] { panel.reconfigure(); };
    tray.onCommand=[&](int cmd) {
        if(cmd==1) panel.show();
        if(cmd==2) settings.show();
        if(cmd==3) PostQuitMessage(0);
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
