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
 * \file hex_text.cpp
 * \brief The text for HiEasyX
 */

#define _CRT_SECURE_NO_WARNINGS

#include <include/hex.h>
#include <include/hex_text.h>

#include <memory>

namespace HX {
TextProfile::TextProfile() {
	auto &theme = GetTheme();
	Font        = HXFont{};
	Color       = theme.WindowTitleText;
}

void Text(const HXString &Title, bool AutoAlign) {
	if (RoughVisibility()) {
		return;
	}

	auto &theme   = GetTheme();
	auto &context = GetContext();

	auto painter = CurrentPainter();

	auto rectangle = painter->MeasureText(Title, HXFont{}, theme.FontSizeBody);
	if (AccVisibility({rectangle.Right, rectangle.Bottom})) {
		return;
	}

	auto alignHeight = CurrentLayout()->CurrentHeightMax == 0
		                   ? rectangle.Bottom / 2
		                   : CurrentLayout()->CurrentHeightMax / 2;
	if (!AutoAlign) {
		alignHeight = rectangle.Bottom / 2;
	}

	auto position = CurrentLayoutPosition();

	painter->Begin();
	painter->DrawText(Title, HXFont{},
	                  {position.X,
	                   position.Y + (alignHeight - rectangle.Bottom / 2)},
	                  theme.TextPrimary, theme.FontSizeBody);
	painter->End();

	LayoutNextControl({rectangle.Right, rectangle.Bottom});
}

void Text(const HXString &Title, TextProfile &Profile, bool AutoAlign) {
	if (RoughVisibility()) {
		return;
	}

	auto &context = GetContext();

	auto painter = CurrentPainter();

	auto rectangle = painter->MeasureText(Title, Profile.Font, Profile.Height);
	if (AccVisibility({rectangle.Right, rectangle.Bottom})) {
		return;
	}

	auto alignHeight = CurrentLayout()->CurrentHeightMax == 0
		                   ? rectangle.Bottom / 2
		                   : CurrentLayout()->CurrentHeightMax / 2;
	if (!AutoAlign) {
		alignHeight = rectangle.Bottom / 2;
	}

	auto position = CurrentLayoutPosition();

	painter->Begin();
	painter->DrawText(Title, Profile.Font, {position.X,
	                                        position.Y + (alignHeight - rectangle.Bottom / 2)},
	                  Profile.Color, Profile.Height);
	painter->End();

	LayoutNextControl({rectangle.Right, rectangle.Bottom});
}
}
