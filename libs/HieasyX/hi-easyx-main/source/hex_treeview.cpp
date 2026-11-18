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
 * \file hex_treeview.cpp
 * \brief TreeView control implementation
 */

#include <include/hex_treeview.h>
#include <include/hex.h>

#include <algorithm>

#undef max
#undef min

namespace HX {

struct TreeViewState {
	int ScrollY = 0;
	bool InDrag = false;
	int DragStartY = 0;
	int ScrollStart = 0;
	int LastSelectedRow = -1;
};

struct FlatNode {
	HXTreeNode *Node;
	int         Level;
	HXString    Path;
};

static void FlattenTree(std::vector<HXTreeNode> &nodes, int level, const HXString &parentPath, std::vector<FlatNode> &out) {
	for (auto &node : nodes) {
		HXString path = parentPath.empty() ? node.Label : parentPath + HXStr("/") + node.Label;
		out.push_back({&node, level, path});
		if (node.Expanded) {
			FlattenTree(node.Children, level + 1, path, out);
		}
	}
}

void TreeView(const HXString &id, std::vector<HXTreeNode> &nodes, TreeViewProfile &profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();
	profile.Activated = false;
	profile.ContextMenuRequested = false;

	HXPoint size = profile.Size;
	auto *layout = CurrentLayout();
	if (layout) {
		if (size.X <= 0) {
			size.X = layout->AvailableRegion.Right - layout->AvailableRegion.Left;
			if (size.X <= 0) size.X = 200;
		}
		if (size.Y <= 0) {
			size.Y = layout->AvailableRegion.Bottom - layout->BaseLine;
			if (size.Y <= 0) size.Y = 200;
		}
	}

	if (RoughVisibility()) {
		LayoutNextControl(size);
		return;
	}

	auto position = CurrentLayoutPosition();
	if (AccVisibility({size.X, size.Y})) {
		LayoutNextControl(size);
		return;
	}

	auto *state = GetStatePool().Get<TreeViewState>(id);
	auto  painter = CurrentPainter();

	HXRect controlRect{position.X, position.Y, position.X + size.X, position.Y + size.Y};

	// Flatten visible tree
	std::vector<FlatNode> flat;
	FlattenTree(nodes, 0, HXStr(""), flat);

	int totalHeight = static_cast<int>(flat.size()) * profile.RowHeight;
	int maxScroll   = std::max(0, totalHeight - size.Y);
	state->ScrollY  = std::max(0, std::min(state->ScrollY, maxScroll));

	// ─── Scrollbar metrics ──────────────────────────────────────────────
	bool needScrollbar = totalHeight > size.Y;
	HXRect trackRect{0, 0, 0, 0};
	HXRect thumbRect{0, 0, 0, 0};
	if (needScrollbar) {
		trackRect = {position.X + size.X - profile.ScrollbarWidth, position.Y,
		             position.X + size.X, position.Y + size.Y};
		int trackH = trackRect.CalHeight();
		int thumbH = std::max(20, trackH * size.Y / totalHeight);
		int thumbY = position.Y + state->ScrollY * (trackH - thumbH) / maxScroll;
		thumbRect  = {trackRect.Left, thumbY, trackRect.Right, thumbY + thumbH};
	}

	// Handle wheel scrolling
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.MouseWheel) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (controlRect.PointInside(mouse)) {
			state->ScrollY += msg.MouseWheelUp ? profile.RowHeight : -profile.RowHeight;
			msg.Processed = true;
		}
	}

	// Handle scrollbar thumb drag
	if (needScrollbar) {
		for (auto &msg : context.MessageQuery) {
			if (!msg.MouseAction) continue;
			auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
			if (msg.MouseLeftPressed && thumbRect.PointInside(mouse)) {
				state->InDrag     = true;
				state->DragStartY = mouse.Y;
				state->ScrollStart = state->ScrollY;
				msg.Processed     = true;
			}
			if (state->InDrag && !msg.MouseLeftRelease) {
				int delta    = mouse.Y - state->DragStartY;
				int trackH   = trackRect.CalHeight();
				int thumbH   = thumbRect.CalHeight();
				state->ScrollY = state->ScrollStart + delta * maxScroll / std::max(1, trackH - thumbH);
				msg.Processed  = true;
			}
			if (msg.MouseLeftRelease) {
				state->InDrag = false;
			}
		}
	}

	state->ScrollY = std::max(0, std::min(state->ScrollY, maxScroll));

	if (state->LastSelectedRow >= static_cast<int>(flat.size())) {
		state->LastSelectedRow = static_cast<int>(flat.size()) - 1;
	}

	// Determine visible range
	int firstRow = state->ScrollY / profile.RowHeight;
	int lastRow  = std::min(static_cast<int>(flat.size()) - 1,
	                        firstRow + size.Y / profile.RowHeight + 1);

	// Draw background
	painter->Begin();
	painter->DrawFilledRoundedRectangle(controlRect, theme.PanelBorder, theme.PanelBackground, theme.RadiusLg);

	// Clip to control rect using a sub-region so rows don't spill out
	HXRect contentRect{position.X, position.Y, position.X + size.X - (needScrollbar ? profile.ScrollbarWidth : 0), position.Y + size.Y};

	// Draw visible rows
	for (int i = firstRow; i <= lastRow && i < static_cast<int>(flat.size()); ++i) {
		auto &fn   = flat[i];
		int   rowY = position.Y + i * profile.RowHeight - state->ScrollY;
		if (rowY + profile.RowHeight < position.Y || rowY > position.Y + size.Y) continue;

		HXRect rowRect{contentRect.Left, rowY, contentRect.Right, rowY + profile.RowHeight};

		// Hover detection (do not consume messages)
		bool hovered = false;
		for (auto &msg : context.MessageQuery) {
			if (msg.Processed) continue;
			auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
			if (msg.MouseAction && rowRect.PointInside(mouse)) {
				hovered = true;
				break;
			}
		}

		// Hover highlight
		if (!fn.Node->Selected && hovered) {
			HXRect hoverRect{contentRect.Left + 4, rowY, contentRect.Right - 4, rowY + profile.RowHeight};
			painter->DrawFilledRoundedRectangle(hoverRect, {0,0,0,0}, theme.HoverBackground, theme.RadiusMd);
		}

		// Selection highlight
		if (fn.Node->Selected) {
			painter->DrawFilledRectangle(rowRect, {0,0,0,0}, theme.SelectedBackground);
			painter->DrawFilledRectangle({rowRect.Left, rowRect.Top, rowRect.Left + 3, rowRect.Bottom}, {0,0,0,0}, theme.SelectedBorder);
		}

		// Expand/collapse arrow
		int iconX = contentRect.Left + 4 + fn.Level * profile.Indent;
		int iconY = rowY + profile.RowHeight / 2;
		if (!fn.Node->Children.empty()) {
			std::vector<HXPoint> arrow;
			if (fn.Node->Expanded) {
				// Down arrow (12px)
				arrow = {{iconX, iconY - 4}, {iconX + 12, iconY - 4}, {iconX + 6, iconY + 6}};
			} else {
				// Right arrow (12px)
				arrow = {{iconX, iconY - 6}, {iconX + 12, iconY}, {iconX, iconY + 6}};
			}
			painter->DrawFilledPolygon(arrow, theme.DisabledText, theme.DisabledText);
		}

		// Custom item rendering
		if (profile.RenderItemCallback) {
			HXRect rowRect{contentRect.Left, rowY, contentRect.Right, rowY + profile.RowHeight};
			profile.RenderItemCallback(painter, rowRect, *fn.Node);
		}

		// Label
		int textX = iconX + 20;
		painter->DrawText(fn.Node->Label, HXFont{}, {textX, rowY + 3}, theme.WindowTitleText, theme.FontSizeBody);
	}

	// Draw scrollbar
	if (needScrollbar) {
		// Track
		painter->DrawFilledRoundedRectangle(trackRect, {0,0,0,0}, theme.ScrollerBackground, theme.RadiusSm);
		// Thumb
		int trackH = trackRect.CalHeight();
		int thumbH = std::max(20, trackH * size.Y / totalHeight);
		int thumbY = position.Y + state->ScrollY * (trackH - thumbH) / maxScroll;
		thumbRect  = {trackRect.Left, thumbY, trackRect.Right, thumbY + thumbH};
		painter->DrawFilledRoundedRectangle(thumbRect, {0,0,0,0}, theme.HoverBackground, theme.RadiusSm);
	}

	painter->End();

	// Handle clicks (expand / select)
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || (!msg.MouseLeftPressed && !msg.MouseRightPressed)) continue;
		// If any popup menu is open, let the overlay handle input;
		// don't process messages here.
		if (IsAnyPopupMenuOpen()) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (!controlRect.PointInside(mouse)) continue;
		if (needScrollbar && thumbRect.PointInside(mouse)) continue;

		int rowIdx = (mouse.Y - position.Y + state->ScrollY) / profile.RowHeight;
		if (rowIdx >= 0 && rowIdx < static_cast<int>(flat.size())) {
			auto &fn = flat[rowIdx];

			if (msg.MouseRightPressed) {
				for (auto &n : flat) n.Node->Selected = false;
				fn.Node->Selected = true;
				state->LastSelectedRow = rowIdx;
				profile.ContextMenuRequested = true;
				profile.ContextMenuLabel = fn.Node->Label;
				profile.ContextMenuPath = fn.Path;
				profile.ContextMenuPos = {msg.MouseX, msg.MouseY};
				msg.Processed = true;
				continue;
			}

			// Check if expand arrow clicked
			int iconX = contentRect.Left + 4 + fn.Level * profile.Indent;
			HXRect arrowRect{iconX, position.Y + rowIdx * profile.RowHeight - state->ScrollY,
			                 iconX + 12, position.Y + rowIdx * profile.RowHeight - state->ScrollY + profile.RowHeight};
			if (!fn.Node->Children.empty() && arrowRect.PointInside(mouse)) {
				fn.Node->Expanded = !fn.Node->Expanded;
			} else {
				if (profile.MultiSelect && msg.InShift && state->LastSelectedRow >= 0) {
					int first = std::min(state->LastSelectedRow, rowIdx);
					int last  = std::max(state->LastSelectedRow, rowIdx);
					for (auto &n : flat) n.Node->Selected = false;
					for (int i = first; i <= last && i < static_cast<int>(flat.size()); ++i) {
						flat[i].Node->Selected = true;
					}
				} else if (profile.MultiSelect && msg.InCtrl) {
					fn.Node->Selected = !fn.Node->Selected;
					state->LastSelectedRow = rowIdx;
				} else {
					for (auto &n : flat) n.Node->Selected = false;
					fn.Node->Selected = true;
					state->LastSelectedRow = rowIdx;
				}
				if (fn.Node->Children.empty()) {
					profile.Activated = true;
					profile.ActivatedLabel = fn.Node->Label;
					profile.ActivatedPath = fn.Path;
				}
			}
			msg.Processed = true;
		}
	}

	// Keyboard navigation
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.KeyPressed) continue;
		if (msg.KeyCode == VK_UP) {
			if (state->LastSelectedRow > 0) {
				state->LastSelectedRow--;
				for (auto &n : flat) n.Node->Selected = false;
				flat[state->LastSelectedRow].Node->Selected = true;
				msg.Processed = true;
			}
		} else if (msg.KeyCode == VK_DOWN) {
			if (state->LastSelectedRow >= 0 && state->LastSelectedRow < static_cast<int>(flat.size()) - 1) {
				state->LastSelectedRow++;
				for (auto &n : flat) n.Node->Selected = false;
				flat[state->LastSelectedRow].Node->Selected = true;
				msg.Processed = true;
			} else if (state->LastSelectedRow < 0 && !flat.empty()) {
				state->LastSelectedRow = 0;
				for (auto &n : flat) n.Node->Selected = false;
				flat[0].Node->Selected = true;
				msg.Processed = true;
			}
		} else if (msg.KeyCode == VK_LEFT) {
			if (state->LastSelectedRow >= 0 && state->LastSelectedRow < static_cast<int>(flat.size())) {
				auto &fn = flat[state->LastSelectedRow];
				if (!fn.Node->Children.empty() && fn.Node->Expanded) {
					fn.Node->Expanded = false;
					msg.Processed = true;
				}
			}
		} else if (msg.KeyCode == VK_RIGHT) {
			if (state->LastSelectedRow >= 0 && state->LastSelectedRow < static_cast<int>(flat.size())) {
				auto &fn = flat[state->LastSelectedRow];
				if (!fn.Node->Children.empty() && !fn.Node->Expanded) {
					fn.Node->Expanded = true;
					msg.Processed = true;
				}
			}
		} else if (msg.KeyCode == VK_RETURN) {
			if (profile.ItemActivatedCallback && state->LastSelectedRow >= 0 &&
			    state->LastSelectedRow < static_cast<int>(flat.size())) {
				profile.ItemActivatedCallback(*flat[state->LastSelectedRow].Node);
				msg.Processed = true;
			}
		}
	}

	LayoutNextControl(size);
}

}
