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
 * \file hex_tab.h
 * \brief Tab bar and tab items
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {

/**
 * Profile for a tab bar
 */
struct TabBarProfile {
	HXGInt ActiveTab = 0;

	int FontSize = 20;
	int TabMinHeight = 28;
	int HorizontalPadding = 16;
	int VerticalPadding = 6;
	int CloseButtonWidth = 20;
	bool KeyboardSwitching = false;
};

/**
 * Profile for a single tab item
 */
struct TabItemProfile {
	bool Active = false;
};

/**
 * Begin a tab bar.
 * @param Id Unique identifier for state persistence
 * @param Profile Tab bar state
 * @return True if the tab bar is visible
 */
bool BeginTabBar(const HXString &Id, TabBarProfile &Profile);

/**
 * End a tab bar.
 */
void EndTabBar();

/**
 * Begin a tab item. Only returns true if this tab is currently active.
 * @param Title The label shown on the tab
 * @param Profile Tab item state
 * @return True if this tab is selected (render your content inside the if-block)
 */
bool BeginTabItem(const HXString &Title, TabItemProfile &Profile);

/**
 * End a tab item.
 */
void EndTabItem();

}
