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
 * \file hex_inline_editor.cpp
 * \brief Single-line text editor implementation
 */


#include <include/hex_inline_editor.h>
#include <include/hex.h>
#include <include/hex_pool.h>
#include <algorithm>

#undef max
#undef min

namespace HX {

bool HXIMEPendingStartComposition();

////////////////////////////////////////////////////////////
// Helper: clamp selection to text bounds
static void ClampCursor(HXSingleLineEditorState& state) {
	int len = static_cast<int>(state.Text.size());
	if (state.CursorPos < 0) state.CursorPos = 0;
	if (state.CursorPos > len) state.CursorPos = len;
}

////////////////////////////////////////////////////////////
// Hit-test: local X -> cursor position

int SingleLineEditorHitTest(HXSingleLineEditor& editor, HXBufferPainter* painter, HXGInt localX) {
	if (!editor.State) return 0;
	auto& state = *editor.State;
	HXFont defaultFont{};
	HXGInt x = editor.Style.LeftPadding;
	HXGInt target = localX;

	if (target <= x) return 0;

	for (int i = 0; i < static_cast<int>(state.Text.size()); ++i) {
		HXString chStr = state.Text.substr(i, 1);
		HXGInt chW = painter->MeasureText(chStr, defaultFont, editor.Style.FontSize).CalWidth();
		if (target < x + chW / 2) {
			return i;
		}
		x += chW;
	}
	return static_cast<int>(state.Text.size());
}

////////////////////////////////////////////////////////////
// Message processing

static void ProcessMouse(HXSingleLineEditor& editor, HXBufferPainter* painter) {
	auto& context = GetContext();
	auto& state = *editor.State;

	for (auto& msg : context.MessageQuery) {
		if (msg.Processed) continue;
		if (!msg.MouseAction) continue;

		// Bounds are in window-local coordinates (same as TextInput's editRectangle).
		// Mouse messages arrive in screen coordinates, so we must ClipCoord() them.
		HXPoint localMouse = ClipCoord({msg.MouseX, msg.MouseY});
		bool inside = editor.Bounds.PointInside(localMouse);

		// Update drag selection FIRST (even if mouse moves outside bounds)
		if (state.Dragging && !msg.MouseLeftRelease) {
			int pos = SingleLineEditorHitTest(editor, painter, localMouse.X - editor.Bounds.Left);
			state.SelectEnd = pos;
			state.CursorPos = pos;
		}

		if (inside) {
			// Set editing cursor style
			if (!editor.MouseStyle) {
				editor.MouseStyle = true;
				context.FrameCursor = HXCursorStyle::Editing;
			}

			if (msg.MouseLeftPressed) {
				msg.Processed = true;
				editor.Active = true;
				state.BlinkClock = clock();
				state.ShowCursor = true;

				int pos = SingleLineEditorHitTest(editor, painter, localMouse.X - editor.Bounds.Left);
				if (msg.InShift && state.HasSelection()) {
					// Extend selection from anchor
					state.SelectEnd = pos;
					state.CursorPos = pos;
				} else {
					state.CursorPos = pos;
					state.SelectStart = pos;
					state.SelectEnd = pos;
					state.AnchorPos = pos;
					state.Dragging = true;
				}
			}
			if (msg.MouseLeftRelease) {
				msg.Processed = true;
				state.Dragging = false;
			}
		} else if (msg.MouseLeftPressed) {
			// Click outside deactivates
			if (editor.Active) {
				editor.Active = false;
				if (state.InIMEComposition) {
					context.OSAPI->CancelIMEComposition();
					state.InIMEComposition = false;
					state.IMECompositionString.clear();
				}
			}
			if (editor.MouseStyle) {
				editor.MouseStyle = false;
			}
		} else if (msg.MouseLeftRelease) {
			msg.Processed = true;
			state.Dragging = false;
			if (editor.MouseStyle) {
				editor.MouseStyle = false;
			}
		} else if (editor.MouseStyle) {
			// Mouse moved outside without any button action
			editor.MouseStyle = false;
		}
	}
}

static void ProcessIME(HXSingleLineEditor& editor) {
	auto& context = GetContext();
	auto& state = *editor.State;

	for (auto& msg : context.MessageQuery) {
		if (msg.Processed) continue;

		if (msg.IMEStartComposition) {
			state.InIMEComposition = true;
			state.IMECompositionString.clear();
			msg.Processed = true;
		}
		if (msg.IMEResult) {
			if (!msg.IMEResultString.empty()) {
				state.DeleteSelection();
				state.Text.insert(state.CursorPos, msg.IMEResultString);
				state.CursorPos += static_cast<int>(msg.IMEResultString.size());
				state.SelectStart = state.CursorPos;
				state.SelectEnd = state.CursorPos;
				state.AnchorPos = state.CursorPos;
			}
			state.InIMEComposition = false;
			state.LastIMEEndTime = clock();
			state.IMECompositionString.clear();
			msg.Processed = true;
		}
		if (msg.IMEComposition) {
			state.IMECompositionString = msg.IMECompositionString;
			if (!state.IMECompositionString.empty()) {
				state.InIMEComposition = true;
			}
			msg.Processed = true;
		}
		if (msg.IMEEndComposition) {
			state.InIMEComposition = false;
			state.LastIMEEndTime = clock();
			state.IMECompositionString.clear();
			msg.Processed = true;
		}
	}
}

static void ProcessCharacters(HXSingleLineEditor& editor) {
	auto& context = GetContext();
	auto& state = *editor.State;

	if (!editor.Active) return;

	for (auto& msg : context.MessageQuery) {
		if (msg.Processed) continue;
		if (!msg.InputChar) continue;

		msg.Processed = true;

		// Ctrl+A
		if (msg.Char == '\x1') {
			state.SelectStart = 0;
			state.SelectEnd = static_cast<int>(state.Text.size());
			state.CursorPos = state.SelectEnd;
			state.AnchorPos = 0;
			state.BlinkClock = clock();
			state.ShowCursor = true;
			continue;
		}

		// Ctrl+C
		if (msg.Char == '\x3') {
			if (state.HasSelection()) {
				auto& ctx = GetContext();
				ctx.OSAPI->CopyToClipboard(state.Text.substr(state.SelMin(), state.SelMax() - state.SelMin()));
			}
			continue;
		}

		// Ctrl+V
		if (msg.Char == '\x16') {
			auto& ctx = GetContext();
			HXString clip = ctx.OSAPI->ReadFromClipboard();
			if (!clip.empty()) {
				// Sanitize to single line: replace newlines with spaces
				for (auto& c : clip) {
					if (c == L'\r' || c == L'\n') c = L' ';
				}
				state.DeleteSelection();
				state.Text.insert(state.CursorPos, clip);
				state.CursorPos += static_cast<int>(clip.size());
				state.SelectStart = state.CursorPos;
				state.SelectEnd = state.CursorPos;
				state.AnchorPos = state.CursorPos;
				state.BlinkClock = clock();
				state.ShowCursor = true;
			}
			continue;
		}

		// Suppress normal characters during IME composition or shortly after
		// Also suppress if a composition string is present (handles WM_CHAR arriving
		// before WM_IME_STARTCOMPOSITION is processed).
		if (state.InIMEComposition || !state.IMECompositionString.empty()) {
			continue;
		}
		if (clock() - state.LastIMEEndTime < 50) {
			continue;
		}

		// Enter / Return -> handled by caller as commit signal
		if (msg.Char == '\r' || msg.Char == '\n') {
			// Do not consume here; caller may want to detect it
			msg.Processed = false;
			continue;
		}

		// Printable character
		if (msg.Char >= L' ') {
			state.DeleteSelection();
			state.Text.insert(state.CursorPos, 1, msg.Char);
			++state.CursorPos;
			state.SelectStart = state.CursorPos;
			state.SelectEnd = state.CursorPos;
			state.AnchorPos = state.CursorPos;
			state.BlinkClock = clock();
			state.ShowCursor = true;
		}
	}
}

static void ProcessKeys(HXSingleLineEditor& editor) {
	auto& context = GetContext();
	auto& state = *editor.State;

	if (!editor.Active) return;

	for (auto& msg : context.MessageQuery) {
		if (msg.Processed) continue;
		if (!msg.KeyPressed) continue;

		// During IME composition, let the IME handle all keyboard input.
		if (state.InIMEComposition) {
			continue;
		}

		bool handled = true;
		bool hasSel = state.HasSelection();
		int selMin = state.SelMin();
		int selMax = state.SelMax();

		auto resetSel = [&]() {
			state.SelectStart = state.CursorPos;
			state.SelectEnd = state.CursorPos;
			state.AnchorPos = state.CursorPos;
		};

		auto extendSel = [&](int newPos) {
			state.CursorPos = newPos;
			state.SelectEnd = newPos;
		};

		switch (msg.KeyCode) {
			case VK_LEFT:
				state.BlinkClock = clock();
				state.ShowCursor = true;
				if (msg.InShift) {
					if (hasSel) {
						int moving = (state.SelectStart == state.AnchorPos) ? state.SelectEnd : state.SelectStart;
						moving = std::max(0, moving - 1);
						state.SelectStart = std::min(state.AnchorPos, moving);
						state.SelectEnd = std::max(state.AnchorPos, moving);
						state.CursorPos = moving;
					} else {
						state.AnchorPos = state.CursorPos;
						state.CursorPos = std::max(0, state.CursorPos - 1);
						state.SelectStart = state.AnchorPos;
						state.SelectEnd = state.CursorPos;
					}
				} else {
					if (hasSel) {
						state.CursorPos = selMin;
					} else {
						state.CursorPos = std::max(0, state.CursorPos - 1);
					}
					resetSel();
				}
				break;

			case VK_RIGHT:
				state.BlinkClock = clock();
				state.ShowCursor = true;
				if (msg.InShift) {
					if (hasSel) {
						int moving = (state.SelectStart == state.AnchorPos) ? state.SelectEnd : state.SelectStart;
						moving = std::min(static_cast<int>(state.Text.size()), moving + 1);
						state.SelectStart = std::min(state.AnchorPos, moving);
						state.SelectEnd = std::max(state.AnchorPos, moving);
						state.CursorPos = moving;
					} else {
						state.AnchorPos = state.CursorPos;
						state.CursorPos = std::min(static_cast<int>(state.Text.size()), state.CursorPos + 1);
						state.SelectStart = state.AnchorPos;
						state.SelectEnd = state.CursorPos;
					}
				} else {
					if (hasSel) {
						state.CursorPos = selMax;
					} else {
						state.CursorPos = std::min(static_cast<int>(state.Text.size()), state.CursorPos + 1);
					}
					resetSel();
				}
				break;

			case VK_HOME:
				state.BlinkClock = clock();
				state.ShowCursor = true;
				if (msg.InShift) {
					if (hasSel) {
						int moving = (state.SelectStart == state.AnchorPos) ? state.SelectEnd : state.SelectStart;
						state.SelectStart = std::min(state.AnchorPos, 0);
						state.SelectEnd = std::max(state.AnchorPos, 0);
						state.CursorPos = 0;
					} else {
						state.AnchorPos = state.CursorPos;
						state.CursorPos = 0;
						state.SelectStart = state.AnchorPos;
						state.SelectEnd = 0;
					}
				} else {
					state.CursorPos = 0;
					resetSel();
				}
				break;

			case VK_END:
				state.BlinkClock = clock();
				state.ShowCursor = true;
				if (msg.InShift) {
					if (hasSel) {
						int moving = (state.SelectStart == state.AnchorPos) ? state.SelectEnd : state.SelectStart;
						int endPos = static_cast<int>(state.Text.size());
						state.SelectStart = std::min(state.AnchorPos, endPos);
						state.SelectEnd = std::max(state.AnchorPos, endPos);
						state.CursorPos = endPos;
					} else {
						state.AnchorPos = state.CursorPos;
						int endPos = static_cast<int>(state.Text.size());
						state.CursorPos = endPos;
						state.SelectStart = state.AnchorPos;
						state.SelectEnd = endPos;
					}
				} else {
					state.CursorPos = static_cast<int>(state.Text.size());
					resetSel();
				}
				break;

			case VK_BACK:
				state.BlinkClock = clock();
				state.ShowCursor = true;
				if (hasSel) {
					state.DeleteSelection();
				} else if (state.CursorPos > 0) {
					state.Text.erase(state.CursorPos - 1, 1);
					--state.CursorPos;
				}
				resetSel();
				break;

			case VK_DELETE:
				state.BlinkClock = clock();
				state.ShowCursor = true;
				if (hasSel) {
					state.DeleteSelection();
				} else if (state.CursorPos < static_cast<int>(state.Text.size())) {
					state.Text.erase(state.CursorPos, 1);
				}
				resetSel();
				break;

			case VK_ESCAPE:
				// Caller handles cancellation
				handled = false;
				break;

			case VK_RETURN:
				// Caller handles commit
				handled = false;
				break;

			default:
				handled = false;
				break;
		}

		if (handled) {
			msg.Processed = true;
		}
	}
}

void SingleLineEditorProcessMessages(HXSingleLineEditor& editor, HXBufferPainter* painter) {
	if (!editor.State) return;

	editor.WasActive = editor.Active;

	ProcessMouse(editor, painter);
	ProcessIME(editor);
	ProcessKeys(editor);
	ProcessCharacters(editor);

	ClampCursor(*editor.State);
}

////////////////////////////////////////////////////////////
// Rendering

void SingleLineEditorRender(HXSingleLineEditor& editor, HXBufferPainter* painter) {
	if (!editor.State) return;
	auto& state = *editor.State;
	auto& style = editor.Style;
	HXFont defaultFont{};

	// Background
	painter->Begin();
	painter->DrawFilledRectangle(editor.Bounds, style.BackgroundColor, style.BackgroundColor);
	painter->End();

	// Border
	painter->Begin();
	auto borderColor = editor.Active ? style.BorderFocusColor : style.BorderColor;
	painter->DrawRectangle(editor.Bounds, borderColor);
	if (editor.Active) {
		painter->DrawRectangle(
			{editor.Bounds.Left - 1, editor.Bounds.Top - 1, editor.Bounds.Right + 1, editor.Bounds.Bottom + 1},
			borderColor);
	}
	painter->End();

	// Clip text to inner bounds
	HXRect inner = {
		editor.Bounds.Left + style.LeftPadding,
		editor.Bounds.Top + 2,
		editor.Bounds.Right - style.LeftPadding,
		editor.Bounds.Bottom - 2
	};

	// Text rendering with selection support
	HXGInt baseY = editor.Bounds.Top + (editor.Bounds.CalHeight() - style.FontSize) / 2;
	HXGInt textX = inner.Left;

	int selMin = state.SelMin();
	int selMax = state.SelMax();

	// If we have IME composition, treat the composition text as "virtual" inserted at cursor
	HXString displayText = state.Text;
	int compLen = static_cast<int>(state.IMECompositionString.size());
	bool hasComp = state.InIMEComposition && compLen > 0;

	// Helper: draw a substring character-by-character with selection support
	auto drawTextSegment = [&](const HXString& segment, HXGInt startX, int baseIndex) -> HXGInt {
		HXGInt x = startX;
		for (int i = 0; i < static_cast<int>(segment.size()); ++i) {
			int globalIdx = baseIndex + i;
			bool inSel = (globalIdx >= selMin && globalIdx < selMax);
			HXString chStr = segment.substr(i, 1);
			HXGInt chW = painter->MeasureText(chStr, defaultFont, style.FontSize).CalWidth();
			if (inSel) {
				painter->Begin();
				painter->DrawFilledRectangle({x, baseY, x + chW, baseY + style.FontSize},
				                              style.SelectionBg, style.SelectionBg);
				painter->End();
			}
			auto color = inSel ? style.SelectionTextColor : style.TextColor;
			painter->Begin();
			painter->DrawText(chStr, defaultFont, {x, baseY}, color, style.FontSize);
			painter->End();
			x += chW;
		}
		return x;
	};

	// Render: before cursor | composition | after cursor
	// This ensures after-cursor text is pushed right by the composition width.
	int cursorPos = state.CursorPos;
	textX = inner.Left;

	// 1. Text before cursor
	HXString before = displayText.substr(0, cursorPos);
	textX = drawTextSegment(before, textX, 0);

	// 2. IME composition string at cursor (pushes after-text to the right)
	HXGInt compW = 0;
	if (hasComp) {
		painter->Begin();
		painter->DrawText(state.IMECompositionString, defaultFont, {textX, baseY}, style.TextColor, style.FontSize);
		compW = painter->MeasureText(state.IMECompositionString, defaultFont, style.FontSize).CalWidth();
		painter->DrawLine({textX, baseY + style.FontSize}, {textX + compW, baseY + style.FontSize}, style.TextColor);
		painter->End();
		textX += compW;
	}

	// 3. Text after cursor
	HXString after = displayText.substr(cursorPos);
	textX = drawTextSegment(after, textX, cursorPos);

	// Cursor blink
	if (editor.Active) {
		if (clock() - state.BlinkClock > 500) {
			state.BlinkClock = clock();
			state.ShowCursor = !state.ShowCursor;
		}

		if (state.ShowCursor) {
			// Compute cursor X (up to actual text cursor position)
			HXGInt cursorX = inner.Left;
			for (int i = 0; i < state.CursorPos && i < static_cast<int>(displayText.size()); ++i) {
				HXString chStr = displayText.substr(i, 1);
				cursorX += painter->MeasureText(chStr, defaultFont, style.FontSize).CalWidth();
			}
			// Add composition width so cursor follows the candidate pinyin
			if (hasComp) {
				cursorX += painter->MeasureText(state.IMECompositionString, defaultFont, style.FontSize).CalWidth();
			}

			painter->Begin();
			painter->DrawLine({cursorX, baseY}, {cursorX, baseY + style.FontSize}, style.CursorColor);
			painter->End();
		}
	}
}

////////////////////////////////////////////////////////////
// IME position update

void SingleLineEditorUpdateIME(HXSingleLineEditor& editor, HXBufferPainter* painter) {
	if (!editor.State || !editor.Active) return;
	auto& state = *editor.State;
	auto& context = GetContext();

	HXFont defaultFont{};
	HXGInt compW = 0;
	if (state.InIMEComposition && !state.IMECompositionString.empty()) {
		compW = painter->MeasureText(state.IMECompositionString, defaultFont, editor.Style.FontSize).CalWidth();
	}

	HXGInt absX = editor.Bounds.Left + editor.Style.LeftPadding;
	HXGInt absY = editor.Bounds.Top + (editor.Bounds.CalHeight() - editor.Style.FontSize) / 2;

	// Add window/scroller offsets if IMEBasePos is not manually set
	if (editor.IMEBasePos.X == 0 && editor.IMEBasePos.Y == 0 && context.CurrentWindow) {
		absX += context.CurrentWindow->ScrollerX + context.CurrentWindow->Where.X;
		absY += context.CurrentWindow->ScrollerY + context.CurrentWindow->Where.Y;
	} else {
		absX += editor.IMEBasePos.X;
		absY += editor.IMEBasePos.Y;
	}

	// Compute text width up to cursor
	for (int i = 0; i < state.CursorPos && i < static_cast<int>(state.Text.size()); ++i) {
		HXString chStr = state.Text.substr(i, 1);
		absX += painter->MeasureText(chStr, defaultFont, editor.Style.FontSize).CalWidth();
	}

	HXRect imeRect;
	imeRect.Left = absX;
	imeRect.Top = absY;
	imeRect.Right = absX + compW;
	imeRect.Bottom = absY + editor.Style.FontSize;

	static HXRect lastRect = {0, 0, 0, 0};
	if (imeRect.Left != lastRect.Left || imeRect.Top != lastRect.Top ||
	    imeRect.Right != lastRect.Right || HXIMEPendingStartComposition()) {
		context.OSAPI->SetIMERect(imeRect);
		lastRect = imeRect;
	}
}

} // namespace HX
