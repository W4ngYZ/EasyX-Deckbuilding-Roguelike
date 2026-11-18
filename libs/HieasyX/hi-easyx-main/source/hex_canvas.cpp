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
 * \file hex_canvas.cpp
 * \brief The canvas control for HiEasyX
 */

#include <include/hex.h>
#include <include/hex_canvas.h>
#include <include/hex_pool.h>

namespace HX {
void Canvas(HXPoint Size, const std::function<void(HXBufferPainter *, HXPoint)> &Callback) {
	if (RoughVisibility()) {
		return;
	}

	auto &context = GetContext();

	if (AccVisibility(Size)) {
		return;
	}

	auto painter = CurrentPainter();
	auto subPainter = GetSubPainterPool().Acquire(Size.X, Size.Y);
	if (!subPainter) {
		LayoutNextControl(Size);
		return;
	}

	auto position = CurrentLayoutPosition();

	subPainter->Begin();
	Callback(subPainter, Size);
	subPainter->End();

	painter->Begin();
	painter->DrawPainter(subPainter, position);
	painter->End();

	LayoutNextControl(Size);

	// Defer release until after the parent window's draw list has been flushed
	context.CurrentWindow->PendingSubPainters.push_back(subPainter);
}
}
