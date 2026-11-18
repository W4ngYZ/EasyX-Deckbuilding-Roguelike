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
 * \file hex_container.h
 * \brief Layout containers and helpers for complex UI composition
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {

/**
 * Profile for a Panel container (borderless sub-region with padding)
 */
struct PanelProfile {
	HXPoint          Size = {-1, -1};
	HXGInt           Padding = 5;
	HXGInt           Margin = 5;
	HXLayoutDirection Direction = HXLayoutDirection::Vertical;
	HXLayoutAlign     ItemAlign = HXLayoutAlign::Stretch;
	HXColor           BackgroundColor = HXColor{0, 0, 0, 0};
	HXColor           BorderColor = HXColor{0, 0, 0, 0};
	HXGInt            BorderThickness = 1;
	bool              ClipContent = false;
};

/**
 * Profile for a Group container (titled box with border)
 */
struct GroupProfile {
	HXPoint           Size = {-1, -1};
	HXGInt            Padding = 8;
	HXGInt            Margin = 5;
	HXLayoutDirection Direction = HXLayoutDirection::Vertical;
	HXLayoutAlign     ItemAlign = HXLayoutAlign::Stretch;
	bool              ClipContent = false;

	int TitleBarHeight = 24;
	int FontSize       = 20;
};

/**
 * Set the size for the next item. {-1,-1} means auto.
 */
void SetNextItemSize(HXPoint Size);

/**
 * Set the width for the next item. -1 means auto.
 */
void SetNextItemWidth(HXGInt Width);

/**
 * Set the height for the next item. -1 means auto.
 */
void SetNextItemHeight(HXGInt Height);

/**
 * Set the margin (outer spacing) for the next item
 */
void SetNextItemMargin(HXGInt Margin);

/**
 * Set the padding (inner spacing) for the next container
 */
void SetNextItemPadding(HXGInt Padding);

/**
 * Begin a horizontal layout region.
 * All controls between BeginHorizontal and EndHorizontal will be laid out in a row.
 */
void BeginHorizontal();

/**
 * End a horizontal layout region.
 */
void EndHorizontal();

/**
 * Begin a Panel container.
 * @param Id Unique identifier for state persistence
 * @param Profile Panel appearance and layout settings
 * @return True if the panel is visible and not collapsed
 */
bool BeginPanel(const HXString &Id, PanelProfile &Profile);

/**
 * End a Panel container.
 */
void EndPanel(PanelProfile &Profile);

/**
 * Begin a Group container (titled box).
 * @param Title The title displayed at the top of the group
 * @param Profile Group appearance and layout settings
 * @return True if the group is visible and not collapsed
 */
bool BeginGroup(const HXString &Title, GroupProfile &Profile);

/**
 * End a Group container.
 */
void EndGroup(GroupProfile &Profile);

/**
 * Draw a horizontal separator line.
 */
void Separator();

/**
 * Insert empty space.
 * @param Size Width and height of the spacer. In vertical layout only Height matters;
 *             in horizontal layout only Width matters.
 */
void Spacer(HXPoint Size = {0, 5});

}
