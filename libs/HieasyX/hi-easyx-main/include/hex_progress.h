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
 * \file hex_progress.h
 * \brief Progress bar control
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {

struct ProgressBarProfile {
	int FontSize = 14;
	int Height = 6;
	HXColor FillColor = {0, 0, 0, 0};
	HXColor TrackColor = {0, 0, 0, 0};
	HXColor BorderColor = {0, 0, 0, 0};
};

/**
 * Draw a progress bar.
 * @param Fraction Value between 0.0 and 1.0
 * @param Size Total size of the bar. X=-1 means auto-fill available width.
 */
void ProgressBar(float Fraction, HXPoint Size = {-1, 22});
void ProgressBar(float Fraction, HXPoint Size, ProgressBarProfile &Profile);

/**
 * Draw a labeled progress bar.
 * @param Title Label shown to the left of the bar
 * @param Fraction Value between 0.0 and 1.0
 * @param Size Total size of the bar area
 */
void ProgressBar(const HXString &Title, float Fraction, HXPoint Size = {-1, 22});
void ProgressBar(const HXString &Title, float Fraction, HXPoint Size, ProgressBarProfile &Profile);

}
