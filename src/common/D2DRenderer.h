#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
namespace suite {
class D2DRenderer {
public:
    D2DRenderer();
    HRESULT begin(HWND window);
    HRESULT end();
    void circle(float x, float y, float radius, D2D1_COLOR_F color);
    void text(const wchar_t* value, D2D1_RECT_F rect, D2D1_COLOR_F color);
    void reset() { target_.Reset(); }
private:
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> write_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
};
}
