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
 * \file hex_text.h
 * \brief The text for HiEasyX
 */
 
#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {
/**
 * The profile for a text label
 */
struct TextProfile {
	HXFont  Font;
	HXGInt  Height = 18;
	HXColor Color;

	TextProfile();
};

/**
 * Creating a text label, the text will automatically align to the center
 * (But not very perfect every time)
 * @param Title The title of the text
 * @param AutoAlign If it is true, it will enables the auto align of the text
 */
void Text(const HXString &Title, bool AutoAlign = true);

/**
 * Creating a text label with a text profile, the text will automatically align to the center
 * (But not very perfect every time)
 * @param Title The title of the text
 * @param Profile The pointer to the text profile
 * @param AutoAlign If it is true, it will enables the auto align of the text
 */
void Text(const HXString &Title, TextProfile &Profile, bool AutoAlign = true);
}