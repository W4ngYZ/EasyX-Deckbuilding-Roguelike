/*
 * Copyright (c) 2025 HiEasyX
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * \file hex_impl_direct2d.cpp
 * \brief Direct2D render backend implementation
 */


#include <include/impl/Direct2D/hex_impl_direct2d.h>
#include <include/hex.h>
#include <include/impl/EasyX/hex_easyx.h>

#include <windows.h>
#include <graphics.h>
#include <cmath>
#include <memory>

#undef min
#undef max

#pragma comment(lib, "windowscodecs.lib")

namespace HX {

////////////////////////////////////////////////////////////
/// Dynamic loader

using PFN_D2D1CreateFactory = HRESULT(STDMETHODCALLTYPE *)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS *, void **);
using PFN_DWriteCreateFactory = HRESULT(STDMETHODCALLTYPE *)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **);

static HMODULE g_d2d1Module = nullptr;
static HMODULE g_dwriteModule = nullptr;
static PFN_D2D1CreateFactory g_pfnD2D1CreateFactory = nullptr;
static PFN_DWriteCreateFactory g_pfnDWriteCreateFactory = nullptr;

bool LoadDirect2D() {
	if (g_pfnD2D1CreateFactory && g_pfnDWriteCreateFactory) return true;
	g_d2d1Module = LoadLibraryW(L"d2d1.dll");
	g_dwriteModule = LoadLibraryW(L"dwrite.dll");
	if (!g_d2d1Module || !g_dwriteModule) {
		UnloadDirect2D();
		return false;
	}
	g_pfnD2D1CreateFactory = (PFN_D2D1CreateFactory)GetProcAddress(g_d2d1Module, "D2D1CreateFactory");
	g_pfnDWriteCreateFactory = (PFN_DWriteCreateFactory)GetProcAddress(g_dwriteModule, "DWriteCreateFactory");
	if (!g_pfnD2D1CreateFactory || !g_pfnDWriteCreateFactory) {
		UnloadDirect2D();
		return false;
	}
	return true;
}

bool IsDirect2DAvailable() {
	return g_pfnD2D1CreateFactory != nullptr;
}

void UnloadDirect2D() {
	g_pfnD2D1CreateFactory = nullptr;
	g_pfnDWriteCreateFactory = nullptr;
	if (g_d2d1Module) { FreeLibrary(g_d2d1Module); g_d2d1Module = nullptr; }
	if (g_dwriteModule) { FreeLibrary(g_dwriteModule); g_dwriteModule = nullptr; }
}

////////////////////////////////////////////////////////////
/// Helpers

static inline D2D1_COLOR_F ToD2D(HXColor c) {
	return D2D1::ColorF(c.R / 255.0f, c.G / 255.0f, c.B / 255.0f, c.A / 255.0f);
}

static inline D2D1_POINT_2F ToD2D(HXPoint p) {
	return D2D1::Point2F(static_cast<float>(p.X), static_cast<float>(p.Y));
}

static inline D2D1_RECT_F ToD2D(HXRect r) {
	return D2D1::RectF(static_cast<float>(r.Left), static_cast<float>(r.Top),
	                   static_cast<float>(r.Right), static_cast<float>(r.Bottom));
}

////////////////////////////////////////////////////////////
/// P1: Caches

struct HXDirect2DPainter::BrushCache {
	std::unordered_map<uint32_t, ID2D1SolidColorBrush *> brushes;
	~BrushCache() {
		for (auto &kv : brushes) if (kv.second) kv.second->Release();
	}
	ID2D1SolidColorBrush *Get(ID2D1RenderTarget *rt, HXColor color) {
		uint32_t key = (color.A << 24) | (color.R << 16) | (color.G << 8) | color.B;
		auto it = brushes.find(key);
		if (it != brushes.end()) return it->second;
		ID2D1SolidColorBrush *brush = nullptr;
		rt->CreateSolidColorBrush(ToD2D(color), &brush);
		if (brush) brushes[key] = brush;
		return brush;
	}
};

