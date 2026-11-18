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
 * \file hex_dragdrop.h
 * \brief Drag and Drop system for HiEasyX
 */

#pragma once

#include <include/hex_string.h>
#include <include/hex_geo.h>
#include <include/impl/hex_impl.h>
#include <functional>

struct HXBufferPainter;

namespace HX {

/**
 * Drag & Drop payload
 */
struct HXDragDropPayload {
	HXString Type;
	HXString Data;
	void    *Source      = nullptr;
	HXPoint  MouseOffset = {0, 0};
};

struct DragDropConfig {
	std::function<void(HXBufferPainter* p, const HXRect& rect, const HXDragDropPayload& payload)> RenderPreviewCallback;
	int FontSize = 16;
	HXColor BackgroundColor{51, 51, 51, 204};
	HXColor BorderColor{170, 170, 170, 255};
	HXColor TextColor{255, 255, 255, 255};
	int Padding = 6;
	int OffsetXY = 15;
};

/**
 * Begin a drag operation. Call when a draggable item receives MouseLeftPressed.
 */
void BeginDrag(const HXString &Type, const HXString &Data);

/**
 * Check if a drag operation is currently active.
 * @param Type If non-empty, also checks that the payload type matches.
 */
bool IsDragging(const HXString &Type = HXString{});

/**
 * Attempt to accept a drop on the current target.
 * Returns the payload pointer if a drag of the given type is active, else nullptr.
 * The caller must verify that the mouse is hovering over the target region.
 */
const HXDragDropPayload *AcceptDrop(const HXString &Type);

/**
 * End the current drag operation. Call on MouseLeftRelease (or when cancelled).
 */
void EndDrag();

/**
 * Access the global drag-drop config (preview style, colors, callbacks).
 */
DragDropConfig &GetDragDropConfig();

/**
 * Render the floating drag preview overlay. Called automatically by HX::Render().
 */
void RenderDragDropOverlay(HXBufferPainter *painter);

}
