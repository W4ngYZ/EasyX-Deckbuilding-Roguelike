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
 * \file hex_table.cpp
 * \brief Table control implementation
 */

#include <include/hex_table.h>
#include <include/hex.h>

#include <algorithm>
#include <set>
#include <ctime>

#undef max
#undef min

namespace HX {

struct TableState {
	int           ScrollY     = 0;
	int           SelectedRow = -1;
	std::set<int> SelectedRows;
	int           LastSelectedRow = -1;
	bool          Resizing = false;
	int           ResizeColumn = -1;
	int           ResizeStartX = 0;
	int           ResizeStartWidth = 0;
	clock_t       LastClickTime = 0;
	int           LastClickRow = -1;
	int           RowCount = 0;
};

struct ActiveTable {
	bool           Active     = false;
	TableProfile  *Profile    = nullptr;
	HXPoint        Position   = {0, 0};
	int            CurrentRow = -1;
	int            CurrentCol = 0;
	bool           RowVisible = false;
	int            RowY       = 0;
	HXRect         Rect       = {0, 0, 0, 0};
	TableState    *State      = nullptr;
};

static ActiveTable G_ActiveTable;

void TableBegin(const HXString &id, TableProfile &profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	G_ActiveTable = ActiveTable{};
	profile.ActivatedRow = -1;

	if (RoughVisibility()) {
		LayoutNextControl(profile.Size);
		return;
	}

	auto position = CurrentLayoutPosition();
	if (AccVisibility({profile.Size.X, profile.Size.Y})) {
		LayoutNextControl(profile.Size);
		return;
	}

	auto *state = GetStatePool().Get<TableState>(id);
	auto  painter = CurrentPainter();

	HXRect controlRect{position.X, position.Y, position.X + profile.Size.X, position.Y + profile.Size.Y};

	// Handle wheel scrolling
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.MouseWheel) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (controlRect.PointInside(mouse)) {
			state->ScrollY += msg.MouseWheelUp ? profile.RowHeight : -profile.RowHeight;
			msg.Processed = true;
		}
	}

	int headerHeight = profile.RowHeight;

	// Handle column resize before header rendering so the frame reflects it.
	if (profile.Resizable) {
		int colX = position.X;
		for (int i = 0; i < static_cast<int>(profile.Columns.size()); ++i) {
			auto &col = profile.Columns[i];
			HXRect handleRect{colX + col.Width - 3, position.Y,
			                  colX + col.Width + 3, position.Y + headerHeight};
			for (auto &msg : context.MessageQuery) {
				if (msg.Processed || !msg.MouseAction) continue;
				auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
				if (msg.MouseLeftPressed && handleRect.PointInside(mouse)) {
					state->Resizing = true;
					state->ResizeColumn = i;
					state->ResizeStartX = mouse.X;
					state->ResizeStartWidth = col.Width;
					msg.Processed = true;
				}
			}
			colX += col.Width;
		}

		if (state->Resizing && state->ResizeColumn >= 0 &&
		    state->ResizeColumn < static_cast<int>(profile.Columns.size())) {
			for (auto &msg : context.MessageQuery) {
				if (!msg.MouseAction) continue;
				auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
				if (!msg.MouseLeftRelease) {
					int delta = mouse.X - state->ResizeStartX;
					profile.Columns[state->ResizeColumn].Width =
						std::max(profile.MinColumnWidth, state->ResizeStartWidth + delta);
					msg.Processed = true;
				} else {
					state->Resizing = false;
					state->ResizeColumn = -1;
					msg.Processed = true;
				}
			}
		}
	}

	// Keyboard navigation
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.KeyPressed) continue;
		if (msg.KeyCode == VK_UP) {
			if (state->SelectedRow > 0) {
				state->SelectedRow--;
			} else if (state->SelectedRow < 0 && state->RowCount > 0) {
				state->SelectedRow = state->RowCount - 1;
			}
			state->LastSelectedRow = state->SelectedRow;
			state->SelectedRows.clear();
			if (state->SelectedRow >= 0) {
				state->SelectedRows.insert(state->SelectedRow);
			}
			msg.Processed = true;
		} else if (msg.KeyCode == VK_DOWN) {
			if (state->SelectedRow < state->RowCount - 1) {
				state->SelectedRow++;
			} else if (state->SelectedRow < 0 && state->RowCount > 0) {
				state->SelectedRow = 0;
			}
			state->LastSelectedRow = state->SelectedRow;
			state->SelectedRows.clear();
			if (state->SelectedRow >= 0) {
				state->SelectedRows.insert(state->SelectedRow);
			}
			msg.Processed = true;
		} else if (msg.KeyCode == VK_RETURN) {
			if (profile.RowActivatedCallback && state->SelectedRow >= 0) {
				profile.RowActivatedCallback(state->SelectedRow);
				profile.ActivatedRow = state->SelectedRow;
				msg.Processed = true;
			}
		}
	}

	// Draw background
	painter->Begin();
	painter->DrawFilledRoundedRectangle(controlRect, {0,0,0,0}, theme.PanelBackground, theme.RadiusLg);

	// Draw column headers
	int colX = position.X;
	for (const auto &col : profile.Columns) {
		painter->DrawText(col.Header, HXFont{}, {colX + 4, position.Y + 3}, theme.DisabledText, theme.FontSizeCaption);
		if (profile.Resizable) {
			painter->DrawLine({colX + col.Width, position.Y + 4},
			                  {colX + col.Width, position.Y + headerHeight - 4},
			                  theme.PanelBorder);
		}
		colX += col.Width;
	}

	// Header bottom divider
	painter->DrawLine({position.X, position.Y + headerHeight - 1},
	                  {position.X + profile.Size.X, position.Y + headerHeight - 1},
	                  theme.PanelBorder);
	painter->End();

	G_ActiveTable = {
		true,
		&profile,
		position,
		-1,
		0,
		false,
		0,
		controlRect,
		state
	};

	LayoutNextControl(profile.Size);
}