struct HXDirect2DPainter::TextFormatCache {
	std::unordered_map<std::wstring, IDWriteTextFormat *> formats;
	~TextFormatCache() {
		for (auto &kv : formats) if (kv.second) kv.second->Release();
	}
	IDWriteTextFormat *Get(IDWriteFactory *factory, const HXFont &font, HXGUInt height) {
		std::wstring key = font.Family + L"|" + std::to_wstring(static_cast<int>(font.Style)) + L"|" +
		                   (font.Italic ? L"1" : L"0") + L"|" + std::to_wstring(height);
		auto it = formats.find(key);
		if (it != formats.end()) return it->second;

		DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
		switch (font.Style) {
		case HXFontStyle::Light:  weight = DWRITE_FONT_WEIGHT_LIGHT; break;
		case HXFontStyle::Medium: weight = DWRITE_FONT_WEIGHT_MEDIUM; break;
		case HXFontStyle::Bold:   weight = DWRITE_FONT_WEIGHT_BOLD; break;
		case HXFontStyle::Black:  weight = DWRITE_FONT_WEIGHT_BLACK; break;
		default: break;
		}

		IDWriteTextFormat *format = nullptr;
		HRESULT hr = factory->CreateTextFormat(
			font.Family.c_str(), nullptr, weight,
			font.Italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(height), L"zh-CN", &format);

		// Fallback to Microsoft YaHei
		if (FAILED(hr) || !format) {
			hr = factory->CreateTextFormat(
				L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
				static_cast<float>(height), L"zh-CN", &format);
		}

		if (format) formats[key] = format;
		return format;
	}
};

struct HXDirect2DPainter::StrokeStyleCache {
	std::unordered_map<int, ID2D1StrokeStyle *> styles;
	~StrokeStyleCache() {
		for (auto &kv : styles) if (kv.second) kv.second->Release();
	}
	ID2D1StrokeStyle *Get(ID2D1Factory *factory, HXLineStyle style) {
		int key = (style.Thickness << 8) | static_cast<int>(style.Shape);
		auto it = styles.find(key);
		if (it != styles.end()) return it->second;

		D2D1_DASH_STYLE dash = D2D1_DASH_STYLE_SOLID;
		switch (style.Shape) {
		case HXLineShapeStyle::Dash:        dash = D2D1_DASH_STYLE_DASH; break;
		case HXLineShapeStyle::Dotted:      dash = D2D1_DASH_STYLE_DOT; break;
		case HXLineShapeStyle::DashDotted:  dash = D2D1_DASH_STYLE_DASH_DOT; break;
		default: break;
		}

		ID2D1StrokeStyle *stroke = nullptr;
		factory->CreateStrokeStyle(
			D2D1::StrokeStyleProperties(
				D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_ROUND,
				D2D1_LINE_JOIN_MITER, 10.0f, dash, 0.0f),
			nullptr, 0, &stroke);
		if (stroke) styles[key] = stroke;
		return stroke;
	}
};

////////////////////////////////////////////////////////////
/// Painter

HXDirect2DPainter::HXDirect2DPainter(ID2D1RenderTarget *renderTarget, bool owns)
	: rt(renderTarget), ownsRT(owns) {
	if (rt) {
		ID2D1BitmapRenderTarget *b = nullptr;
		if (SUCCEEDED(rt->QueryInterface(__uuidof(ID2D1BitmapRenderTarget), (void **)&b))) {
			brt = b;
			D2D1_SIZE_F sz = rt->GetSize();
			_width = static_cast<HXGInt>(sz.width);
			_height = static_cast<HXGInt>(sz.height);
		}
		// DefaultPainter (DCRenderTarget) size is resolved lazily in Begin()
	}
	brushCache = std::make_unique<BrushCache>();
	textFormatCache = std::make_unique<TextFormatCache>();
	strokeStyleCache = std::make_unique<StrokeStyleCache>();
}

HXDirect2DPainter::~HXDirect2DPainter() {
	brushCache.reset();
	textFormatCache.reset();
	strokeStyleCache.reset();
	if (brt) { brt->Release(); brt = nullptr; }
	if (ownsRT && rt) { rt->Release(); rt = nullptr; }
}

void HXDirect2DPainter::SetRenderTarget(ID2D1RenderTarget *renderTarget, bool owns) {
	if (brt) { brt->Release(); brt = nullptr; }
	if (ownsRT && rt) { rt->Release(); }
	rt = renderTarget;
	ownsRT = owns;
	if (rt) {
		D2D1_SIZE_F sz = rt->GetSize();
		_width = static_cast<HXGInt>(sz.width);
		_height = static_cast<HXGInt>(sz.height);
		ID2D1BitmapRenderTarget *b = nullptr;
		if (SUCCEEDED(rt->QueryInterface(__uuidof(ID2D1BitmapRenderTarget), (void **)&b))) {
			brt = b;
		}
	}
}

