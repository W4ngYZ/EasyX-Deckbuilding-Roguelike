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
 * \file hex_splitter.h
 * \brief Draggable splitter for resizable panel layouts
 */

#pragma once

#include <include/hex_string.h>

namespace HX {

/**
 * Profile for a draggable splitter.
 */
struct SplitterProfile {
	bool   Horizontal = false; // false = vertical (splits left/right), true = horizontal (splits top/bottom)
	HXGInt Thickness  = 4;
	HXGInt MinSize    = 20;
	HXGInt MaxSize    = 1000;
	bool   InDrag     = false;
	HXGInt DragStart  = 0;
	bool   OnHover    = false;
};

/**
 * A draggable splitter between two panels.
 *
 * Place between two panels (e.g. inside BeginHorizontal/EndHorizontal for a
 * vertical splitter).  The splitter occupies `Thickness` pixels and updates
 * `Size` (the width or height of the first panel) when dragged.
 *
 * @param id      Unique identifier for drag-state persistence
 * @param Size    Reference to the first panel's size (width if vertical, height if horizontal)
 * @param Profile Appearance and behaviour settings
 * @return True while the user is actively dragging the splitter
 */
bool Splitter(const HXString &id, HXGInt &Size, SplitterProfile &Profile);

} // namespace HX
