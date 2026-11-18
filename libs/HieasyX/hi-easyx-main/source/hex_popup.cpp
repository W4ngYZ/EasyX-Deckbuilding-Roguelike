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
 * \file hex_popup.cpp
 * \brief Popup menu implementation
 */

#include <include/hex.h>
#include <include/hex_popup.h>

#undef max
#undef min

namespace HX {

struct PopupMenuOverlay {
	HXRect                     MenuRect;
	std::vector<PopupMenuItem> Items;
	HXGInt                     HoveredItem;
	HXGInt                     Width;
	int                        ItemHeight;
	int                        SeparatorHeight;
	int                        Padding;
	int                        FontSize;
	HXColor                    BackgroundColor;
	HXColor                    BorderColor;
	HXColor                    TextColor;
};

static std::vector<PopupMenuOverlay> G_PendingPopupMenus;
static int                        G_OpenPopupMenuCount = 0;

void OpenPopupMenu(PopupMenuProfile &Profile, HXPoint ScreenPos) {
	if (!Profile.Open) {
		++G_OpenPopupMenuCount;
	}
	Profile.Position   = ScreenPos;
	Profile.Open       = true;
	Profile._hoveredItem = -1;
}

void ClosePopupMenu(PopupMenuProfile &Profile) {
	if (Profile.Open) {
		--G_OpenPopupMenuCount;
	}
	Profile.Open       = false;
	Profile._hoveredItem = -1;
}

bool IsPopupMenuOpen(PopupMenuProfile &Profile) {
	return Profile.Open;
}

bool IsAnyPopupMenuOpen() {
	return G_OpenPopupMenuCount > 0;
}

HXGInt PopupMenu(PopupMenuProfile &Profile) {
	if (!Profile.Open) {
		return -1;
	}

	auto &context = GetContext();
	auto  painter = context.RenderContext->DefaultPainter();

	// Compute total height
	HXGInt totalHeight = 0;
	for (auto &item : Profile.Items) {
		totalHeight += item.Separator ? Profile.SeparatorHeight : Profile.ItemHeight;
	}

	HXRect menuRect{
		Profile.Position.X,
		Profile.Position.Y,
		Profile.Position.X + Profile.Width,
		Profile.Position.Y + totalHeight
	};

	// Clamp to screen bounds (best-effort using window size)
	if (menuRect.Left < 0) {
		menuRect.Right -= menuRect.Left;
		menuRect.Left   = 0;
	}
	if (menuRect.Top < 0) {
		menuRect.Bottom -= menuRect.Top;
		menuRect.Top     = 0;
	}
	// We don't know exact screen size, but we can clamp to a reasonable upper bound
	// based on the current window to avoid menus going completely off-screen.
	auto &win = *context.CurrentWindow;
	HXGInt maxRight  = win.Where.X + win.Size.X;
	HXGInt maxBottom = win.Where.Y + win.Size.Y;
	if (menuRect.Right > maxRight) {
		menuRect.Left -= (menuRect.Right - maxRight);
		menuRect.Right = maxRight;
	}
	if (menuRect.Bottom > maxBottom) {
		menuRect.Top -= (menuRect.Bottom - maxBottom);
		menuRect.Bottom = maxBottom;
	}
	if (menuRect.Left < 0) menuRect.Left = 0;
	if (menuRect.Top < 0)  menuRect.Top  = 0;

	PushFocusScope(&Profile, HXStr("popup_menu"));

	HXGInt clickedItem = -1;
	Profile._hoveredItem = -1;

	// Only the topmost focus scope may capture outside messages and process input.
	if (IsFocusScopeTop(&Profile)) {
		// Reclaim processed messages that fall inside our menu rect.
		// Controls underneath (e.g. TreeView) may have marked them processed
		// before the popup had a chance to handle them.
		for (auto &msg : context.MessageQuery) {
			if (!msg.MouseAction) continue;
			HXPoint mouse{msg.MouseX, msg.MouseY};
			if (menuRect.PointInside(mouse)) {
				msg.Processed = false;
			}
		}

		// Click outside -> close
		for (auto &msg : context.MessageQuery) {
			if (!msg.MouseAction) continue;
			if (msg.Processed) continue;
			HXPoint mouse{msg.MouseX, msg.MouseY};
			if (!menuRect.PointInside(mouse)) {
				if (msg.MouseLeftPressed || msg.MouseRightPressed) {
					msg.Processed = true;
					ClosePopupMenu(Profile);
					PopFocusScope();
					return -1;
				}
				msg.Processed = true;
			}
		}

		// Detect hovered / clicked item
		HXGInt currentY = menuRect.Top;
		for (HXGInt i = 0; i < static_cast<HXGInt>(Profile.Items.size()); ++i) {
			auto &item = Profile.Items[i];
			if (item.Separator) {
				currentY += Profile.SeparatorHeight;
				continue;
			}

			HXRect itemRect{menuRect.Left, currentY, menuRect.Right, currentY + Profile.ItemHeight};

			for (auto &msg : context.MessageQuery) {
				if (!msg.MouseAction) continue;
				if (msg.Processed) continue;
				HXPoint mouse{msg.MouseX, msg.MouseY};
				if (itemRect.PointInside(mouse)) {
					Profile._hoveredItem = i;
					if (msg.MouseLeftPressed && item.Enabled) {
						msg.Processed = true;
						clickedItem   = i;
					}
				}
			}
			currentY += Profile.ItemHeight;
		}
	}

	// Record overlay for deferred rendering (after all windows)
	G_PendingPopupMenus.push_back({menuRect, Profile.Items, Profile._hoveredItem, Profile.Width,
	                               Profile.ItemHeight, Profile.SeparatorHeight, Profile.Padding, Profile.FontSize,
	                               Profile.BackgroundColor, Profile.BorderColor, Profile.TextColor});

	PopFocusScope();

	if (clickedItem >= 0) {
		ClosePopupMenu(Profile);
	}

	return clickedItem;
}

void RenderPendingPopupMenus(HXBufferPainter *Painter) {
	if (G_PendingPopupMenus.empty()) {
		return;
	}

	auto &theme = GetTheme();

	for (auto &overlay : G_PendingPopupMenus) {
		HXColor bgColor   = overlay.BackgroundColor.A > 0 ? overlay.BackgroundColor : theme.ElevatedBackground;
		HXColor borderColor = overlay.BorderColor.A > 0 ? overlay.BorderColor : theme.ElevatedBorder;
		HXColor textColorBase = overlay.TextColor.A > 0 ? overlay.TextColor : theme.PopupText;

		// Background + border
		Painter->DrawFilledRoundedRectangle(overlay.MenuRect, borderColor, bgColor, theme.RadiusXl);

		// Items
		HXGInt currentY = overlay.MenuRect.Top;
		for (HXGInt i = 0; i < static_cast<HXGInt>(overlay.Items.size()); ++i) {
			auto &item = overlay.Items[i];
			if (item.Separator) {
				HXGInt sepY = currentY + overlay.SeparatorHeight / 2;
				Painter->DrawLine(
					{overlay.MenuRect.Left + 12, sepY},
					{overlay.MenuRect.Right - 12, sepY},
					theme.BorderSubtle);
				currentY += overlay.SeparatorHeight;
				continue;
			}

			HXRect itemRect{overlay.MenuRect.Left + 6, currentY,
			                overlay.MenuRect.Right - 6, currentY + overlay.ItemHeight};

			// Hover background
			if (i == overlay.HoveredItem && item.Enabled) {
				Painter->DrawFilledRoundedRectangle(itemRect, theme.HoverBackground, theme.HoverBackground, theme.RadiusMd);
			}

			// Label
			HXColor textColor = item.Enabled ? textColorBase : theme.DisabledText;
			int fontSize = overlay.FontSize > 0 ? overlay.FontSize : theme.FontSizeBody;
			Painter->DrawText(item.Label, HXFont{},
			                  {itemRect.Left + 8, itemRect.Top + (overlay.ItemHeight - fontSize) / 2},
			                  textColor, fontSize);

			currentY += overlay.ItemHeight;
		}
	}

	G_PendingPopupMenus.clear();
}

}
