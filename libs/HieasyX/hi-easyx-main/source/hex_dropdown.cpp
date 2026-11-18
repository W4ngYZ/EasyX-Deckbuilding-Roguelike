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
 * \file hex_dropdown.cpp
 * \brief The dropdown selector for HiEasyX
 */

#include <include/hex.h>
#include <include/hex_dropdown.h>

#undef max
#undef min

namespace HX {

struct DropdownOverlay {
	HXRect   ListRect;
	HXGInt   ScrollOffset;
	HXGInt   SelectingItem;
	HXGInt   HoveredItem;
	ItemCollection Items;
	HXGInt   ItemHeight;
	HXGInt   MaxVisibleItems;
	int      FontSize;
	std::function<void(HXBufferPainter* p, const HXRect& itemRect, const HXString& text, bool hovered)> RenderItemCallback;
};

static std::vector<DropdownOverlay> PendingOverlays;

static void RecordDropdownOverlay(const HXRect &ListRect, HXGInt ScrollOffset, HXGInt SelectingItem,
                                  HXGInt HoveredItem, const ItemCollection &Items,
                                  HXGInt ItemHeight, HXGInt MaxVisibleItems, int FontSize,
                                  const std::function<void(HXBufferPainter*, const HXRect&, const HXString&, bool)> &RenderItemCallback) {
	PendingOverlays.push_back(DropdownOverlay{ListRect, ScrollOffset, SelectingItem, HoveredItem, Items,
	                                          ItemHeight, MaxVisibleItems, FontSize, RenderItemCallback});
}

void RenderPendingDropdownOverlays(HXBufferPainter *Painter) {
	auto &theme = GetTheme();
	for (auto &overlay : PendingOverlays) {
		Painter->DrawFilledRoundedRectangle(overlay.ListRect, theme.ElevatedBorder, theme.ElevatedBackground, theme.RadiusXl);

		HXGInt totalItems   = static_cast<HXGInt>(overlay.Items.size());
		if (totalItems == 0) {
			continue;
		}
		HXGInt firstVisible = overlay.ScrollOffset / overlay.ItemHeight;
		HXGInt offset       = overlay.ScrollOffset % overlay.ItemHeight;
		if (firstVisible < 0) firstVisible = 0;
		if (offset < 0) offset = 0;

		for (HXGInt i = firstVisible;
		     i < totalItems && i < firstVisible + overlay.MaxVisibleItems + (offset > 0 ? 1 : 0);
		     ++i) {
			HXGInt itemTop    = overlay.ListRect.Top + (i - firstVisible) * overlay.ItemHeight - offset;
			HXGInt itemBottom = itemTop + overlay.ItemHeight;
			auto   itemRect   = HXRect{overlay.ListRect.Left + 4, itemTop, overlay.ListRect.Right - 4, itemBottom};

			bool isSeparator = overlay.Items[i].Title.empty();
			if (isSeparator) {
				HXGInt sepY = itemTop + overlay.ItemHeight / 2;
				Painter->DrawLine({overlay.ListRect.Left + 12, sepY}, {overlay.ListRect.Right - 12, sepY}, theme.BorderSubtle);
				continue;
			}

			if (i == overlay.SelectingItem) {
				Painter->DrawFilledRoundedRectangle(itemRect,
				                             theme.SelectedBorder, theme.SelectedBackground, theme.RadiusMd);
			} else if (i == overlay.HoveredItem) {
				Painter->DrawFilledRoundedRectangle(itemRect,
				                             {0,0,0,0}, theme.HoverBackground, theme.RadiusMd);
			}

			auto textColor = (i == overlay.SelectingItem) ? theme.SelectedText : theme.DropdownText;
			if (overlay.RenderItemCallback) {
				overlay.RenderItemCallback(Painter, itemRect, overlay.Items[i].Title, i == overlay.HoveredItem);
			} else {
				Painter->DrawText(overlay.Items[i].Title, HXFont{},
				                  {itemRect.Left + 8, itemRect.Top + (overlay.ItemHeight - overlay.FontSize) / 2},
				                  textColor, overlay.FontSize);
			}
		}
	}
	PendingOverlays.clear();
}

HXGInt Dropdown(DropdownProfile &Profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto position = CurrentLayoutPosition();

	bool visible = true;

	if (RoughVisibility()) {
		return -1;
	}
	if (AccVisibility({Profile.Width, 22})) {
		visible = false;
	}

	bool returnSameLine = false;
	if (CurrentLayout()->InSameLine) {
		returnSameLine = true;
		EndSameLine();
	}

	auto painter          = CurrentPainter();
	auto buttonRectangle  = HXRect{position.X, position.Y, position.X + Profile.Width, position.Y + 22};
	auto triggerRectangle = HXRect{position.X, position.Y, position.X + Profile.Width, position.Y + 22 + 105};

	if (visible) {
		RegisterFocus(&Profile);
		Profile.OnFocus = IsFocused(&Profile);

		// Keyboard activation when focused
		if (Profile.OnFocus) {
			for (auto &Message : context.MessageQuery) {
				if (Message.Processed) {
					continue;
				}
				if (Message.KeyPressed && (Message.KeyCode == VK_SPACE || Message.KeyCode == VK_RETURN)) {
					Message.Processed = true;
					Profile.OnWorking = !Profile.OnWorking;
				}
				if (Message.KeyPressed && Message.KeyCode == VK_ESCAPE && Profile.OnWorking) {
					Message.Processed = true;
					Profile.OnWorking = false;
				}
				if (Profile.OnWorking) {
					if (Message.KeyPressed && Message.KeyCode == VK_DOWN) {
						Message.Processed = true;
						if (Profile.SelectingItem < static_cast<HXGInt>(Profile.Items.size()) - 1) {
							Profile.SelectingItem++;
						}
					}
					if (Message.KeyPressed && Message.KeyCode == VK_UP) {
						Message.Processed = true;
						if (Profile.SelectingItem > 0) {
							Profile.SelectingItem--;
						}
					}
				}
			}
		}

		// Mouse handling for trigger button
		for (auto &Message : context.MessageQuery) {
			auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
			if (Message.Processed) {
				continue;
			}
			if (Message.MouseAction) {
				Profile.OnHover = buttonRectangle.PointInside(mouse);
				if (buttonRectangle.PointInside(mouse) && Message.MouseLeftPressed) {
					Message.Processed = true;
					context.FocusedProfile = &Profile;
					Profile.OnWorking = !Profile.OnWorking;
				}
			}
		}

		painter->Begin();
		if (Profile.OnHover) {
			painter->DrawFilledRoundedRectangle(buttonRectangle,
			                             theme.DropdownBorder, theme.HoverBackground, theme.RadiusMd);
		} else if (Profile.OnFocus) {
			painter->DrawFilledRoundedRectangle(buttonRectangle,
			                             theme.SelectedBorder, theme.DropdownBackground, theme.RadiusMd);
		} else {
			painter->DrawFilledRoundedRectangle(buttonRectangle,
			                             theme.DropdownBorder, theme.DropdownBackground, theme.RadiusMd);
		}

		if (Profile.SelectingItem >= 0 && Profile.SelectingItem < static_cast<HXGInt>(Profile.Items.size())) {
			painter->DrawText(Profile.Items[Profile.SelectingItem].Title, HXFont{}, {
								  position.X + 8,
								  position.Y + 5}, theme.DropdownText, Profile.FontSize);
		}

		// Dropdown arrow icon on the right side
		if (Profile.OnWorking) {
			DrawDropdownIcon(painter, {position.X + Profile.Width - 16, position.Y + 7}, 8, false, theme.DropdownText);
		} else {
			DrawDropdownIcon(painter, {position.X + Profile.Width - 16, position.Y + 7}, 8, true, theme.DropdownText);
		}

		painter->End();

		LayoutNextControl({Profile.Width, 22});
	}

	// Dropdown list
	if (Profile.OnWorking) {
		HXGInt totalItems   = static_cast<HXGInt>(Profile.Items.size());
		HXGInt visibleItems = std::min(totalItems, Profile.MaxVisibleItems);
		HXGInt listHeight   = visibleItems * Profile.ItemHeight;

		auto listRect = HXRect{position.X, position.Y + 22,
		                       position.X + Profile.Width, position.Y + 22 + listHeight};

		// Clamp scroll offset
		HXGInt maxScroll = std::max(0, static_cast<int>((totalItems - Profile.MaxVisibleItems) * Profile.ItemHeight));
		if (Profile.ScrollOffset < 0) {
			Profile.ScrollOffset = 0;
		}
		if (Profile.ScrollOffset > maxScroll) {
			Profile.ScrollOffset = maxScroll;
		}

		PushFocusScope(&Profile, HXStr("dropdown"));

		// Only the topmost focus scope processes dropdown input.
		if (IsFocusScopeTop(&Profile)) {
			// Mouse handling for list (still in scroller-local coordinates)
			for (auto &Message : context.MessageQuery) {
				auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
				if (Message.Processed) continue;
				if (Message.MouseLeftPressed) {
					if (listRect.PointInside(mouse)) {
						Message.Processed = true;
						int idx = static_cast<int>((mouse.Y - listRect.Top + Profile.ScrollOffset) / Profile.ItemHeight);
						if (idx >= 0 && idx < totalItems) {
							Profile.SelectingItem = idx;
							Profile.OnWorking     = false;
						}
					} else if (!buttonRectangle.PointInside(mouse)) {
						// Click outside closes dropdown
						Message.Processed = true;
						Profile.OnWorking = false;
					}
				}
				if (Message.MouseWheel && listRect.PointInside(mouse)) {
					Message.Processed = true;
					if (Message.MouseWheelUp) {
						Profile.ScrollOffset -= Profile.ItemHeight;
					} else {
						Profile.ScrollOffset += Profile.ItemHeight;
					}
				}
			}

			// Consume any remaining mouse messages that fall outside the dropdown
			// so they don't penetrate to controls underneath the open list.
			for (auto &Message : context.MessageQuery) {
				if (!Message.MouseAction) continue;
				if (Message.Processed) continue;
				auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
				if (!listRect.PointInside(mouse) && !buttonRectangle.PointInside(mouse)) {
					Message.Processed = true;
				}
			}
		}

		// Re-clamp after wheel events may have pushed ScrollOffset out of bounds
		if (Profile.ScrollOffset < 0) {
			Profile.ScrollOffset = 0;
		}
		if (Profile.ScrollOffset > maxScroll) {
			Profile.ScrollOffset = maxScroll;
		}

		// Determine hovered item for deferred rendering (even when not top scope,
		// so the visual state is correct; input is blocked by Processed flags).
		HXGInt hoveredItem = -1;
		for (auto &msg : context.MessageQuery) {
			auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
			if (listRect.PointInside(mouse)) {
				int idx = static_cast<int>((mouse.Y - listRect.Top + Profile.ScrollOffset) / Profile.ItemHeight);
				if (idx >= 0 && idx < totalItems) {
					hoveredItem = idx;
				}
			}
		}

		PopFocusScope();

		// Record overlay for deferred rendering (after window compositing)
		HXGInt absX = position.X + context.CurrentWindow->ScrollerX + context.CurrentWindow->Where.X;
		HXGInt absY = position.Y + 22 + context.CurrentWindow->ScrollerY + context.CurrentWindow->Where.Y;
		RecordDropdownOverlay({absX, absY, absX + Profile.Width, absY + listHeight},
		                      Profile.ScrollOffset, Profile.SelectingItem, hoveredItem, Profile.Items,
		                      Profile.ItemHeight, Profile.MaxVisibleItems, Profile.FontSize,
		                      Profile.RenderItemCallback);
	} else {
		Profile.ScrollOffset = 0;
	}

	if (returnSameLine) {
		BeginSameLine();
	}

	return Profile.SelectingItem;
}

}
