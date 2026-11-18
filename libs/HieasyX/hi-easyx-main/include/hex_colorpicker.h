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
 * \file hex_colorpicker.h
 * \brief Color picker control API
 */


#pragma once

#include <include/hex.h>
#include <include/hex_slider.h>

namespace HX {

struct ColorPickerProfile {
	HXColor Color = {255, 255, 255, 255};
	HXPoint Size = {180, 220};
	HXColor CursorColor{255, 255, 255, 255};
	HXColor PreviewBorderColor{200, 200, 200, 255};

	// Internal
	struct {
		SliderProfile1f R{};
		SliderProfile1f G{};
		SliderProfile1f B{};
		float r = 255, g = 255, b = 255;
	} _internal;
};

/**
 * RGB color picker with sliders and preview box.
 * @return true if the color changed this frame.
 */
bool ColorPicker(const HXString &id, ColorPickerProfile &profile);

} // namespace HX
