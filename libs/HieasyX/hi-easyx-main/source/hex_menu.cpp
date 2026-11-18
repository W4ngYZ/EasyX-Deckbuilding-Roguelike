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
 * \file hex_menu.cpp
 * \brief Menu bar and dropdown menu implementation
 */

#include <include/hex.h>
#include <include/hex_menu.h>

namespace HX {

static MenuBarProfile G_MenuBarProfile;
static constexpr HXGInt MENU_HPADDING    = 12;

MenuBarProfile& GetMenuBarProfile() {
	return G_MenuBarProfile;
}

struct MenuItemInfo {
	HXString Label;
	bool     Enabled   = true;
	bool     Separator = false;
};

struct MenuDropdownOverlay {
	HXRect                     MenuRect;
	std::vector<MenuItemInfo>  Items;
	HXGInt                     HoveredItem = -1;
	HXGInt                     ClickedItem = -1;
};

static std::vector<MenuDropdownOverlay> G_PendingMenuDropdowns;
static HXString                         G_ActiveMenuLabel;
static bool                             G_IsInDropdown = false;
static HXPoint                          G_DropdownOrigin{0, 0};
static HXGInt                           G_DropdownWidth = 150;
static bool                             G_MenuBarAnyHovered = false;

// ─── Internal helpers ────────────────────────────────────────────────────

static bool MenuItemTopLevel(const HXString &Title, bool Selected, bool Enabled) {
	if (RoughVisibility()) {
		return false;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();
	auto  layout  = CurrentLayout();

	auto position = CurrentLayoutPosition();
	auto textRect = painter->MeasureText(Title, HXFont{}, G_MenuBarProfile.FontSize);

	constexpr HXGInt hPadding = 12;
	constexpr HXGInt vPadding = 4;
	auto itemRect = HXRect{position.X, position.Y,
	                       position.X + textRect.Right + hPadding * 2,
	                       position.Y + textRect.Bottom + vPadding * 2};

	if (itemRect.Bottom - itemRect.Top < G_MenuBarProfile.BarHeight) {
		itemRect.Bottom = itemRect.Top + G_MenuBarProfile.BarHeight;
	}

	if (AccVisibility({itemRect.CalWidth(), itemRect.CalHeight()})) {
		return false;
	}

	bool clicked = false;
	bool hovered = false;

	if (Enabled) {
		for (auto &Message : context.MessageQuery) {
			auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
			if (Message.Processed) {
				continue;
			}
			if (Message.MouseAction && itemRect.PointInside(mouse)) {
				hovered = true;
				if (Message.MouseLeftPressed) {
					Message.Processed = true;
					clicked           = true;
				}
			}
		}
	}

	if (hovered) G_MenuBarAnyHovered = true;

	painter->Begin();
	if (Selected) {
		painter->DrawFilledRectangle(itemRect, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
		painter->DrawText(Title, HXFont{}, {position.X + hPadding, position.Y + vPadding + 2},
		                  theme.ButtonOnHoverText, G_MenuBarProfile.FontSize);
	} else if (hovered && Enabled) {
		painter->DrawFilledRectangle(itemRect, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
		painter->DrawText(Title, HXFont{}, {position.X + hPadding, position.Y + vPadding + 2},
		                  theme.ButtonOnHoverText, G_MenuBarProfile.FontSize);
	} else if (!Enabled) {
		painter->DrawText(Title, HXFont{}, {position.X + hPadding, position.Y + vPadding + 2},
		                  theme.SplitLine, G_MenuBarProfile.FontSize);
	} else {
		painter->DrawText(Title, HXFont{}, {position.X + hPadding, position.Y + vPadding + 2},
		                  theme.WindowTitleText, G_MenuBarProfile.FontSize);
	}
	painter->End();

	LayoutNextControl({itemRect.CalWidth(), itemRect.CalHeight()});

	return clicked;
}

static bool MenuItemDropdown(const HXString &Title, bool Enabled) {
	auto &context = GetContext();
	auto  painter = CurrentPainter();

	auto &overlay   = G_PendingMenuDropdowns.back();
	HXGInt itemIndex = static_cast<HXGInt>(overlay.Items.size());

	// Update dropdown width based on this item's text
	auto textRect   = painter->MeasureText(Title, HXFont{}, G_MenuBarProfile.FontSize);
	HXGInt itemWidth = textRect.Right + MENU_HPADDING * 2;
	if (itemWidth > G_DropdownWidth) {
		G_DropdownWidth = itemWidth;
	}

	// Hit-test rect (uses current known width; may grow for later items)
	HXRect itemRect{
		G_DropdownOrigin.X,
		G_DropdownOrigin.Y + itemIndex * G_MenuBarProfile.ItemHeight,
		G_DropdownOrigin.X + G_DropdownWidth,
		G_DropdownOrigin.Y + (itemIndex + 1) * G_MenuBarProfile.ItemHeight
	};

	// Update overall menu rect
	overlay.MenuRect.Left   = G_DropdownOrigin.X;
	overlay.MenuRect.Top    = G_DropdownOrigin.Y;
	overlay.MenuRect.Right  = G_DropdownOrigin.X + G_DropdownWidth;
	overlay.MenuRect.Bottom = itemRect.Bottom;

	bool clicked = false;
	if (Enabled) {
		for (auto &msg : context.MessageQuery) {
			if (msg.Processed) continue;
			if (!msg.MouseAction) continue;
			HXPoint mouse{msg.MouseX, msg.MouseY};
			if (itemRect.PointInside(mouse)) {
				overlay.HoveredItem = itemIndex;
				if (msg.MouseLeftPressed) {
					msg.Processed = true;
					clicked = true;
					overlay.ClickedItem = itemIndex;
					G_ActiveMenuLabel.clear();
				}
			}
		}
	}

	overlay.Items.push_back(MenuItemInfo{Title, Enabled, false});
	return clicked;
}

// ─── Public API ──────────────────────────────────────────────────────────

void BeginMenuBar() {
	auto layout = CurrentLayout();
	// Keep current baseline so menu bar sits flush against the previous control / title bar
	layout->LeftPadding      = 5;
	layout->LeftPaddingValue = 5;
	BeginSameLine();
	G_PendingMenuDropdowns.clear();
	G_MenuBarAnyHovered = false;

	// Close menus on Escape
	auto &context = GetContext();
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed) continue;
		if (msg.KeyPressed && msg.KeyCode == VK_ESCAPE) {
			G_ActiveMenuLabel.clear();
			msg.Processed = true;
		}
	}
}

void EndMenuBar() {
	EndSameLine();
	auto layout = CurrentLayout();
	// Remove the trailing control-gap so the dockspace / next control sits flush
	if (layout->BaseLine >= layout->ControlGap) {
		layout->BaseLine -= layout->ControlGap;
	}
	layout->LeftPadding = layout->LeftPaddingValue;

	// Close menu if clicked outside any dropdown and outside menu-bar labels
	if (!G_ActiveMenuLabel.empty()) {
		auto &context = GetContext();
		for (auto &msg : context.MessageQuery) {
			if (!msg.MouseAction) continue;
			if (msg.Processed) continue;
			if (msg.MouseLeftPressed) {
				bool insideDropdown = false;
				for (auto &overlay : G_PendingMenuDropdowns) {
					HXPoint mouse{msg.MouseX, msg.MouseY};
					if (overlay.MenuRect.PointInside(mouse)) {
						insideDropdown = true;
						break;
					}
				}
				if (!insideDropdown && !G_MenuBarAnyHovered) {
					G_ActiveMenuLabel.clear();
				}
			}
		}
	}
}

bool BeginMenu(const HXString &Label) {
	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();
	auto  position = CurrentLayoutPosition();

	auto textRect = painter->MeasureText(Label, HXFont{}, G_MenuBarProfile.FontSize);
	constexpr HXGInt hPadding = 12;
	constexpr HXGInt vPadding = 4;
	auto itemRect = HXRect{position.X, position.Y,
	                       position.X + textRect.Right + hPadding * 2,
	                       position.Y + textRect.Bottom + vPadding * 2};
	if (itemRect.Bottom - itemRect.Top < G_MenuBarProfile.BarHeight) {
		itemRect.Bottom = itemRect.Top + G_MenuBarProfile.BarHeight;
	}

	bool clicked = false;
	bool hovered = false;
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed) continue;
		if (!msg.MouseAction) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (itemRect.PointInside(mouse)) {
			hovered = true;
			if (msg.MouseLeftPressed) {
				msg.Processed = true;
				clicked = true;
			}
		}
	}

