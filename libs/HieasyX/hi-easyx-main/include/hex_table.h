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
 * \file hex_table.h
 * \brief Table control with virtual scrolling
 */

#pragma once

#include <include/hex_string.h>
#include <include/hex_geo.h>
#include <vector>
#include <functional>

struct HXBufferPainter;

namespace HX {

/**
 * Column descriptor for Table
 */
struct TableColumn {
	HXString Header;
	int      Width = 100;
};

/**
 * Profile for Table
 */
struct TableProfile {
	HXPoint                  Size      = {400, 300};
	int                      RowHeight = 24;
	std::vector<TableColumn> Columns;
	bool                     Resizable = true;
	bool                     MultiSelect = false;
	int                      MinColumnWidth = 32;
	int                      ActivatedRow = -1;
	std::function<int(const HXString& a, const HXString& b, int columnIdx)> SortCompareCallback;
	std::function<void(HXBufferPainter* p, const HXRect& cellRect, int row, int col)> RenderCellCallback;
	std::function<void(int rowIdx)> RowActivatedCallback;
};

/**
 * Begin a table. Must be paired with TableEnd.
 */
void TableBegin(const HXString &id, TableProfile &profile);

/**
 * Begin a row in the table. Returns true if the row is visible and should be rendered.
 * @param rowIndex The zero-based index of this row
 */
bool TableRow(int rowIndex);

/**
 * Render a cell in the current row. Must be called once per column.
 * @param text The cell text
 */
void TableCell(const HXString &text);

/**
 * End the table.
 */
void TableEnd();

}