ID2D1Bitmap *HXDirect2DPainter::GetD2DBitmap() const {
	if (!brt) return nullptr;
	ID2D1Bitmap *bmp = nullptr;
	brt->GetBitmap(&bmp);
	return bmp;
}

void HXDirect2DPainter::Begin() {
	if (!rt) return;
	if (brt) {
		// Sub-painter: always begin/end its own bitmap target
		rt->BeginDraw();
	} else {
		// Default painter (DCRenderTarget): bind to EasyX back-buffer HDC then begin
		auto *dcRT = static_cast<ID2D1DCRenderTarget *>(rt);
		_width = getwidth();
		_height = getheight();
		HDC hdc = GetImageHDC();
		RECT rc = {0, 0, _width, _height};
		dcRT->BindDC(hdc, &rc);
		rt->BeginDraw();
	}
}

void HXDirect2DPainter::End() {
	if (!rt) return;
	if (brt) {
		rt->EndDraw();
	} else {
		rt->EndDraw();
	}
}

void HXDirect2DPainter::DrawShadowRect(HXRect rect, HXColor shadowColor, HXGInt radius, HXGInt blur, HXGInt offsetX, HXGInt offsetY) {
	if (!rt || blur <= 0) return;

	HXRect shadowRect = rect;
	shadowRect.Left   += offsetX;
	shadowRect.Top    += offsetY;
	shadowRect.Right  += offsetX;
	shadowRect.Bottom += offsetY;

	for (HXGInt i = blur; i >= 1; --i) {
		HXColor color = shadowColor;
		float alphaFactor = (1.0f - static_cast<float>(i) / static_cast<float>(blur + 2)) * 0.3f;
		color.A = static_cast<HXColInt>(std::min(255.0f, shadowColor.A * alphaFactor));

		HXRect r = shadowRect;
		r.Left   -= i;
		r.Top    -= i;
		r.Right  += i;
		r.Bottom += i;

		auto *brush = brushCache->Get(rt, color);
		if (brush) {
			D2D1_ROUNDED_RECT rr = { ToD2D(r), static_cast<float>(radius + i), static_cast<float>(radius + i) };
			rt->FillRoundedRectangle(rr, brush);
		}
	}
}

void HXDirect2DPainter::DrawLine(HXPoint p1, HXPoint p2, HXColor col) {
	if (!rt) return;
	auto *b = brushCache->Get(rt, col);
	if (b) rt->DrawLine(ToD2D(p1), ToD2D(p2), b, 1.0f);
}

void HXDirect2DPainter::DrawLineStyled(HXPoint p1, HXPoint p2, HXColor col, HXLineStyle style) {
	if (!rt) return;
	auto *b = brushCache->Get(rt, col);
	ID2D1Factory *f = nullptr;
	rt->GetFactory(&f);
	ID2D1StrokeStyle *s = f ? strokeStyleCache->Get(f, style) : nullptr;
	if (f) f->Release();
	if (b) rt->DrawLine(ToD2D(p1), ToD2D(p2), b, static_cast<float>(style.Thickness), s);
}

void HXDirect2DPainter::DrawRectangle(HXRect r, HXColor col) {
	if (!rt) return;
	auto *b = brushCache->Get(rt, col);
	if (b) rt->DrawRectangle(ToD2D(r), b, 1.0f);
}

void HXDirect2DPainter::DrawFilledRectangle(HXRect r, HXColor border, HXColor fill) {
	if (!rt) return;
	auto *fb = brushCache->Get(rt, fill);
	auto *bb = brushCache->Get(rt, border);
	if (fb) rt->FillRectangle(ToD2D(r), fb);
	if (bb) rt->DrawRectangle(ToD2D(r), bb, 1.0f);
}

void HXDirect2DPainter::DrawFilledRoundedRectangle(HXRect r, HXColor border, HXColor fill, HXGInt radius) {
	if (!rt) return;
	auto *fb = brushCache->Get(rt, fill);
	auto *bb = brushCache->Get(rt, border);
	D2D1_ROUNDED_RECT rr = { ToD2D(r), static_cast<float>(radius), static_cast<float>(radius) };
	if (fb) rt->FillRoundedRectangle(rr, fb);
	if (bb) rt->DrawRoundedRectangle(rr, bb, 1.0f);
}

