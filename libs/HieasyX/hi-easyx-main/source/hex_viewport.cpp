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
 * \file hex_viewport.cpp
 * \brief Viewport implementation (render-to-sub-buffer with clipping)
 */

#include <include/hex.h>
#include <include/hex_viewport.h>
#include <include/hex_pool.h>

namespace HX {

bool BeginViewport(ViewportProfile &Profile) {
	HXPoint size = Profile.Size;
	auto *layout = CurrentLayout();
	if (layout) {
		if (size.X <= 0) {
			size.X = layout->AvailableRegion.Right - layout->AvailableRegion.Left;
			if (size.X <= 0) size.X = 300;
		}
		if (size.Y <= 0) {
			size.Y = layout->AvailableRegion.Bottom - layout->BaseLine;
			if (size.Y <= 0) size.Y = 200;
		}
	}
	Profile._internal.ComputedSize = size;

	if (RoughVisibility()) {
		LayoutNextControl(size);
		return false;
	}

	if (AccVisibility(size)) {
		LayoutNextControl(size);
		return false;
	}

	auto &context = GetContext();
	auto *parentPainter = CurrentPainter();
	auto *subPainter = GetSubPainterPool().Acquire(size.X, size.Y);
	if (!subPainter) {
		LayoutNextControl(size);
		return false;
	}

	auto position = CurrentLayoutPosition();

	Profile._internal.SubPainter   = subPainter;
	Profile._internal.SavedPainter = context.CurrentWindow->Painter;
	Profile._internal.SavedScrollerX = context.CurrentWindow->ScrollerX;
	Profile._internal.SavedScrollerY = context.CurrentWindow->ScrollerY;
	Profile._internal.RegionLeft   = position.X;
	Profile._internal.RegionTop    = position.Y;

	context.CurrentWindow->Painter = subPainter;
	context.CurrentWindow->ScrollerX += position.X;
	context.CurrentWindow->ScrollerY += position.Y;

	auto *vpLayout = new HXLayoutContext();
	vpLayout->BaseLine         = 0;
	vpLayout->StartBaseLine    = 0;
	vpLayout->LeftPadding      = 0;
	vpLayout->LeftPaddingValue = 0;
	vpLayout->StartLeftPadding = 0;
	vpLayout->AvailableRegion  = {0, 0, size.X, size.Y};
	vpLayout->ControlGap       = 5;
	vpLayout->Direction        = HXLayoutDirection::Vertical;
	vpLayout->ItemAlign        = HXLayoutAlign::Start;
	vpLayout->LeftMargin       = 0;

	context.LayoutStack.push_back(vpLayout);

	return true;
}

void EndViewport(ViewportProfile &Profile) {
	auto &context = GetContext();

	// Pop viewport layout
	if (!context.LayoutStack.empty()) {
		delete context.LayoutStack.back();
		context.LayoutStack.pop_back();
	}

	// Restore painter and scroller offsets
	context.CurrentWindow->Painter   = Profile._internal.SavedPainter;
	context.CurrentWindow->ScrollerX = Profile._internal.SavedScrollerX;
	context.CurrentWindow->ScrollerY = Profile._internal.SavedScrollerY;

	// Blit sub-buffer back to parent painter
	auto *parentPainter = CurrentPainter();
	parentPainter->Begin();
	parentPainter->DrawPainter(Profile._internal.SubPainter,
	                           {Profile._internal.RegionLeft, Profile._internal.RegionTop});
	parentPainter->End();

	// Defer release until after the parent window's draw list has been flushed
	context.CurrentWindow->PendingSubPainters.push_back(Profile._internal.SubPainter);
	Profile._internal.SubPainter = nullptr;

	LayoutNextControl(Profile._internal.ComputedSize);
}

}
