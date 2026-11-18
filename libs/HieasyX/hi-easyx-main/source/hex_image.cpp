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
 * \file hex_image.cpp
 * \brief The image label control for HiEasyX
 */

#include "include/hex.h"

#include <include/hex_image.h>

namespace HX {
void Image(void *Buffer, HXPoint Size) {
	if (RoughVisibility()) {
		return;
	}

	auto &context = GetContext();
	auto  painter = CurrentPainter();

	auto position = CurrentLayoutPosition();

	auto imageRect = painter->MeasureImage(Buffer);

	Size.X = Size.X <= 0 ? imageRect.X : Size.X;
	Size.Y = Size.Y <= 0 ? imageRect.Y : Size.Y;

	if (AccVisibility(Size)) {
		return;
	}

	painter->Begin();
	painter->DrawImage(Buffer, position, Size);
	painter->End();

	LayoutNextControl(Size);
}
}
