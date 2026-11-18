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
 * \file hex_slider.h
 * \brief The value slider control for HiEasyX
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

#include <array>
#include <ctime>
#include <functional>

namespace HX {

/**
 * The profile for slider
 * @tparam Type The type of the slider value
 */
template <class Type>
struct SliderProfile {
	bool   InDrag  = false;
	bool   OnFocus = false;
	bool   InEditMode    = false;
	HXString EditText;
	clock_t  LastClickClock = 0;
	Type   MinValue = Type{};
	Type   MaxValue = Type(1);
	HXGInt Width = -1;

	// Edit-mode cursor / selection state
	HXGInt EditCursorPos   = 0;
	HXGInt EditSelectStart = 0;
	HXGInt EditSelectEnd   = 0;
	bool   EditDragging    = false;
	clock_t EditBlinkClock  = 0;
	bool   EditShowCursor   = true;

	// Customizable rendering / input
	int FontSize = 20;
	int Height = 22;
	int EditBlinkMs = 500;
	Type Step = Type{};
	std::function<HXString(Type value)> FormatCallback;
};

using SliderProfile1f = SliderProfile<float>;
using SliderProfile2f = std::array<SliderProfile<float>, 2>;
using SliderProfile3f = std::array<SliderProfile<float>, 3>;
using SliderProfile4f = std::array<SliderProfile<float>, 4>;

using SliderProfile1i = SliderProfile<int>;
using SliderProfile2i = std::array<SliderProfile<int>, 2>;
using SliderProfile3i = std::array<SliderProfile<int>, 3>;
using SliderProfile4i = std::array<SliderProfile<int>, 4>;

/**
 * Creating a slider control for one float value
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider1f(const HXString &Title, float &Value, SliderProfile1f &Profile);

/**
 * Creating a slider control for two float values
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider2f(const HXString &Title, float (&Value)[2], SliderProfile2f &Profile);

/**
 * Creating a slider control for three float values
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider3f(const HXString &Title, float (&Value)[3], SliderProfile3f &Profile);

/**
 * Creating a slider control for four float values
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider4f(const HXString &Title, float (&Value)[4], SliderProfile4f &Profile);

/**
 * Creating a slider control for one int value
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider1i(const HXString &Title, int &Value, SliderProfile1i &Profile);

/**
 * Creating a slider control for two int values
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider2i(const HXString &Title, int (&Value)[2], SliderProfile2i &Profile);

/**
 * Creating a slider control for three int values
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider3i(const HXString &Title, int (&Value)[3], SliderProfile3i &Profile);

/**
 * Creating a slider control for four int values
 * @param Title The title of the slider
 * @param Value The value store of the slider
 * @param Profile THe profile of the slider
 */
void Slider4i(const HXString &Title, int (&Value)[4], SliderProfile4i &Profile);
}