	if (hovered) G_MenuBarAnyHovered = true;

	bool wasOpen = (G_ActiveMenuLabel == Label);
	bool isOpen  = wasOpen;

	if (clicked) {
		isOpen = !wasOpen;
	} else if (hovered && !G_ActiveMenuLabel.empty() && G_ActiveMenuLabel != Label) {
		isOpen = true;
	}

	if (isOpen && !wasOpen) {
		G_ActiveMenuLabel = Label;
	} else if (!isOpen && wasOpen) {
		G_ActiveMenuLabel.clear();
	}

	painter->Begin();
	if (isOpen || hovered) {
		painter->DrawFilledRectangle(itemRect, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
		painter->DrawText(Label, HXFont{}, {position.X + hPadding, position.Y + vPadding + 2},
		                  theme.ButtonOnHoverText, G_MenuBarProfile.FontSize);
	} else {
		painter->DrawText(Label, HXFont{}, {position.X + hPadding, position.Y + vPadding + 2},
		                  theme.WindowTitleText, G_MenuBarProfile.FontSize);
	}
	painter->End();

	LayoutNextControl({itemRect.CalWidth(), itemRect.CalHeight()});

	if (isOpen) {
		G_IsInDropdown   = true;
		G_DropdownWidth  = 150;
		G_DropdownOrigin = {
			position.X + context.CurrentWindow->ScrollerX + context.CurrentWindow->Where.X,
			itemRect.Bottom + context.CurrentWindow->ScrollerY + context.CurrentWindow->Where.Y
		};
		G_PendingMenuDropdowns.push_back(MenuDropdownOverlay{});
	}

	return isOpen;
}

void EndMenu() {
	G_IsInDropdown = false;
}

void MenuSeparator() {
	if (!G_IsInDropdown || G_PendingMenuDropdowns.empty()) return;
	auto &overlay = G_PendingMenuDropdowns.back();
	overlay.Items.push_back(MenuItemInfo{HXStr(""), true, true});
}

bool MenuItem(const HXString &Title, bool Selected, bool Enabled) {
	if (G_IsInDropdown) {
		return MenuItemDropdown(Title, Enabled);
	}
	return MenuItemTopLevel(Title, Selected, Enabled);
}

void RenderPendingMenuBarDropdowns(HXBufferPainter *Painter) {
	if (G_PendingMenuDropdowns.empty()) return;

	auto &theme = GetTheme();

	for (auto &overlay : G_PendingMenuDropdowns) {
		// Background + border
		Painter->DrawFilledRectangle(overlay.MenuRect, theme.DropdownBorder, theme.WindowBackground);
		Painter->DrawRectangle(overlay.MenuRect, theme.DropdownBorder);

		// Items
		HXGInt currentY = overlay.MenuRect.Top;
		for (HXGInt i = 0; i < static_cast<HXGInt>(overlay.Items.size()); ++i) {
			auto &item = overlay.Items[i];
			if (item.Separator) {
				HXGInt sepY = currentY + G_MenuBarProfile.SeparatorHeight / 2;
				Painter->DrawLine(
					{overlay.MenuRect.Left + 8, sepY},
					{overlay.MenuRect.Right - 8, sepY},
					theme.DropdownBorder);
				currentY += G_MenuBarProfile.SeparatorHeight;
				continue;
			}

			HXRect itemRect{overlay.MenuRect.Left, currentY, overlay.MenuRect.Right, currentY + G_MenuBarProfile.ItemHeight};

			// Hover background
			if (i == overlay.HoveredItem && item.Enabled) {
				Painter->DrawFilledRectangle(itemRect, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
			}

			// Label
			HXColor textColor = item.Enabled ? theme.TextInputText : theme.DropdownBorder;
			Painter->DrawText(item.Label, HXFont{}, {itemRect.Left + MENU_HPADDING, itemRect.Top + 5}, textColor, G_MenuBarProfile.FontSize);

			currentY += G_MenuBarProfile.ItemHeight;
		}
	}

	G_PendingMenuDropdowns.clear();
}

}
