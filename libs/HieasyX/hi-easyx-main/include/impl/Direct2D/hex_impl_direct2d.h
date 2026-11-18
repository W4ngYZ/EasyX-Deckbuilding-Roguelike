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
 * \file hex_impl_direct2d.h
 * \brief Direct2D hardware-accelerated render backend
 */


#pragma once

#include <include/impl/hex_impl.h>
#include <include/hex_render_backend.h>

#include <d2d1.h>
#include <dwrite.h>
#include <unordered_map>
#include <string>
#include <memory>

namespace HX {

////////////////////////////////////////////////////////////
/// Dynamic loader

bool LoadDirect2D();
bool IsDirect2DAvailable();
void UnloadDirect2D();

////////////////////////////////////////////////////////////
/// Render Backend

class HXDirect2DRenderBackend : public HXRenderBackend {
public:
	HXDirect2DRenderBackend();
	~HXDirect2DRenderBackend() override;

	void Init() override;
	void Shutdown() override;
	void NewFrame() override;
	void EndFrame() override;
	HXBufferPainter *CreatePainter(HXGInt width, HXGInt height) override;

	ID2D1Factory *GetFactory() const { return factory; }
	IDWriteFactory *GetWriteFactory() const { return writeFactory; }
	ID2D1DCRenderTarget *GetDCRenderTarget() const { return dcRT; }

private:
	ID2D1Factory *factory = nullptr;
	IDWriteFactory *writeFactory = nullptr;
	ID2D1DCRenderTarget *dcRT = nullptr;
	bool initialized = false;
};

////////////////////////////////////////////////////////////
/// Painter

class HXDirect2DPainter : public HXBufferPainter {
public:
	HXDirect2DPainter(ID2D1RenderTarget *rt, bool ownsRT);
	~HXDirect2DPainter() override;

	void SetRenderTarget(ID2D1RenderTarget *rt, bool ownsRT);

public:
	void DrawLine(HXPoint Point1, HXPoint Point2, HXColor Color) override;
	void DrawLineStyled(HXPoint Point1, HXPoint Point2, HXColor Color, HXLineStyle Style) override;
	void DrawRectangle(HXRect Rect, HXColor Color) override;
	void DrawFilledRectangle(HXRect Rect, HXColor Color, HXColor FillColor) override;
	void DrawFilledRoundedRectangle(HXRect Rect, HXColor Color, HXColor FillColor, HXGInt Radius) override;
	void DrawFilledPolygon(std::vector<HXPoint> Points, HXColor Color, HXColor FillColor) override;
	void DrawText(const HXString &Text, HXFont Font, HXPoint Where, HXColor Color, HXGUInt Height) override;
	void DrawImage(void *Buffer, HXPoint Where, HXPoint Size) override;
	HXPoint MeasureImage(void *Buffer) override;
	void DrawPainter(HXBufferPainter *Painter, HXPoint Where) override;
	void DrawPainter(HXBufferPainter *Painter, HXPoint Where, HXPoint Origin, HXPoint Size) override;
	void Clear(HXColor Color) override;
	HXRect MeasureText(const HXString &Text, HXFont Font, HXGUInt Height) override;
	HXRect MeasureText(const HXChar &Text, HXFont Font, HXGUInt Height) override;

	HXBufferPainter *CreateSubPainter(HXGInt Width, HXGInt Height) override;
	HXBufferPainter *CreateFromBuffer(void *Buffer) override;
	void *GetBuffer() override;
	HXPoint GetSize() override;

	void Begin() override;
	void End() override;
	void DrawShadowRect(HXRect Rect, HXColor ShadowColor, HXGInt Radius, HXGInt Blur, HXGInt OffsetX, HXGInt OffsetY) override;

	ID2D1RenderTarget *GetRenderTarget() const { return rt; }
	ID2D1Bitmap *GetD2DBitmap() const;

private:
	ID2D1RenderTarget *rt = nullptr;
	ID2D1BitmapRenderTarget *brt = nullptr;
	bool ownsRT = false;
	HXGInt _width = 0;
	HXGInt _height = 0;

	ID2D1SolidColorBrush *GetBrush(HXColor color);
	IDWriteTextFormat *GetTextFormat(const HXFont &font, HXGUInt height);
	ID2D1StrokeStyle *GetStrokeStyle(HXLineStyle style);

	// P1: caches (stored per-painter to avoid thread-safety issues)
	struct BrushCache;
	struct TextFormatCache;
	struct StrokeStyleCache;
	std::unique_ptr<BrushCache> brushCache;
	std::unique_ptr<TextFormatCache> textFormatCache;
	std::unique_ptr<StrokeStyleCache> strokeStyleCache;
};

} // namespace HX