bool TableRow(int rowIndex) {
	if (!G_ActiveTable.Active || !G_ActiveTable.Profile) return false;

	auto &at      = G_ActiveTable;
	auto &profile = *at.Profile;
	auto &context = GetContext();
	auto &theme   = GetTheme();

	at.CurrentRow  = rowIndex;
	at.CurrentCol  = 0;
	int headerHeight = profile.RowHeight;
	int rowY         = at.Position.Y + headerHeight + rowIndex * profile.RowHeight - at.State->ScrollY;
	at.RowY          = rowY;
	at.RowVisible    = (rowY + profile.RowHeight > at.Position.Y + headerHeight) &&
	                    (rowY < at.Position.Y + profile.Size.Y);

	if (!at.RowVisible) {
		return false;
	}

	auto painter = CurrentPainter();

	HXRect rowRect{at.Position.X, rowY, at.Position.X + profile.Size.X, rowY + profile.RowHeight};

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

	painter->Begin();

	// Selection highlight
	bool selected = at.State->SelectedRow == rowIndex ||
	                at.State->SelectedRows.find(rowIndex) != at.State->SelectedRows.end();
	if (selected) {
		painter->DrawFilledRectangle(rowRect, {0,0,0,0}, theme.SelectedBackground);
		painter->DrawFilledRectangle({rowRect.Left, rowRect.Top, rowRect.Left + 3, rowRect.Bottom}, {0,0,0,0}, theme.SelectedBorder);
	} else if (hovered) {
		painter->DrawFilledRectangle(rowRect, {0,0,0,0}, theme.HoverBackground);
	}

	// Grid lines
	int colX = at.Position.X;
	for (const auto &col : profile.Columns) {
		painter->DrawRectangle({colX, rowY, colX + col.Width, rowY + profile.RowHeight}, theme.PanelBorder);
		colX += col.Width;
	}

	painter->End();

	// Handle row selection click
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.MouseLeftPressed) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		HXRect rowRect{at.Position.X, rowY, at.Position.X + profile.Size.X, rowY + profile.RowHeight};
	if (rowRect.PointInside(mouse)) {
			if (profile.MultiSelect && msg.InShift && at.State->LastSelectedRow >= 0) {
				int first = std::min(at.State->LastSelectedRow, rowIndex);
				int last  = std::max(at.State->LastSelectedRow, rowIndex);
				at.State->SelectedRows.clear();
				for (int i = first; i <= last; ++i) {
					at.State->SelectedRows.insert(i);
				}
			} else if (profile.MultiSelect && msg.InCtrl) {
				if (at.State->SelectedRows.find(rowIndex) != at.State->SelectedRows.end()) {
					at.State->SelectedRows.erase(rowIndex);
				} else {
					at.State->SelectedRows.insert(rowIndex);
				}
				at.State->LastSelectedRow = rowIndex;
			} else {
				at.State->SelectedRows.clear();
				at.State->SelectedRows.insert(rowIndex);
				at.State->LastSelectedRow = rowIndex;
			}
			at.State->SelectedRow = rowIndex;
			profile.ActivatedRow = rowIndex;

			// Double-click detection
			clock_t now = clock();
			if (at.State->LastClickRow == rowIndex && (now - at.State->LastClickTime) < CLOCKS_PER_SEC / 3) {
				if (profile.RowActivatedCallback) {
					profile.RowActivatedCallback(rowIndex);
					profile.ActivatedRow = rowIndex;
				}
			}
			at.State->LastClickTime = now;
			at.State->LastClickRow = rowIndex;

			msg.Processed = true;
		}
	}

	return true;
}

void TableCell(const HXString &text) {
	if (!G_ActiveTable.Active || !G_ActiveTable.RowVisible) return;

	auto &at      = G_ActiveTable;
	auto &profile = *at.Profile;
	auto &theme   = GetTheme();
	if (at.CurrentCol >= static_cast<int>(profile.Columns.size())) return;

	auto  painter = CurrentPainter();
	int   colX    = at.Position.X;
	for (int i = 0; i < at.CurrentCol; ++i) colX += profile.Columns[i].Width;

	int cellW = profile.Columns[at.CurrentCol].Width;

	if (profile.RenderCellCallback) {
		HXRect cellRect{colX, at.RowY, colX + cellW, at.RowY + profile.RowHeight};
		profile.RenderCellCallback(painter, cellRect, at.CurrentRow, at.CurrentCol);
	} else {
		painter->Begin();
		painter->DrawText(text, HXFont{}, {colX + 4, at.RowY + 3}, theme.WindowTitleText, theme.FontSizeBody);
		painter->End();
	}

	at.CurrentCol++;
}

void TableEnd() {
	if (!G_ActiveTable.Active) return;

	auto &at      = G_ActiveTable;
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();

	// Draw outer border
	painter->Begin();
	painter->DrawFilledRoundedRectangle(at.Rect, theme.PanelBorder, {0,0,0,0}, theme.RadiusLg);
	painter->End();

	if (at.State) {
		at.State->RowCount = std::max(at.CurrentRow + 1, 0);
	}

	G_ActiveTable = ActiveTable{};
}

}