void HXDirect2DPainter::DrawFilledPolygon(std::vector<HXPoint> pts, HXColor border, HXColor fill) {
	if (!rt || pts.size() < 3) return;
	ID2D1Factory *f = nullptr;
	rt->GetFactory(&f);
	if (!f) return;
	ID2D1PathGeometry *geo = nullptr;
	f->CreatePathGeometry(&geo);
	f->Release();
	if (!geo) return;
	ID2D1GeometrySink *sink = nullptr;
	geo->Open(&sink);
	sink->BeginFigure(ToD2D(pts[0]), D2D1_FIGURE_BEGIN_FILLED);
	for (size_t i = 1; i < pts.size(); ++i)
		sink->AddLine(ToD2D(pts[i]));
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();

	auto *fb = brushCache->Get(rt, fill);
	auto *bb = brushCache->Get(rt, border);
	if (fb) rt->FillGeometry(geo, fb);
	if (bb) rt->DrawGeometry(geo, bb, 1.0f);
	sink->Release();
	geo->Release();
}

void HXDirect2DPainter::DrawText(const HXString &text, HXFont font, HXPoint where, HXColor col, HXGUInt height) {
	if (!rt || text.empty()) return;
	auto *b = brushCache->Get(rt, col);

	IDWriteFactory *dwf = nullptr;
	ID2D1Factory *f = nullptr;
	rt->GetFactory(&f);
	if (f) {
		auto *backend = dynamic_cast<HXDirect2DRenderBackend *>(HX::GetRenderBackend());
		if (backend) dwf = backend->GetWriteFactory();
		f->Release();
	}
	if (!b || !dwf) return;

	// D2D text is ~3px larger than EasyX GDI at the same nominal size
	HXGUInt d2dHeight = height > 3 ? height - 3 : 1;
	auto *fmt = textFormatCache->Get(dwf, font, d2dHeight);
	if (!fmt) return;

	IDWriteTextLayout *layout = nullptr;
	dwf->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()), fmt,
	                      10000.0f, 10000.0f, &layout);
	if (!layout) return;

	rt->DrawTextLayout(ToD2D(where), layout, b,
	                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
	layout->Release();
}

HXRect HXDirect2DPainter::MeasureText(const HXString &text, HXFont font, HXGUInt height) {
	if (text.empty()) return {0,0,0,0};
	IDWriteFactory *dwf = nullptr;
	auto *backend = dynamic_cast<HXDirect2DRenderBackend *>(HX::GetRenderBackend());
	if (backend) dwf = backend->GetWriteFactory();
	if (!dwf) return {0,0,0,0};

	HXGUInt d2dHeight = height > 3 ? height - 3 : 1;
	auto *fmt = textFormatCache->Get(dwf, font, d2dHeight);
	if (!fmt) return {0,0,0,0};

	IDWriteTextLayout *layout = nullptr;
	dwf->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()), fmt,
	                      10000.0f, 10000.0f, &layout);
	if (!layout) return {0,0,0,0};

	DWRITE_TEXT_METRICS m;
	layout->GetMetrics(&m);
	layout->Release();

	return {0, 0, static_cast<HXGInt>(std::ceil(m.width)), static_cast<HXGInt>(std::ceil(m.height))};
}

HXRect HXDirect2DPainter::MeasureText(const HXChar &text, HXFont font, HXGUInt height) {
	return MeasureText(HXString(1, text), font, height);
}

void HXDirect2DPainter::DrawImage(void *buffer, HXPoint where, HXPoint size) {
	if (!rt || !buffer) return;
	auto *img = static_cast<IMAGE *>(buffer);
	if (!img) return;
	int w = img->getwidth();
	int h = img->getheight();
	if (w <= 0 || h <= 0) return;

	DWORD *pixels = GetImageBuffer(img);
	if (!pixels) return;

	ID2D1Bitmap *bmp = nullptr;
	D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT));

	if (SUCCEEDED(rt->CreateBitmap(D2D1::SizeU(w, h), pixels, w * 4, &props, &bmp))) {
		D2D1_RECT_F dest = ToD2D(HXRect{where.X, where.Y, where.X + size.X, where.Y + size.Y});
		rt->DrawBitmap(bmp, dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		bmp->Release();
	}
}

HXPoint HXDirect2DPainter::MeasureImage(void *buffer) {
	auto *img = static_cast<IMAGE *>(buffer);
	if (!img) return {0, 0};
	return {img->getwidth(), img->getheight()};
}

void HXDirect2DPainter::DrawPainter(HXBufferPainter *painter, HXPoint where) {
	auto *other = dynamic_cast<HXDirect2DPainter *>(painter);
	if (!other || !rt) return;
	ID2D1Bitmap *bmp = other->GetD2DBitmap();
	if (!bmp) return;
	D2D1_RECT_F dest = D2D1::RectF(
		static_cast<float>(where.X), static_cast<float>(where.Y),
		static_cast<float>(where.X + other->_width), static_cast<float>(where.Y + other->_height));
	rt->DrawBitmap(bmp, dest);
	bmp->Release();
}

