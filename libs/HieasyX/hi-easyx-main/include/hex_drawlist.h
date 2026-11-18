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
 * \file hex_drawlist.h
 * \brief DrawList batching system for HiEasyX
 *
 * Collects drawing commands during the frame and flushes them in batches
 * to reduce SetWorkingImage call overhead.
 */

#pragma once

#include <include/hex_string.h>
#include <include/hex_geo.h>
#include <include/impl/hex_impl.h>
#include <vector>

struct HXBufferPainter;

namespace HX {

enum class DrawCmdType {
	Rect,
	FilledRect,
	FilledRoundRect,
	Text,
	Line,
	LineStyled,
	FilledPolygon,
	Image,
	ShadowRect,
	Painter  // Draw another painter (for SubPainter compositing)
};

/**
 * A single draw command
 */
struct HXDrawCmd {
	DrawCmdType Type;
	HXRect      ClipRect;   // Scissor rect in screen space

	// Common colors
	HXColor Color;
	HXColor FillColor;

	// Rect / FilledRect / FilledRoundRect
	HXRect Rect;
	HXGInt Radius = 0;

	// Text
	HXString Text;
	HXFont   Font;
	HXPoint  TextPos;
	HXGUInt  TextHeight = 16;

	// Line
	HXPoint LineP1;
	HXPoint LineP2;

	// LineStyled
	HXLineStyle LineStyle;

	// FilledPolygon
	std::vector<HXPoint> PolygonPoints;

	// Image
	void *  ImageBuffer = nullptr;
	HXPoint ImagePos;
	HXPoint ImageSize;

	// ShadowRect
	HXGInt ShadowBlur = 0;
	HXGInt ShadowOffsetX = 0;
	HXGInt ShadowOffsetY = 0;

	// Sub-painter blit
	HXBufferPainter *SourcePainter = nullptr;
	HXPoint          BlitPos;
};

/**
 * A list of draw commands that can be batch-executed.
 */
struct HXDrawList {
	std::vector<HXDrawCmd> Commands;

	void Clear() { Commands.clear(); }

	void AddRect(const HXRect &r, HXColor col, const HXRect &clip);
	void AddFilledRect(const HXRect &r, HXColor fill, const HXRect &clip);
	void AddFilledRoundRect(const HXRect &r, HXColor border, HXColor fill, HXGInt radius, const HXRect &clip);
	void AddText(const HXString &text, HXFont font, HXPoint pos, HXColor col, HXGUInt height, const HXRect &clip);
	void AddLine(HXPoint p1, HXPoint p2, HXColor col, const HXRect &clip);
	void AddLineStyled(HXPoint p1, HXPoint p2, HXColor col, HXLineStyle style, const HXRect &clip);
	void AddFilledPolygon(const std::vector<HXPoint> &points, HXColor col, HXColor fill, const HXRect &clip);
	void AddImage(void *buffer, HXPoint pos, HXPoint size, const HXRect &clip);
	void AddPainter(HXBufferPainter *painter, HXPoint pos, const HXRect &clip);
	void AddShadowRect(const HXRect &r, HXColor shadowColor, HXGInt radius, HXGInt blur, HXGInt offsetX, HXGInt offsetY, const HXRect &clip);
};

/**
 * Flush all commands in a DrawList to the given painter.
 * Commands are sorted by clip rect to minimize state changes.
 */
void FlushDrawList(HXBufferPainter *painter, HXDrawList &list);

/**
 * A proxy painter that records all drawing commands into an HXDrawList
 * while delegating measurements and sub-painter creation to a real painter.
 *
 * This allows all controls to transparently use batch rendering without
 * any code changes to the controls themselves.
 */
class HXDrawListRecorder : public HXBufferPainter {
public:
	HXDrawListRecorder(HXBufferPainter *realPainter, HXDrawList *drawList)
		: _realPainter(realPainter), _drawList(drawList) {}

	// Drawing commands are recorded into the draw list
	void DrawLine(HXPoint p1, HXPoint p2, HXColor col) override {
		_drawList->AddLine(p1, p2, col, {});
	}
	void DrawLineStyled(HXPoint p1, HXPoint p2, HXColor col, HXLineStyle style) override {
		_drawList->AddLineStyled(p1, p2, col, style, {});
	}
	void DrawFilledRectangle(HXRect r, HXColor col, HXColor fill) override {
		_drawList->AddFilledRect(r, fill, {});
	}
	void DrawFilledRoundedRectangle(HXRect r, HXColor col, HXColor fill, HXGInt radius) override {
		_drawList->AddFilledRoundRect(r, col, fill, radius, {});
	}
	void DrawFilledPolygon(std::vector<HXPoint> pts, HXColor col, HXColor fill) override {
		_drawList->AddFilledPolygon(pts, col, fill, {});
	}
	void DrawRectangle(HXRect r, HXColor col) override {
		_drawList->AddRect(r, col, {});
	}
	void DrawShadowRect(HXRect r, HXColor shadowColor, HXGInt radius, HXGInt blur, HXGInt offsetX, HXGInt offsetY) override {
		_drawList->AddShadowRect(r, shadowColor, radius, blur, offsetX, offsetY, {});
	}
	void DrawPainter(HXBufferPainter *p, HXPoint where) override {
		_drawList->AddPainter(p, where, {});
	}
	void DrawPainter(HXBufferPainter *p, HXPoint where, HXPoint origin, HXPoint size) override {
		// Clipped blit is not yet supported in DrawList; fall back to full blit
		_drawList->AddPainter(p, where, {});
	}
	void DrawText(const HXString &text, HXFont font, HXPoint where, HXColor col, HXGUInt height) override {
		_drawList->AddText(text, font, where, col, height, {});
	}
	void DrawImage(void *buffer, HXPoint where, HXPoint size) override {
		_drawList->AddImage(buffer, where, size, {});
	}
	void Clear(HXColor col) override {
		_realPainter->Begin();
		_realPainter->Clear(col);
		_realPainter->End();
	}

	// Measurements delegate to the real painter
	HXPoint MeasureImage(void *buffer) override {
		return _realPainter->MeasureImage(buffer);
	}
	HXRect MeasureText(const HXString &text, HXFont font, HXGUInt height) override {
		return _realPainter->MeasureText(text, font, height);
	}
	HXRect MeasureText(const HXChar &text, HXFont font, HXGUInt height) override {
		return _realPainter->MeasureText(text, font, height);
	}

	// Begin/End are no-ops; the real batch Begin/End happens in FlushDrawList
	void Begin() override {}
	void End() override {}

	// Factory methods delegate to the real painter
	HXBufferPainter *CreateSubPainter(HXGInt w, HXGInt h) override {
		return _realPainter->CreateSubPainter(w, h);
	}
	HXBufferPainter *CreateFromBuffer(void *buffer) override {
		return _realPainter->CreateFromBuffer(buffer);
	}
	void *GetBuffer() override {
		return _realPainter->GetBuffer();
	}
	HXPoint GetSize() override {
		return _realPainter->GetSize();
	}

private:
	HXBufferPainter *_realPainter;
	HXDrawList      *_drawList;
};

}
