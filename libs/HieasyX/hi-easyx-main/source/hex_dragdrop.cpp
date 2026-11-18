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
 * \file hex_dragdrop.cpp
 * \brief Drag and Drop system implementation
 */

#include <include/hex_dragdrop.h>
#include <include/hex.h>

namespace HX {

static HXDragDropPayload G_DragDropPayload;
static bool              G_DragDropActive = false;
static DragDropConfig    G_DragDropConfig;

void BeginDrag(const HXString &Type, const HXString &Data) {
	G_DragDropPayload.Type = Type;
	G_DragDropPayload.Data = Data;
	G_DragDropActive       = true;
}

bool IsDragging(const HXString &Type) {
	if (!G_DragDropActive) return false;
	if (Type.empty()) return true;
	return G_DragDropPayload.Type == Type;
}

const HXDragDropPayload *AcceptDrop(const HXString &Type) {
	if (!G_DragDropActive) return nullptr;
	if (Type.empty() || G_DragDropPayload.Type == Type) {
		return &G_DragDropPayload;
	}
	return nullptr;
}

void EndDrag() {
	G_DragDropActive       = false;
	G_DragDropPayload      = HXDragDropPayload{};
}

DragDropConfig &GetDragDropConfig() {
	return G_DragDropConfig;
}

void RenderDragDropOverlay(HXBufferPainter *painter) {
	if (!G_DragDropActive || !painter) return;

	// Find the most recent mouse position in the message query.
	HXPoint mouse = {0, 0};
	for (auto it = GetContext().MessageQuery.rbegin(); it != GetContext().MessageQuery.rend(); ++it) {
		if (it->MouseAction) {
			mouse = {it->MouseX, it->MouseY};
			break;
		}
	}
	if (mouse.X == 0 && mouse.Y == 0) return;

	auto &config = G_DragDropConfig;

	HXString label = G_DragDropPayload.Type + HXStr(": ") + G_DragDropPayload.Data;
	HXFont   font{};
	auto     textRect = painter->MeasureText(label, font, config.FontSize);

	HXGInt w = textRect.CalWidth() + config.Padding * 2;
	HXGInt h = textRect.CalHeight() + config.Padding * 2;

	// Draw offset from cursor so it doesn't cover the target.
	HXRect bg{mouse.X + config.OffsetXY, mouse.Y + config.OffsetXY, mouse.X + config.OffsetXY + w, mouse.Y + config.OffsetXY + h};

	if (config.RenderPreviewCallback) {
		config.RenderPreviewCallback(painter, bg, G_DragDropPayload);
		return;
	}

	// Semi-transparent dark background
	painter->DrawFilledRoundedRectangle(bg, config.BorderColor, config.BackgroundColor, 4);
	// Text
	painter->DrawText(label, font, {bg.Left + config.Padding, bg.Top + config.Padding}, config.TextColor, config.FontSize);
}

}