void HXDirect2DPainter::DrawPainter(HXBufferPainter *painter, HXPoint where, HXPoint origin, HXPoint size) {
	auto *other = dynamic_cast<HXDirect2DPainter *>(painter);
	if (!other || !rt) return;
	ID2D1Bitmap *bmp = other->GetD2DBitmap();
	if (!bmp) return;
	D2D1_RECT_F dest = D2D1::RectF(
		static_cast<float>(where.X), static_cast<float>(where.Y),
		static_cast<float>(where.X + size.X), static_cast<float>(where.Y + size.Y));
	D2D1_RECT_F src = D2D1::RectF(
		static_cast<float>(origin.X), static_cast<float>(origin.Y),
		static_cast<float>(origin.X + size.X), static_cast<float>(origin.Y + size.Y));
	rt->DrawBitmap(bmp, dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
	bmp->Release();
}

void HXDirect2DPainter::Clear(HXColor col) {
	if (rt) rt->Clear(ToD2D(col));
}

HXBufferPainter *HXDirect2DPainter::CreateSubPainter(HXGInt w, HXGInt h) {
	if (!rt) return nullptr;
	ID2D1BitmapRenderTarget *sub = nullptr;
	if (FAILED(rt->CreateCompatibleRenderTarget(
		D2D1::SizeF(static_cast<float>(w), static_cast<float>(h)), &sub)) || !sub) {
		return nullptr;
	}
	auto *p = new HXDirect2DPainter(sub, true);
	p->_width = w;
	p->_height = h;
	return p;
}

HXBufferPainter *HXDirect2DPainter::CreateFromBuffer(void *buffer) {
	(void)buffer;
	// D2D backend does not use raw buffer; return a wrapper on the same RT
	return new HXDirect2DPainter(rt, false);
}

void *HXDirect2DPainter::GetBuffer() {
	return nullptr; // D2D painter does not expose raw buffer
}

HXPoint HXDirect2DPainter::GetSize() {
	return {_width, _height};
}

////////////////////////////////////////////////////////////
/// Backend

HXDirect2DRenderBackend::HXDirect2DRenderBackend() = default;
HXDirect2DRenderBackend::~HXDirect2DRenderBackend() {
	Shutdown();
}

void HXDirect2DRenderBackend::Init() {
	if (initialized) return;
	if (!LoadDirect2D()) return;

	HRESULT hr = g_pfnD2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED,
		__uuidof(ID2D1Factory), nullptr, reinterpret_cast<void **>(&factory));
	if (FAILED(hr)) return;

	hr = g_pfnDWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&writeFactory));
	if (FAILED(hr)) {
		factory->Release(); factory = nullptr;
		return;
	}

	// HwndRenderTarget will be created lazily in NewFrame when HWND is available
	initialized = true;
}

void HXDirect2DRenderBackend::Shutdown() {
	if (dcRT) { dcRT->Release(); dcRT = nullptr; }
	if (writeFactory) { writeFactory->Release(); writeFactory = nullptr; }
	if (factory) { factory->Release(); factory = nullptr; }
	initialized = false;
}

void HXDirect2DRenderBackend::NewFrame() {
	if (!initialized) return;
	if (dcRT) return;
	if (!factory) return;

	D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_HARDWARE,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
		0, 0, D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);
	HRESULT hr = factory->CreateDCRenderTarget(&rtProps, &dcRT);
	if (FAILED(hr)) {
		rtProps.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
		hr = factory->CreateDCRenderTarget(&rtProps, &dcRT);
	}
	if (dcRT) {
		dcRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dcRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	}
}

void HXDirect2DRenderBackend::EndFrame() {
	// EndDraw is deferred until HXDirect2DPainter::End() on the default painter
}

HXBufferPainter *HXDirect2DRenderBackend::CreatePainter(HXGInt w, HXGInt h) {
	if (!dcRT) return nullptr;
	ID2D1BitmapRenderTarget *sub = nullptr;
	if (FAILED(dcRT->CreateCompatibleRenderTarget(
		D2D1::SizeF(static_cast<float>(w), static_cast<float>(h)), &sub)) || !sub) {
		return nullptr;
	}
	auto *p = new HXDirect2DPainter(sub, true);
	return p;
}

} // namespace HX
