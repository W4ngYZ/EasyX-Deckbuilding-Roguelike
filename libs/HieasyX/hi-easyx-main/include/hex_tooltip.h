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
 * \file hex_tooltip.h
 * \brief Tooltip that follows the mouse cursor
 */

#pragma once

#include <include/hex_string.h>

namespace HX {

struct TooltipProfile {
	int ShowDelayMs = 200;
	int Padding = 6;
	int OffsetXY = 15;
	HXColor BackgroundColor = {0, 0, 0, 0};
	HXColor BorderColor = {0, 0, 0, 0};
	HXColor TextColor = {0, 0, 0, 0};
	int FontSize = 14;
};

/**
 * Get the global tooltip profile for customization.
 */
TooltipProfile &GetTooltipProfile();

/**
 * Display a tooltip with the given text.
 * Call this immediately after the control you want to annotate.
 * The tooltip will appear near the mouse cursor after a short delay.
 * @param Text The tooltip content
 */
void Tooltip(const HXString &Text);

/**
 * Internal: render the pending tooltip at the end of the frame.
 */
void RenderTooltip();

/**
 * Internal: reset per-frame tooltip tracking.
 */
void ResetTooltipFrame();

}
