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
 * \file hex_container.cpp
 * \brief Layout containers: Panel, Group, Horizontal, Separator, Spacer
 */

#include <include/hex.h>
#include <include/hex_container.h>

#include <algorithm>

#undef max
#undef min

namespace HX {

////////////////////////////////////////////////////////////
/// Next-item overrides

void SetNextItemSize(HXPoint Size) {
	auto &context = GetContext();
	context.NextItemSize    = Size;
	context.HasNextItemSize = true;
}

void SetNextItemWidth(HXGInt Width) {
	auto &context = GetContext();
	context.NextItemSize.X  = Width;
	context.HasNextItemSize = true;
}

void SetNextItemHeight(HXGInt Height) {
	auto &context = GetContext();
	context.NextItemSize.Y  = Height;
	context.HasNextItemSize = true;
}

void SetNextItemMargin(HXGInt Margin) {
	auto &context = GetContext();
	context.NextItemMargin = Margin;
}

void SetNextItemPadding(HXGInt Padding) {
	// Padding is a container property; we store it in the current layout for next container
	auto &context = GetContext();
	// No-op for now until next container reads it
}

////////////////////////////////////////////////////////////
/// Internal helper: consume next-item overrides and apply to a size

HXPoint ConsumeNextItemSize(HXPoint DefaultSize) {
	auto &context = GetContext();
	HXPoint result = DefaultSize;
	if (context.HasNextItemSize) {
		if (context.NextItemSize.X >= 0) {
			result.X = context.NextItemSize.X;
		}
		if (context.NextItemSize.Y >= 0) {
			result.Y = context.NextItemSize.Y;
		}
		context.HasNextItemSize = false;
		context.NextItemSize    = {-1, -1};
	}
	return result;
}

////////////////////////////////////////////////////////////
/// Horizontal layout

void BeginHorizontal() {
	auto &context = GetContext();
	auto *parent  = CurrentLayout();

	auto position = CurrentLayoutPosition();

	auto *childLayout      = new HXLayoutContext();
	childLayout->BaseLine  = position.Y;
	childLayout->LeftPaddingValue = position.X;
	childLayout->LeftPadding      = position.X;
	childLayout->AvailableRegion  = parent->AvailableRegion;
	childLayout->InSameLine       = true;
	childLayout->LockingSameLine  = true;
	childLayout->Direction        = HXLayoutDirection::Horizontal;
	childLayout->ControlGap       = parent->ControlGap;
	childLayout->LeftMargin       = 0;

	context.LayoutStack.push_back(childLayout);
}

void EndHorizontal() {
	auto &context = GetContext();
	if (context.LayoutStack.size() <= 1) {
		return;
	}

	auto *childLayout = context.LayoutStack.back();

	// Calculate consumed width and height
	HXGInt consumedWidth  = childLayout->LeftPadding - childLayout->LeftPaddingValue;
	HXGInt consumedHeight = childLayout->CurrentHeightMax;
	if (consumedWidth > 0) {
		consumedWidth -= childLayout->ControlGap * 2;
		consumedWidth  = std::max(consumedWidth, static_cast<HXGInt>(0));
	}
	if (consumedHeight == 0 && childLayout->BaseLine > childLayout->LeftPaddingValue) {
		// Fallback: if no height recorded, estimate from baseline difference
	}

	delete childLayout;
	context.LayoutStack.pop_back();

	// Advance parent layout
	if (consumedWidth == 0 && consumedHeight == 0) {
		consumedHeight = 5; // minimum spacer for empty horizontal
	}
	LayoutNextControl({consumedWidth, consumedHeight});
}

////////////////////////////////////////////////////////////
/// Panel container

struct HXPanelNode {
	HXLayoutContext Layout;
	HXPanelNode *   Parent = nullptr;
};

bool BeginPanel(const HXString &Id, PanelProfile &Profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	if (RoughVisibility()) {
		return false;
	}

	auto position = CurrentLayoutPosition();

	// Apply next-item size override
	auto desiredSize = ConsumeNextItemSize(Profile.Size);
	if (desiredSize.X < 0) {
		desiredSize.X = context.CurrentWindow->Size.X - position.X - 10;
	}
	if (desiredSize.Y < 0) {
		desiredSize.Y = 100;
	}
	Profile.Size = desiredSize;

	auto panelRect = HXRect{position.X, position.Y, position.X + desiredSize.X, position.Y + desiredSize.Y};

	if (AccVisibility(desiredSize)) {
		return false;
	}

	// Draw panel background / border (through deferred painter)
	auto *painter = CurrentPainter();
	painter->Begin();
	if (Profile.BackgroundColor.A > 0) {
		painter->DrawFilledRectangle(panelRect, Profile.BackgroundColor, Profile.BackgroundColor);
	}
	if (Profile.BorderColor.A > 0 && Profile.BorderThickness > 0) {
		HXLineStyle borderStyle;
		borderStyle.Thickness = static_cast<HXColInt>(Profile.BorderThickness);
		painter->DrawLineStyled({panelRect.Left, panelRect.Top}, {panelRect.Right, panelRect.Top}, Profile.BorderColor, borderStyle);
		painter->DrawLineStyled({panelRect.Right, panelRect.Top}, {panelRect.Right, panelRect.Bottom}, Profile.BorderColor, borderStyle);
		painter->DrawLineStyled({panelRect.Right, panelRect.Bottom}, {panelRect.Left, panelRect.Bottom}, Profile.BorderColor, borderStyle);
		painter->DrawLineStyled({panelRect.Left, panelRect.Bottom}, {panelRect.Left, panelRect.Top}, Profile.BorderColor, borderStyle);
	}
	painter->End();

	// Setup child layout
	auto *parentLayout = CurrentLayout();
	auto *childLayout  = new HXLayoutContext();
	childLayout->BaseLine         = position.Y + Profile.Padding;
	childLayout->StartBaseLine    = position.Y + Profile.Padding;
	childLayout->LeftPaddingValue = position.X + Profile.Padding;
	childLayout->StartLeftPadding = position.X + Profile.Padding;
	childLayout->LeftPadding      = position.X + Profile.Padding;
	childLayout->AvailableRegion  = {
		position.X + Profile.Padding,
		position.Y + Profile.Padding,
		position.X + desiredSize.X - Profile.Padding,
		position.Y + desiredSize.Y - Profile.Padding
	};
	childLayout->Direction    = Profile.Direction;
	childLayout->ItemAlign    = Profile.ItemAlign;
	childLayout->ControlGap   = parentLayout->ControlGap;
	childLayout->LeftMargin   = 0;

	context.LayoutStack.push_back(childLayout);

	return true;
}

void EndPanel(PanelProfile &Profile) {
	auto &context = GetContext();
	if (context.LayoutStack.size() <= 1) {
		return;
	}

	auto *childLayout = context.LayoutStack.back();

	delete childLayout;
	context.LayoutStack.pop_back();

	LayoutNextControl(Profile.Size);
}

////////////////////////////////////////////////////////////
/// Group container

bool BeginGroup(const HXString &Title, GroupProfile &Profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	if (RoughVisibility()) {
		return false;
	}

	auto position = CurrentLayoutPosition();
	auto *painter = CurrentPainter();

	// Measure title
	auto titleRect = painter->MeasureText(Title, HXFont{}, Profile.FontSize);
	auto titleBarHeight = Profile.TitleBarHeight;

	auto desiredSize = ConsumeNextItemSize(Profile.Size);
	if (desiredSize.X < 0) {
		desiredSize.X = context.CurrentWindow->Size.X - position.X - 10;
	}
	if (desiredSize.Y < 0) {
		desiredSize.Y = 100;
	}
	Profile.Size = desiredSize;

	auto groupRect = HXRect{position.X, position.Y, position.X + desiredSize.X, position.Y + desiredSize.Y};

	if (AccVisibility(desiredSize)) {
		return false;
	}

	// Draw group box and title
	painter->Begin();
	painter->DrawFilledRectangle(groupRect, theme.WindowBackground, theme.WindowBackground);
	painter->DrawFilledRectangle({groupRect.Left, groupRect.Top, groupRect.Right, groupRect.Top + titleBarHeight},
	                             theme.WindowTitleBackground, theme.WindowTitleBackground);
	painter->DrawText(Title, HXFont{}, {position.X + 8, position.Y + 3}, theme.WindowTitleText, Profile.FontSize);
	painter->End();

	// Setup child layout
	auto *parentLayout = CurrentLayout();
	auto *childLayout  = new HXLayoutContext();
	childLayout->BaseLine         = position.Y + titleBarHeight + Profile.Padding;
	childLayout->StartBaseLine    = position.Y + titleBarHeight + Profile.Padding;
	childLayout->LeftPaddingValue = position.X + Profile.Padding;
	childLayout->StartLeftPadding = position.X + Profile.Padding;
	childLayout->LeftPadding      = position.X + Profile.Padding;
	childLayout->AvailableRegion  = {
		position.X + Profile.Padding,
		position.Y + titleBarHeight + Profile.Padding,
		position.X + desiredSize.X - Profile.Padding,
		position.Y + desiredSize.Y - Profile.Padding
	};
	childLayout->Direction    = Profile.Direction;
	childLayout->ItemAlign    = Profile.ItemAlign;
	childLayout->ControlGap   = parentLayout->ControlGap;
	childLayout->LeftMargin   = 0;

	context.LayoutStack.push_back(childLayout);

	return true;
}

void EndGroup(GroupProfile &Profile) {
	auto &context = GetContext();
	if (context.LayoutStack.size() <= 1) {
		return;
	}

	auto *childLayout = context.LayoutStack.back();

	delete childLayout;
	context.LayoutStack.pop_back();

	LayoutNextControl(Profile.Size);
}

////////////////////////////////////////////////////////////
/// Separator and Spacer

void Separator() {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	if (RoughVisibility()) {
		return;
	}

	auto position = CurrentLayoutPosition();
	auto *painter = CurrentPainter();

	auto layout   = CurrentLayout();
	HXGInt width  = layout->AvailableRegion.Right - layout->AvailableRegion.Left;
	HXGInt height = 1;

	if (AccVisibility({width, 5})) {
		return;
	}

	painter->Begin();
	painter->DrawLine({position.X, position.Y + 2}, {position.X + width, position.Y + 2}, theme.SplitLine);
	painter->End();

	LayoutNextControl({width, 5});
}

void Spacer(HXPoint Size) {
	auto &context = GetContext();

	if (RoughVisibility()) {
		return;
	}

	auto layout = CurrentLayout();
	if (layout->Direction == HXLayoutDirection::Horizontal) {
		if (Size.X <= 0) {
			Size.X = 5;
		}
		LayoutNextControl({Size.X, 1});
	} else {
		if (Size.Y <= 0) {
			Size.Y = 5;
		}
		LayoutNextControl({1, Size.Y});
	}
}

}
