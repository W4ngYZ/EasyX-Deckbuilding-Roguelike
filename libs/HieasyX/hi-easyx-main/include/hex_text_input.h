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
 * \file hex_text_input.h
 * \brief The text input for the HiEasyX
 */

#pragma once

#pragma once

#include <include/hex.h>

#include <ctime>
#include <vector>

namespace HX {
/**
 * Profile for the scroll button, a text input is just like a pro scroll
 */
struct TextInputScrollButtonProfile {
	bool   OnHover    = false;
	bool   InDrag     = false;
	HXGInt DeltaX     = 0;
	HXGInt DeltaY     = 0;
};

/**
 * The cursor method for the text input
 */
struct Cursor {
	HXGInt Position;
};

/**
 * The profile for text input
 */
struct TextInputProfile {
	HXString            Text;
	HXPoint             Size{400, 300};
	HXPoint             Where{0, 0};
	HXGInt              SelectingPoint = 0;
	time_t              CursorClock;
	bool                DisplayCursor = false;
	std::vector<Cursor> Cursors;
	bool   MouseStyle = false;

	bool OnFocus = false;
	bool InDrag  = false;

	float VerticalVPer   = 0;
	float HorizontalVPer = 0;

	TextInputScrollButtonProfile VerticalButton{};
	TextInputScrollButtonProfile HorizontalButton{};

	// IME composition state
	bool     InIMEComposition = false;
	HXString IMECompositionString;
	HXRect   IMELastRect{0, 0, 0, 0};
	time_t   LastIMEEndTime = 0;

	bool ReadOnly = false;

	// Double-click tracking
	clock_t LastClickTime = 0;
	HXGInt  LastClickPos = -1;

	// Customizable rendering / input
	int FontSize      = 16;
	int LineGap       = 3;
	int PaddingX      = 10;
	int CursorBlinkMs = 500;
};

/**
 * Creating a multiple line text input control
 * @param Profile The profile of the input control
 */
void TextInput(TextInputProfile &Profile);
}
