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
 * \file hex_viewport.h
 * \brief Off-screen viewport / render-to-texture for HiEasyX
 *
 * A Viewport captures all controls placed between BeginViewport/EndViewport
 * into an off-screen buffer. The buffer can later be displayed with HX::Image()
 * or drawn onto a Canvas. This enables multi-viewport UIs, minimaps,
 * picture-in-picture, and cached static panels.
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {

/**
 * Profile for a viewport.
 */
struct ViewportProfile {
	HXPoint Size = {300, 200};
	/**
	 * Internal state – user code should not touch.
	 */
	struct {
		HXBufferPainter *SubPainter     = nullptr;
		HXBufferPainter *SavedPainter   = nullptr;
		HXGInt           SavedScrollerX = 0;
		HXGInt           SavedScrollerY = 0;
		HXGInt           RegionLeft     = 0;
		HXGInt           RegionTop      = 0;
		HXPoint          ComputedSize   = {0, 0};
	} _internal;
};

/**
 * Begin a viewport region.
 * All controls between BeginViewport and EndViewport are rendered into an
 * off-screen buffer instead of the current window painter.
 * @param Profile Viewport settings (Size must be > 0)
 * @return true if the viewport was successfully created
 */
bool BeginViewport(ViewportProfile &Profile);

/**
 * End a viewport region.
 * The off-screen buffer is kept inside Profile; it will be drawn automatically
 * when the parent window is rendered. You can also obtain the buffer via
 * HX::GetHXBuffer() for use with HX::Image().
 */
void EndViewport(ViewportProfile &Profile);

}
