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
 * \file hex_tab.cpp
 * \brief Tab bar implementation
 */

#include <include/hex.h>
#include <include/hex_tab.h>

#include <vector>

namespace HX {

struct HXTabBarState {
	HXGInt              CurrentIndex = 0;
	std::vector<HXRect> TabRects;
	HXPoint             ContentPos{0, 0};
	HXPoint             ContentSize{0, 0};
	bool                InBar      = false;
	TabBarProfile *     Profile    = nullptr;
};

static HXTabBarState GTabBarState;

bool BeginTabBar(const HXString &Id, TabBarProfile &Profile) {
	if (RoughVisibility()) {
		return false;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();

	auto position = CurrentLayoutPosition();

	GTabBarState.CurrentIndex = 0;
	GTabBarState.TabRects.clear();
	GTabBarState.ContentPos   = position;
	GTabBarState.InBar        = true;
	GTabBarState.Profile      = &Profile;

	// Advance layout to reserve tab bar row space (height will be determined by tabs)
	LayoutNextControl({0, 30});

	return true;
}

void EndTabBar() {
	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();

	if (!GTabBarState.InBar || GTabBarState.TabRects.empty()) {
		GTabBarState.InBar = false;
		return;
	}

	// Draw underline for the whole tab row
	auto lastRect = GTabBarState.TabRects.back();
	auto lineY    = lastRect.Bottom;
	painter->Begin();
	painter->DrawLine({GTabBarState.TabRects.front().Left, lineY},
	                  {lastRect.Right, lineY}, theme.PanelBorder);
	painter->End();

	// Keyboard navigation
	if (GTabBarState.Profile && GTabBarState.Profile->KeyboardSwitching && GTabBarState.CurrentIndex > 0) {
		auto &context = GetContext();
		int tabCount = GTabBarState.CurrentIndex;
		for (auto &msg : context.MessageQuery) {
			if (msg.Processed || !msg.KeyPressed) continue;
			bool switched = false;
			if (msg.InCtrl && msg.KeyCode == 0x09) { // Ctrl+Tab
				GTabBarState.Profile->ActiveTab = (GTabBarState.Profile->ActiveTab + 1) % tabCount;
				switched = true;
			} else if (msg.KeyCode == 0x25) { // Left arrow
				GTabBarState.Profile->ActiveTab = (GTabBarState.Profile->ActiveTab - 1 + tabCount) % tabCount;
				switched = true;
			} else if (msg.KeyCode == 0x27) { // Right arrow
				GTabBarState.Profile->ActiveTab = (GTabBarState.Profile->ActiveTab + 1) % tabCount;
				switched = true;
			}
			if (switched) {
				msg.Processed = true;
			}
		}
	}

	GTabBarState.InBar = false;
}

bool BeginTabItem(const HXString &Title, TabItemProfile &Profile) {
	if (RoughVisibility() || !GTabBarState.InBar) {
		return false;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();

	auto &profile = *GTabBarState.Profile;
	auto textRect = painter->MeasureText(Title, HXFont{}, theme.FontSizeLabel);
	auto hPad = profile.HorizontalPadding;
	auto vPad = profile.VerticalPadding;

	HXGInt xPos = GTabBarState.ContentPos.X;
	if (!GTabBarState.TabRects.empty()) {
		xPos = GTabBarState.TabRects.back().Right + theme.Space2;
	}

	HXGInt yPos = GTabBarState.ContentPos.Y;
	auto tabRect = HXRect{xPos, yPos, xPos + textRect.Right + hPad * 2, yPos + textRect.Bottom + vPad * 2};
	if (tabRect.Bottom - tabRect.Top < profile.TabMinHeight) {
		tabRect.Bottom = tabRect.Top + profile.TabMinHeight;
	}

	bool hovered = false;
	bool clicked = false;
	for (auto &Message : context.MessageQuery) {
		auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
		if (Message.Processed) {
			continue;
		}
		if (Message.MouseAction && tabRect.PointInside(mouse)) {
			hovered = true;
			if (Message.MouseLeftPressed) {
				Message.Processed = true;
				clicked           = true;
				if (GTabBarState.Profile) {
					GTabBarState.Profile->ActiveTab = GTabBarState.CurrentIndex;
				}
			}
		}
	}

	bool active = false;
	if (GTabBarState.Profile) {
		active = (GTabBarState.CurrentIndex == GTabBarState.Profile->ActiveTab);
	}

	HXColor bgColor{0, 0, 0, 0};
	HXColor textColor = theme.DisabledText;
	HXColor indicatorColor{0, 0, 0, 0};

	if (active) {
		bgColor = theme.SelectedBackground;
		textColor = theme.SelectedText;
		indicatorColor = theme.SelectedBorder;
	} else if (hovered) {
		bgColor = theme.HoverBackground;
		textColor = theme.WindowTitleText;
	}

	painter->Begin();
	if (bgColor.A > 0) {
		painter->DrawFilledRoundedRectangle(tabRect, {0, 0, 0, 0}, bgColor, theme.RadiusMd);
	}
	painter->DrawText(Title, HXFont{}, {xPos + hPad, yPos + vPad + 2}, textColor, theme.FontSizeLabel);

	if (active) {
		int indicatorY = tabRect.Bottom - 2;
		painter->DrawFilledRectangle({tabRect.Left + 4, indicatorY, tabRect.Right - 4, indicatorY + 2}, {0, 0, 0, 0}, indicatorColor);
	}
	painter->End();

	GTabBarState.TabRects.push_back(tabRect);
	++GTabBarState.CurrentIndex;

	return active;
}

void EndTabItem() {
	// No-op for now; content is rendered by user inside the if(BeginTabItem) block
}

}
