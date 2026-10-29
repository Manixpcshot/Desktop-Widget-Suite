#include "D2DRenderer.h"
#include <stdexcept>
#include <cwchar>
namespace suite {
D2DRenderer::D2DRenderer() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf())) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(write_.GetAddressOf()))) ||
        FAILED(write_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12, L"fa-IR", &format_)))
        throw std::runtime_error("Direct2D/DirectWrite initialization failed");
    format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}
HRESULT D2DRenderer::begin(HWND window) {
    RECT r; GetClientRect(window, &r);
    auto size=D2D1::SizeU(r.right, r.bottom);
    if (!target_) {
        auto result=factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(window,size), &target_);
        if (FAILED(result)) return result;
    }
    auto result=target_->Resize(size);
    if (FAILED(result)) { reset(); return result; }
    const float dpi=static_cast<float>(GetDpiForWindow(window));
    target_->SetDpi(dpi,dpi);
    target_->BeginDraw(); target_->Clear(D2D1::ColorF(0x171d2b));
    return S_OK;
}
HRESULT D2DRenderer::end() {
    auto hr=target_->EndDraw(); if(FAILED(hr)) reset(); return hr;
}
void D2DRenderer::circle(float x,float y,float radius,D2D1_COLOR_F color) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> b;
    if(SUCCEEDED(target_->CreateSolidColorBrush(color,&b)))
        target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x,y),radius,radius),b.Get());
}
void D2DRenderer::text(const wchar_t* value,D2D1_RECT_F rect,D2D1_COLOR_F color) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> b;
    if(SUCCEEDED(target_->CreateSolidColorBrush(color,&b)))
        target_->DrawText(value,static_cast<UINT32>(wcslen(value)),format_.Get(),rect,b.Get());
}
}
