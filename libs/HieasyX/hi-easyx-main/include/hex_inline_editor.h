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
 * \file hex_inline_editor.h
 * \brief Single-line text editor for inline editing
 */


#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>
#include <ctime>

namespace HX {

/**
 * Persistent editing state.
 * Allocate one of these and keep it across frames.
 */
struct HXSingleLineEditorState {
	HXString Text;
	int CursorPos = 0;
	int SelectStart = 0;
	int SelectEnd = 0;
	int AnchorPos = 0;
	bool Dragging = false;

	// IME
	bool InIMEComposition = false;
	HXString IMECompositionString;
	clock_t LastIMEEndTime = 0;

	// Blink
	clock_t BlinkClock = 0;
	bool ShowCursor = true;

	void Reset() {
		Text.clear();
		CursorPos = 0;
		SelectStart = 0;
		SelectEnd = 0;
		AnchorPos = 0;
		Dragging = false;
		InIMEComposition = false;
		IMECompositionString.clear();
		LastIMEEndTime = 0;
		BlinkClock = 0;
		ShowCursor = true;
	}

	// Returns true if there is an active (non-empty) selection
	bool HasSelection() const {
		return SelectStart != SelectEnd;
	}

	// Normalized selection range
	int SelMin() const { return (SelectStart < SelectEnd) ? SelectStart : SelectEnd; }
	int SelMax() const { return (SelectStart < SelectEnd) ? SelectEnd : SelectStart; }

	// Delete current selection, if any. Returns true if something was deleted.
	bool DeleteSelection() {
		if (!HasSelection()) return false;
		int start = SelMin();
		int end = SelMax();
		Text.erase(start, end - start);
		CursorPos = start;
		SelectStart = start;
		SelectEnd = start;
		AnchorPos = start;
		return true;
	}

	// Insert text at cursor, replacing selection if any.
	void Insert(const HXString& str) {
		DeleteSelection();
		Text.insert(CursorPos, str);
		CursorPos += static_cast<int>(str.size());
		SelectStart = CursorPos;
		SelectEnd = CursorPos;
		AnchorPos = CursorPos;
	}

	// Insert a single character.
	void InsertChar(HXChar ch) {
		DeleteSelection();
		Text.insert(CursorPos, 1, ch);
		++CursorPos;
		SelectStart = CursorPos;
		SelectEnd = CursorPos;
		AnchorPos = CursorPos;
	}
};

/**
 * Style configuration for the single-line editor.
 */
struct HXSingleLineEditorStyle {
	int FontSize = 18;
	HXColor TextColor = {220, 220, 220};
	HXColor SelectionBg = {0, 120, 215};
	HXColor SelectionTextColor = {255, 255, 255};
	HXColor CursorColor = {255, 255, 255};
	HXColor BackgroundColor = {45, 45, 48};
	HXColor BorderColor = {100, 100, 100};
	HXColor BorderFocusColor = {0, 150, 255};
	int LeftPadding = 5;
};

/**
 * Single-line editor instance.
 * Bounds are in the painter's local coordinate space.
 */
struct HXSingleLineEditor {
	HXSingleLineEditorState* State = nullptr;
	HXRect Bounds = {0, 0, 0, 0};   // Local painter coordinates
	bool Active = false;            // Focused / active this frame
	bool WasActive = false;         // Focused last frame (for IME cleanup)
	bool MouseStyle = false;        // Track cursor style state
	HXSingleLineEditorStyle Style;

	// Optional: absolute screen position for IME rectangle update.
	// If left as {0,0}, IME rect is computed from Bounds + current window offset.
	HXPoint IMEBasePos = {0, 0};
};

/**
 * Process all messages for the current frame.
 * Call once per frame while the editor is visible.
 * Messages that belong to the editor are marked as Processed.
 */
void SingleLineEditorProcessMessages(HXSingleLineEditor& editor, HXBufferPainter* painter);

/**
 * Render the editor. Should be called after ProcessMessages.
 */
void SingleLineEditorRender(HXSingleLineEditor& editor, HXBufferPainter* painter);

/**
 * Convert a local X coordinate inside the editor bounds to a text cursor position.
 */
int SingleLineEditorHitTest(HXSingleLineEditor& editor, HXBufferPainter* painter, HXGInt localX);

/**
 * Update IME candidate window position.
 * Call after rendering if editor is active and on focus.
 */
void SingleLineEditorUpdateIME(HXSingleLineEditor& editor, HXBufferPainter* painter);

} // namespace HX
