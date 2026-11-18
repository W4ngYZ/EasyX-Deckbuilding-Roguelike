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
 * \file hex_dropdown.h
 * \brief The dropdown selector for HiEasyX
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

#include <functional>

namespace HX {
/**
 * The item of the dropdown
 */
struct Item {
	HXString Title;

	Item(HXString ITitle) : Title(ITitle) {}
};

using ItemCollection = std::vector<Item>;

/**
 * The profile for dropdown
 */
struct DropdownProfile {
	ItemCollection Items;
	HXGInt         SelectingItem = -1;
	bool           OnHover       = false;
	bool           OnWorking     = false;
	bool           OnFocus       = false;
	HXGInt         Width         = 150;
	HXGInt         ScrollOffset  = 0;

	// Customizable rendering
	int ItemHeight = 28;
	int MaxVisibleItems = 5;
	int FontSize = 16;
	std::function<void(HXBufferPainter* p, const HXRect& itemRect, const HXString& text, bool hovered)> RenderItemCallback;
};

/**
 * Creating a dropdown control, the same line layout is not available for dropdown control!
 * @param Profile The profile of the dropdown
 * @return The index of selecting item, if not any selection, it will return -1 or the last selection
 */
HXGInt Dropdown(DropdownProfile &Profile);

/**
 * Render any pending dropdown list overlays that were recorded during the frame.
 * Should be called after window compositing in Render().
 */
void RenderPendingDropdownOverlays(HXBufferPainter *Painter);
}
