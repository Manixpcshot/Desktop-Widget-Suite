#pragma once
#include <windows.h>
#include <memory>
#include "common/Database.h"
namespace Gdiplus { class Image; }
namespace suite {
class NativeTools {
public:
    explicit NativeTools(Database& db);
    ~NativeTools();
    void show(int index,HWND owner);
private:
    Database& db_;
    bool noteDirty_=false;
    HWND notes_=nullptr,edit_=nullptr,calendar_=nullptr,clock_=nullptr,photo_=nullptr;
    std::unique_ptr<Gdiplus::Image> image_;
    void saveNote();
    void clock();
    HWND create(const wchar_t* name,int width,int height);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
};
}
