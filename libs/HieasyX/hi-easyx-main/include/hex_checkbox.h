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
 * \file hex_checkbox.h
 * \brief The checkbox for HiEasyX
 */
 
#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {
/**
 * The profile for checkbox
 */
struct CheckboxProfile {
	bool Checked = false;
	bool OnFocus = false;

	// Customizable rendering / input
	int  BoxSize  = 22;
	int  FontSize = 20;
	bool Disabled = false;
};

/**
 * Creating a checkbox control
 * @param Title The title of the checkbox
 * @param Profile The profile for the checkbox
 * @return Returning true if the checkbox is selected
 */
bool Checkbox(const HXString &Title, CheckboxProfile &Profile);
}