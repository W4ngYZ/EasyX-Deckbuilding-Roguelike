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
 * \file hex_scroller.h
 * \brief The scroller in for the HiEasyX
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {
/**
 * The profile for scroller button
 */
struct ScrollerButtonProfile {
	bool   OnHover = false;
	bool   InDrag  = false;
	HXGInt DeltaX  = 0;
	HXGInt DeltaY  = 0;
};

/**
 * The profile for scroller
 */
struct ScrollerProfile {
	HXPoint Size{};
	HXPoint ViewSpace{};

	HXPoint Where{};

	float VerticalVPer   = 0;
	float HorizontalVPer = 0;

	bool Visible = true;

	ScrollerButtonProfile VerticalButton{};
	ScrollerButtonProfile HorizontalButton{};

	// Grace-period timer for wheel scrolling after mouse leaves scroller
	clock_t LastWheelTime = 0;

	// Customizable rendering / input
	int ScrollbarThickness   = 10;
	int WheelGracePeriodMs   = 500;
};

/**
 * [WARNING]:
 *		DO NOT begin and end a scroller with different profile!
 */

/**
 * Creating a scroll control and begin the scroll environment
 * @param Profile The profile of the scroll
 */
void BeginScroller(ScrollerProfile &Profile);

/**
 * End the scroll environment
 * @param Profile The profile of the scroll
 */
void EndScroller(ScrollerProfile &Profile);
}
