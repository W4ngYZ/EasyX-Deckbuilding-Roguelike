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
 * \file hex_editor.cpp
 * \brief Code Editor implementation
 */

#include <include/hex_editor.h>
#include <include/hex.h>

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <ctime>
#include <set>
#include <utility>

#undef max
#undef min

namespace HX {

bool HXIMEPendingStartComposition();

////////////////////////////////////////////////////////////
/// Lexer

static const std::set<HXString> G_CppKeywords = {
	HXStr("alignas"), HXStr("alignof"), HXStr("and"), HXStr("and_eq"), HXStr("asm"),
	HXStr("auto"), HXStr("bitand"), HXStr("bitor"), HXStr("bool"), HXStr("break"),
	HXStr("case"), HXStr("catch"), HXStr("char"), HXStr("char8_t"), HXStr("char16_t"),
	HXStr("char32_t"), HXStr("class"), HXStr("compl"), HXStr("concept"), HXStr("const"),
	HXStr("consteval"), HXStr("constexpr"), HXStr("constinit"), HXStr("const_cast"),
	HXStr("continue"), HXStr("co_await"), HXStr("co_return"), HXStr("co_yield"),
	HXStr("decltype"), HXStr("default"), HXStr("delete"), HXStr("do"), HXStr("double"),
	HXStr("dynamic_cast"), HXStr("else"), HXStr("enum"), HXStr("explicit"), HXStr("export"),
	HXStr("extern"), HXStr("false"), HXStr("float"), HXStr("for"), HXStr("friend"),
	HXStr("goto"), HXStr("if"), HXStr("inline"), HXStr("int"), HXStr("long"),
	HXStr("mutable"), HXStr("namespace"), HXStr("new"), HXStr("noexcept"), HXStr("not"),
	HXStr("not_eq"), HXStr("nullptr"), HXStr("operator"), HXStr("or"), HXStr("or_eq"),
	HXStr("private"), HXStr("protected"), HXStr("public"), HXStr("register"),
	HXStr("reinterpret_cast"), HXStr("requires"), HXStr("return"), HXStr("short"),
	HXStr("signed"), HXStr("sizeof"), HXStr("static"), HXStr("static_assert"),
	HXStr("static_cast"), HXStr("struct"), HXStr("switch"), HXStr("template"),
	HXStr("this"), HXStr("thread_local"), HXStr("throw"), HXStr("true"), HXStr("try"),
	HXStr("typedef"), HXStr("typeid"), HXStr("typename"), HXStr("union"), HXStr("unsigned"),
	HXStr("using"), HXStr("virtual"), HXStr("void"), HXStr("volatile"), HXStr("wchar_t"),
	HXStr("while"), HXStr("xor"), HXStr("xor_eq"),
	// Common types / macros
	HXStr("size_t"), HXStr("ssize_t"), HXStr("uint32_t"), HXStr("int32_t"),
	HXStr("uint64_t"), HXStr("int64_t"), HXStr("uint8_t"), HXStr("int8_t"),
	HXStr("uint16_t"), HXStr("int16_t"), HXStr("uintptr_t"), HXStr("intptr_t"),
	HXStr("NULL"), HXStr("TRUE"), HXStr("FALSE"),
};

static std::vector<EditorToken> DefaultCppLexer(const HXString &line) {
	std::vector<EditorToken> tokens;
	size_t i = 0;
	while (i < line.size()) {
		wchar_t c = line[i];

		// Line comment
		if (c == L'/' && i + 1 < line.size() && line[i + 1] == L'/') {
			tokens.push_back({line.substr(i), EditorTokenType::Comment});
			break;
		}

		// Preprocessor directive
		if (c == L'#' && (i == 0 || iswspace(line[i - 1]))) {
			size_t j = i + 1;
			while (j < line.size() && iswalpha(line[j])) ++j;
			tokens.push_back({line.substr(i, j - i), EditorTokenType::Keyword});
			i = j;
			continue;
		}

		// String literal (double quotes)
		if (c == L'"') {
			size_t j = i + 1;
			while (j < line.size() && line[j] != L'"') {
				if (line[j] == L'\\' && j + 1 < line.size()) ++j;
				++j;
			}
			if (j < line.size()) ++j;
			tokens.push_back({line.substr(i, j - i), EditorTokenType::String});
			i = j;
			continue;
		}

		// Character literal (single quotes)
		if (c == L'\'') {
			size_t j = i + 1;
			while (j < line.size() && line[j] != L'\'') {
				if (line[j] == L'\\' && j + 1 < line.size()) ++j;
				++j;
			}
			if (j < line.size()) ++j;
			tokens.push_back({line.substr(i, j - i), EditorTokenType::String});
			i = j;
			continue;
		}

		// Number
		if (iswdigit(c) || (c == L'.' && i + 1 < line.size() && iswdigit(line[i + 1]))) {
			size_t j = i;
			// Hex / binary / octal prefix
			if (c == L'0' && j + 1 < line.size() &&
			    (line[j + 1] == L'x' || line[j + 1] == L'X' ||
			     line[j + 1] == L'b' || line[j + 1] == L'B')) {
				j += 2;
				while (j < line.size() && iswxdigit(line[j])) ++j;
			} else {
				while (j < line.size() && iswdigit(line[j])) ++j;
				if (j < line.size() && line[j] == L'.') {
					++j;
					while (j < line.size() && iswdigit(line[j])) ++j;
				}
				if (j < line.size() && (line[j] == L'e' || line[j] == L'E')) {
					++j;
					if (j < line.size() && (line[j] == L'+' || line[j] == L'-')) ++j;
					while (j < line.size() && iswdigit(line[j])) ++j;
				}
			}
			// Integer suffixes
			while (j < line.size() &&
			       (line[j] == L'u' || line[j] == L'U' || line[j] == L'l' || line[j] == L'L' ||
			        line[j] == L'z' || line[j] == L'Z' || line[j] == L'f' || line[j] == L'F')) {
				++j;
			}
			tokens.push_back({line.substr(i, j - i), EditorTokenType::Number});
			i = j;
			continue;
		}

		// Identifier / Keyword
		if (iswalpha(c) || c == L'_') {
			size_t j = i;
			while (j < line.size() && (iswalnum(line[j]) || line[j] == L'_')) ++j;
			HXString word = line.substr(i, j - i);
			EditorTokenType t = EditorTokenType::Identifier;
			if (G_CppKeywords.count(word)) t = EditorTokenType::Keyword;
			tokens.push_back({word, t});
			i = j;
			continue;
		}

		// Multi-char operators
		static const wchar_t *twoCharOps[] = {
			L"++", L"--", L"->", L"::", L"<<", L">>", L"&&", L"||", L"==", L"!=",
			L"<=", L">=", L"+=", L"-=", L"*=", L"/=", L"%=", L"&=", L"|=", L"^=",
			L"<<=", L">>=", L"->*", L"..."
		};
		bool foundOp = false;
		for (const wchar_t *op : twoCharOps) {
			size_t len = std::wcslen(op);
			if (i + len <= line.size() && line.compare(i, len, op) == 0) {
				tokens.push_back({line.substr(i, len), EditorTokenType::Operator});
				i += len;
				foundOp = true;
				break;
			}
		}
		if (foundOp) continue;

		// Single-char operator
		if (std::wcschr(L"+-*/=<>!&|%^~?:.,;()[]{}", c)) {
			tokens.push_back({line.substr(i, 1), EditorTokenType::Operator});
			++i;
			continue;
		}

		// Whitespace
		if (iswspace(c)) {
			size_t j = i;
			while (j < line.size() && iswspace(line[j])) ++j;
			tokens.push_back({line.substr(i, j - i), EditorTokenType::Whitespace});
			i = j;
			continue;
		}

		// Unknown
		tokens.push_back({line.substr(i, 1), EditorTokenType::Unknown});
		++i;
	}
	return tokens;
}

static HXColor GetTokenColor(EditorTokenType type, const HXTheme &theme, const TextEditorProfile &profile) {
	if (profile.TokenColorCallback) {
		return profile.TokenColorCallback(type);
	}
	switch (type) {
	case EditorTokenType::Keyword:     return theme.EditorKeywordColor;
	case EditorTokenType::String:      return theme.EditorStringColor;
	case EditorTokenType::Comment:     return theme.EditorCommentColor;
	case EditorTokenType::Number:      return theme.EditorNumberColor;
	case EditorTokenType::Operator:    return theme.EditorOperatorColor;
	case EditorTokenType::Identifier:  return theme.EditorIdentifierColor;
	case EditorTokenType::Whitespace:  return theme.TextInputText;
	default:                           return theme.TextInputText;
	}
}

////////////////////////////////////////////////////////////
/// Editor state

struct EditorState {
	int CursorLine   = 0;
	int CursorColumn = 0;
	int ScrollY      = 0;
	int ScrollX      = 0;
	int DesiredColumn = 0; // For vertical cursor movement preserving column intent
	bool Focused = false;
	bool DraggingSelection = false;
	bool DraggingVScroll = false;
	bool DraggingHScroll = false;
	int DragStartX = 0;
	int DragStartY = 0;
	int DragScrollX = 0;
	int DragScrollY = 0;
	bool CursorVisible = true;
	clock_t LastCursorBlink = 0;

	// Selection
	bool HasSelection = false;
	int SelStartLine = 0, SelStartCol = 0;
	int SelEndLine = 0, SelEndCol = 0;
	int AnchorLine = 0, AnchorCol = 0;

	// Click tracking (double-click = word, triple-click = line)
	clock_t LastClickTime = 0;
	int LastClickLine = -1;
	int LastClickColumn = -1;
	int ClickCount = 0;

	// IME composition state
	bool InIMEComposition = false;
	HXString IMECompositionString;
	clock_t LastIMEEndTime = 0;
	HXRect IMELastRect;
};

static void ClampCursor(EditorState &st, const std::vector<HXString> &lines) {
	if (lines.empty()) {
		st.CursorLine = 0;
		st.CursorColumn = 0;
		return;
	}
	st.CursorLine = std::max(0, std::min(st.CursorLine, static_cast<int>(lines.size()) - 1));
	int lineLen = static_cast<int>(lines[st.CursorLine].size());
	st.CursorColumn = std::max(0, std::min(st.CursorColumn, lineLen));
}

static bool PositionLess(int lineA, int colA, int lineB, int colB) {
	return lineA < lineB || (lineA == lineB && colA < colB);
}

static void NormalizeSelection(const EditorState &st, int &sl, int &sc, int &el, int &ec) {
	sl = st.SelStartLine;
	sc = st.SelStartCol;
	el = st.SelEndLine;
	ec = st.SelEndCol;
	if (PositionLess(el, ec, sl, sc)) {
		std::swap(sl, el);
		std::swap(sc, ec);
	}
}

static bool HasRealSelection(const EditorState &st) {
	return st.HasSelection && (st.SelStartLine != st.SelEndLine || st.SelStartCol != st.SelEndCol);
}

static void ClearSelection(EditorState &st) {
	st.HasSelection = false;
	st.SelStartLine = st.SelEndLine = st.CursorLine;
	st.SelStartCol = st.SelEndCol = st.CursorColumn;
	st.AnchorLine = st.CursorLine;
	st.AnchorCol = st.CursorColumn;
}

static void SetSelectionFromAnchor(EditorState &st, int line, int col) {
	st.CursorLine = line;
	st.CursorColumn = col;
	st.SelStartLine = st.AnchorLine;
	st.SelStartCol = st.AnchorCol;
	st.SelEndLine = line;
	st.SelEndCol = col;
	st.HasSelection = st.SelStartLine != st.SelEndLine || st.SelStartCol != st.SelEndCol;
}

static void ResetBlink(EditorState &st) {
	st.CursorVisible = true;
	st.LastCursorBlink = clock();
}

static void DeleteSelection(std::vector<HXString> &lines, EditorState &st) {
	if (!HasRealSelection(st)) return;

	int sl, sc, el, ec;
	NormalizeSelection(st, sl, sc, el, ec);

	if (sl == el) {
		lines[sl].erase(sc, ec - sc);
	} else {
		HXString prefix = lines[sl].substr(0, sc);
		HXString suffix = lines[el].substr(ec);
		lines[sl] = prefix + suffix;
		lines.erase(lines.begin() + sl + 1, lines.begin() + el + 1);
	}

	st.CursorLine = sl;
	st.CursorColumn = sc;
	st.DesiredColumn = sc;
	ClearSelection(st);
}

static HXString GetSelectedText(const std::vector<HXString> &lines, const EditorState &st) {
	if (!HasRealSelection(st)) return HXStr("");

	int sl, sc, el, ec;
	NormalizeSelection(st, sl, sc, el, ec);

	if (sl == el) {
		return lines[sl].substr(sc, ec - sc);
	}

	HXString result = lines[sl].substr(sc);
	for (int i = sl + 1; i < el; ++i) {
		result += HXStr("\r\n");
		result += lines[i];
	}
	result += HXStr("\r\n");
	result += lines[el].substr(0, ec);
	return result;
}

static void InsertTextAtCursor(std::vector<HXString> &lines, EditorState &st, const HXString &text) {
	DeleteSelection(lines, st);

	for (wchar_t ch : text) {
		if (ch == L'\r') continue;
		if (ch == L'\n') {
			HXString after = lines[st.CursorLine].substr(st.CursorColumn);
			lines[st.CursorLine].erase(st.CursorColumn);
			lines.insert(lines.begin() + st.CursorLine + 1, after);
			++st.CursorLine;
			st.CursorColumn = 0;
			continue;
		}
		if (ch >= L' ') {
			lines[st.CursorLine].insert(st.CursorColumn, 1, ch);
			++st.CursorColumn;
		}
	}

	st.DesiredColumn = st.CursorColumn;
	ClearSelection(st);
	ResetBlink(st);
}

static std::pair<int, int> MoveCursorPosition(const std::vector<HXString> &lines, EditorState &st, int keyCode) {
	int line = st.CursorLine;
	int col = st.CursorColumn;

	switch (keyCode) {
	case VK_LEFT:
		if (col > 0) {
			--col;
		} else if (line > 0) {
			--line;
			col = static_cast<int>(lines[line].size());
		}
		break;
	case VK_RIGHT:
		if (col < static_cast<int>(lines[line].size())) {
			++col;
		} else if (line + 1 < static_cast<int>(lines.size())) {
			++line;
			col = 0;
		}
		break;
	case VK_UP:
		if (line > 0) {
			--line;
			col = std::min(st.DesiredColumn, static_cast<int>(lines[line].size()));
		}
		break;
	case VK_DOWN:
		if (line + 1 < static_cast<int>(lines.size())) {
			++line;
			col = std::min(st.DesiredColumn, static_cast<int>(lines[line].size()));
		}
		break;
	case VK_HOME:
		col = 0;
		break;
	case VK_END:
		col = static_cast<int>(lines[line].size());
		break;
	default:
		break;
	}

	return {line, col};
}

static void MoveCursor(std::vector<HXString> &lines, EditorState &st, int keyCode, bool selecting) {
	if (!selecting && HasRealSelection(st)) {
		int sl, sc, el, ec;
		NormalizeSelection(st, sl, sc, el, ec);
		if (keyCode == VK_LEFT || keyCode == VK_UP || keyCode == VK_HOME) {
			st.CursorLine = sl;
			st.CursorColumn = (keyCode == VK_HOME) ? 0 : sc;
		} else if (keyCode == VK_RIGHT || keyCode == VK_DOWN || keyCode == VK_END) {
			st.CursorLine = el;
			st.CursorColumn = (keyCode == VK_END) ? static_cast<int>(lines[el].size()) : ec;
		}
		st.DesiredColumn = st.CursorColumn;
		ClearSelection(st);
		ResetBlink(st);
		return;
	}

	if (selecting && !HasRealSelection(st)) {
		st.AnchorLine = st.CursorLine;
		st.AnchorCol = st.CursorColumn;
	}

	auto [line, col] = MoveCursorPosition(lines, st, keyCode);
	st.CursorLine = line;
	st.CursorColumn = col;
	if (keyCode != VK_UP && keyCode != VK_DOWN) {
		st.DesiredColumn = st.CursorColumn;
	}

	if (selecting) {
		SetSelectionFromAnchor(st, line, col);
	} else {
		ClearSelection(st);
	}
	ResetBlink(st);
}

static std::pair<int, int> HitTestEditor(HXPoint mouse, HXPoint position, int contentX,
                                         int charWidth, const TextEditorProfile &profile,
                                         const std::vector<HXString> &lines, const EditorState &st) {
	int clickedLine = (mouse.Y - position.Y + st.ScrollY) / profile.LineHeight;
	clickedLine = std::max(0, std::min(clickedLine, static_cast<int>(lines.size()) - 1));

	int clickedColumn = 0;
	if (mouse.X >= contentX + profile.PaddingX) {
		clickedColumn = (mouse.X - contentX - profile.PaddingX + st.ScrollX + charWidth / 2) / charWidth;
		clickedColumn = std::max(0, std::min(clickedColumn, static_cast<int>(lines[clickedLine].size())));
	}
	return {clickedLine, clickedColumn};
}

////////////////////////////////////////////////////////////
/// TextEditor


static void ProcessEditorIMEMessage(EditorState &st, std::vector<HXString> &lines, TextEditorProfile &profile) {
	if (profile.ReadOnly) return;
	auto &context = GetContext();
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed) continue;

		if (msg.IMEStartComposition) {
			st.InIMEComposition = true;
			st.IMECompositionString.clear();
			msg.Processed = true;
		}
		if (msg.IMEResult) {
			if (!msg.IMEResultString.empty()) {
				InsertTextAtCursor(lines, st, msg.IMEResultString);
			}
			st.InIMEComposition = false;
			st.LastIMEEndTime = clock();
			st.IMECompositionString.clear();
			msg.Processed = true;
		}
		if (msg.IMEComposition) {
			st.IMECompositionString = msg.IMECompositionString;
			if (!st.IMECompositionString.empty()) {
				st.InIMEComposition = true;
			}
			msg.Processed = true;
		}
		if (msg.IMEEndComposition) {
			st.InIMEComposition = false;
			st.LastIMEEndTime = clock();
			st.IMECompositionString.clear();
			msg.Processed = true;
		}
	}
}

static void UpdateEditorIMERect(const EditorState &st, const TextEditorProfile &profile, HXPoint position, int contentX, int charWidth, HXBufferPainter *painter) {
	if (!st.Focused) return;

	int cursorX = contentX + profile.PaddingX + st.CursorColumn * charWidth - st.ScrollX;
	int cursorY = position.Y + st.CursorLine * profile.LineHeight - st.ScrollY;

	HXFont editorFont{};
	editorFont.Family = HXStr("Consolas");
	HXGInt compWidth = 0;
	if (st.InIMEComposition && !st.IMECompositionString.empty()) {
		compWidth = painter->MeasureText(st.IMECompositionString, editorFont, profile.FontSize).CalWidth();
	}

	// IME candidate window should appear at the end of the composition string
	// (same as TextInput::GetCursorScreenPos which adds compWidth during composition)
	int imeX = cursorX + compWidth;
	int imeY = cursorY;

	auto &context = GetContext();
	HXGInt absX = imeX + context.CurrentWindow->ScrollerX + context.CurrentWindow->Where.X;
	HXGInt absY = imeY + context.CurrentWindow->ScrollerY + context.CurrentWindow->Where.Y;

	HXRect imeRect;
	imeRect.Left = absX;
	imeRect.Top = absY;
	imeRect.Right = absX + compWidth;
	imeRect.Bottom = absY + profile.LineHeight;

	if (imeRect.Left != st.IMELastRect.Left || imeRect.Top != st.IMELastRect.Top ||
	    imeRect.Right != st.IMELastRect.Right || HX::HXIMEPendingStartComposition() ||
	    st.InIMEComposition) {
		context.OSAPI->SetIMERect(imeRect);
		const_cast<EditorState&>(st).IMELastRect = imeRect;
	}
}

void TextEditor(const HXString &id, std::vector<HXString> &lines, TextEditorProfile &profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto position = CurrentLayoutPosition();
	auto *layout = CurrentLayout();
	int actualWidth = profile.Size.X;
	int actualHeight = profile.Size.Y;
	if (layout) {
		if (actualWidth <= 0) actualWidth = layout->AvailableRegion.Right - position.X;
		if (actualHeight <= 0) actualHeight = layout->AvailableRegion.Bottom - position.Y;
	}
	actualWidth = std::max(1, actualWidth);
	actualHeight = std::max(1, actualHeight);

	if (RoughVisibility()) {
		LayoutNextControl({actualWidth, actualHeight});
		return;
	}

	if (AccVisibility({actualWidth, actualHeight})) {
		LayoutNextControl({actualWidth, actualHeight});
		return;
	}

	auto *state = GetStatePool().Get<EditorState>(id);
	auto  painter = CurrentPainter();

	HXRect controlRect{position.X, position.Y, position.X + actualWidth, position.Y + actualHeight};
	int contentX = position.X + (profile.ShowLineNumbers ? profile.LineNumberWidth : 0);
	HXFont editorFont{};
	editorFont.Family = HXStr("Consolas");
	int charWidth = profile.CharWidth;
	if (charWidth <= 0) {
		charWidth = std::max(1, painter->MeasureText(HXStr("M"), editorFont, profile.FontSize).CalWidth());
	}

	// Ensure at least one line exists
	if (lines.empty()) lines.push_back(HXStr(""));

	ClampCursor(*state, lines);
	bool revealCursor = false;
	if (profile.JumpToLine >= 0) {
		state->CursorLine = std::max(0, std::min(profile.JumpToLine, static_cast<int>(lines.size()) - 1));
		state->CursorColumn = std::max(0, std::min(profile.JumpToColumn, static_cast<int>(lines[state->CursorLine].size())));
		state->DesiredColumn = state->CursorColumn;
		state->ScrollX = 0;
		state->Focused = true;
		ClearSelection(*state);
		ResetBlink(*state);
		profile.JumpToLine = -1;
		revealCursor = true;
	}

	int maxLineChars = 0;
	for (const auto &line : lines) {
		maxLineChars = std::max(maxLineChars, static_cast<int>(line.size()));
	}
	int totalHeight = static_cast<int>(lines.size()) * profile.LineHeight;
	int totalWidth = maxLineChars * charWidth + profile.PaddingX * 2;
	int scrollbarSize = std::max(8, profile.ScrollbarSize);
	int rawTextW = std::max(1, actualWidth - (profile.ShowLineNumbers ? profile.LineNumberWidth : 0));
	int rawTextH = std::max(1, actualHeight);
	bool needVScroll = totalHeight > rawTextH;
	int textW = rawTextW - (needVScroll ? scrollbarSize : 0);
	bool needHScroll = totalWidth > textW;
	int textH = rawTextH - (needHScroll ? scrollbarSize : 0);
	if (!needVScroll && totalHeight > textH) {
		needVScroll = true;
		textW = rawTextW - scrollbarSize;
		needHScroll = totalWidth > textW;
		textH = rawTextH - (needHScroll ? scrollbarSize : 0);
	}
	textW = std::max(1, textW);
	textH = std::max(1, textH);
	HXRect textRect{contentX, position.Y, contentX + textW, position.Y + textH};
	HXRect gutterRect{position.X, position.Y, contentX, position.Y + textH};
	HXRect vTrack{contentX + textW, position.Y, contentX + textW + scrollbarSize, position.Y + textH};
	HXRect hTrack{contentX, position.Y + textH, contentX + textW, position.Y + textH + scrollbarSize};
	int maxScrollY = std::max(0, totalHeight - textH);
	int maxScrollX = std::max(0, totalWidth - textW);
	state->ScrollY = std::max(0, std::min(state->ScrollY, maxScrollY));
	state->ScrollX = std::max(0, std::min(state->ScrollX, maxScrollX));

	auto makeVThumb = [&]() {
		int trackH = std::max(1, vTrack.CalHeight());
		int thumbH = std::max(24, trackH * textH / std::max(textH, totalHeight));
		int thumbY = vTrack.Top + (maxScrollY > 0 ? state->ScrollY * (trackH - thumbH) / maxScrollY : 0);
		return HXRect{vTrack.Left, thumbY, vTrack.Right, thumbY + thumbH};
	};
	auto makeHThumb = [&]() {
		int trackW = std::max(1, hTrack.CalWidth());
		int thumbW = std::max(32, trackW * textW / std::max(textW, totalWidth));
		int thumbX = hTrack.Left + (maxScrollX > 0 ? state->ScrollX * (trackW - thumbW) / maxScrollX : 0);
		return HXRect{thumbX, hTrack.Top, thumbX + thumbW, hTrack.Bottom};
	};
	HXRect vThumb = needVScroll ? makeVThumb() : HXRect{0, 0, 0, 0};
	HXRect hThumb = needHScroll ? makeHThumb() : HXRect{0, 0, 0, 0};

	// ---- Scrollbar interaction ----
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.MouseAction) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (msg.MouseLeftPressed && needVScroll && vThumb.PointInside(mouse)) {
			state->DraggingVScroll = true;
			state->DragStartY = mouse.Y;
			state->DragScrollY = state->ScrollY;
			msg.Processed = true;
		}
		if (msg.MouseLeftPressed && needHScroll && hThumb.PointInside(mouse)) {
			state->DraggingHScroll = true;
			state->DragStartX = mouse.X;
			state->DragScrollX = state->ScrollX;
			msg.Processed = true;
		}
		if (state->DraggingVScroll) {
			if (msg.MouseLeftRelease) {
				state->DraggingVScroll = false;
			} else {
				int trackH = std::max(1, vTrack.CalHeight());
				int thumbH = std::max(1, vThumb.CalHeight());
				int delta = mouse.Y - state->DragStartY;
				state->ScrollY = state->DragScrollY + delta * maxScrollY / std::max(1, trackH - thumbH);
				state->ScrollY = std::max(0, std::min(state->ScrollY, maxScrollY));
			}
			msg.Processed = true;
		}
		if (state->DraggingHScroll) {
			if (msg.MouseLeftRelease) {
				state->DraggingHScroll = false;
			} else {
				int trackW = std::max(1, hTrack.CalWidth());
				int thumbW = std::max(1, hThumb.CalWidth());
				int delta = mouse.X - state->DragStartX;
				state->ScrollX = state->DragScrollX + delta * maxScrollX / std::max(1, trackW - thumbW);
				state->ScrollX = std::max(0, std::min(state->ScrollX, maxScrollX));
			}
			msg.Processed = true;
		}
	}

	// ---- Mouse focus, cursor and drag selection ----
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.MouseAction) continue;

		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		bool inside = textRect.PointInside(mouse) || gutterRect.PointInside(mouse);
		if (inside || state->DraggingSelection) {
			context.FrameCursor = HXCursorStyle::Editing;
		}

		if (state->DraggingSelection && !msg.MouseLeftRelease) {
			auto [line, col] = HitTestEditor(mouse, position, contentX, charWidth, profile, lines, *state);
			SetSelectionFromAnchor(*state, line, col);
			state->DesiredColumn = state->CursorColumn;
			ResetBlink(*state);
			revealCursor = true;
			msg.Processed = true;
			continue;
		}

		if (msg.MouseLeftRelease && state->DraggingSelection) {
			state->DraggingSelection = false;
			msg.Processed = true;
			continue;
		}

		if (!msg.MouseLeftPressed) continue;

		if (!inside) {
			if (state->Focused && state->InIMEComposition) {
				context.OSAPI->CancelIMEComposition();
				state->InIMEComposition = false;
				state->IMECompositionString.clear();
			}
			state->Focused = false;
			state->DraggingSelection = false;
			continue;
		}

		msg.Processed = true;
		state->Focused = true;
		auto [clickedLine, clickedColumn] = HitTestEditor(mouse, position, contentX, charWidth, profile, lines, *state);

		clock_t now = clock();
		bool isMultiClick = (now - state->LastClickTime < 500) &&
		                    (clickedLine == state->LastClickLine) &&
		                    (std::abs(clickedColumn - state->LastClickColumn) <= 2);
		if (isMultiClick) {
			state->ClickCount++;
		} else {
			state->ClickCount = 0;
		}

		if (state->ClickCount >= 2) {
			// Triple-click: select entire line
			int lineLen = static_cast<int>(lines[clickedLine].size());
			state->CursorLine = clickedLine;
			state->CursorColumn = lineLen;
			state->AnchorLine = clickedLine;
			state->AnchorCol = 0;
			state->SelStartLine = clickedLine;
			state->SelStartCol = 0;
			state->SelEndLine = clickedLine;
			state->SelEndCol = lineLen;
			state->HasSelection = lineLen > 0;
			state->DraggingSelection = false;
		} else if (state->ClickCount == 1) {
			// Double-click: select word
			auto &line = lines[clickedLine];
			int start = clickedColumn;
			int end = clickedColumn;

			auto isWordChar = [](wchar_t c) {
				return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
				       (c >= L'0' && c <= L'9') || c == L'_';
			};

			if (clickedColumn < static_cast<int>(line.size()) && !isWordChar(line[clickedColumn])) {
				start = clickedColumn;
				end = clickedColumn + 1;
			} else {
				while (start > 0 && isWordChar(line[start - 1])) --start;
				while (end < static_cast<int>(line.size()) && isWordChar(line[end])) ++end;
			}

			state->CursorLine = clickedLine;
			state->CursorColumn = end;
			state->AnchorLine = clickedLine;
			state->AnchorCol = start;
			state->SelStartLine = clickedLine;
			state->SelStartCol = start;
			state->SelEndLine = clickedLine;
			state->SelEndCol = end;
			state->HasSelection = start != end;
			state->DraggingSelection = false;
		} else if (msg.InShift) {
			if (!HasRealSelection(*state)) {
				state->AnchorLine = state->CursorLine;
				state->AnchorCol = state->CursorColumn;
			}
			SetSelectionFromAnchor(*state, clickedLine, clickedColumn);
			state->DraggingSelection = true;
		} else {
			state->CursorLine = clickedLine;
			state->CursorColumn = clickedColumn;
			state->DesiredColumn = clickedColumn;
			ClearSelection(*state);
			state->DraggingSelection = true;
		}

		state->LastClickTime = now;
		state->LastClickLine = clickedLine;
		state->LastClickColumn = clickedColumn;
		state->DesiredColumn = state->CursorColumn;
		ResetBlink(*state);
		revealCursor = true;
	}

	// ---- Keyboard input ----
	if (state->Focused) {
		ProcessEditorIMEMessage(*state, lines, profile);

		for (auto &msg : context.MessageQuery) {
			if (msg.Processed) continue;

			if (msg.KeyPressed && msg.InCtrl) {
				wchar_t key = static_cast<wchar_t>(std::towupper(msg.KeyCode));
				if (key == L'A') {
					state->CursorLine = static_cast<int>(lines.size()) - 1;
					state->CursorColumn = static_cast<int>(lines.back().size());
					state->AnchorLine = 0;
					state->AnchorCol = 0;
					state->SelStartLine = 0;
					state->SelStartCol = 0;
					state->SelEndLine = state->CursorLine;
					state->SelEndCol = state->CursorColumn;
					state->HasSelection = true;
					ResetBlink(*state);
					revealCursor = true;
					msg.Processed = true;
					continue;
				}
				if (key == L'C') {
					if (HasRealSelection(*state)) {
						context.OSAPI->CopyToClipboard(GetSelectedText(lines, *state));
					}
					msg.Processed = true;
					continue;
				}
				if (key == L'X') {
					if (!profile.ReadOnly && HasRealSelection(*state)) {
						context.OSAPI->CopyToClipboard(GetSelectedText(lines, *state));
						DeleteSelection(lines, *state);
						revealCursor = true;
					}
					msg.Processed = true;
					continue;
				}
				if (key == L'V') {
					if (!profile.ReadOnly) {
						InsertTextAtCursor(lines, *state, context.OSAPI->ReadFromClipboard());
						revealCursor = true;
					}
					msg.Processed = true;
					continue;
				}
			}

			if (msg.InputChar && !msg.InCtrl) {
				msg.Processed = true;
				// Suppress normal characters during IME composition or shortly after
				if (state->InIMEComposition || !state->IMECompositionString.empty() ||
				    clock() - state->LastIMEEndTime < 50) {
					continue;
				}
				if (profile.ReadOnly) continue;
				if (msg.Char == L'\r' || msg.Char == L'\n') {
					InsertTextAtCursor(lines, *state, HXStr("\n"));
				} else if (msg.Char == L'\t') {
					InsertTextAtCursor(lines, *state, HXStr("    "));
				} else if (msg.Char >= L' ') {
					InsertTextAtCursor(lines, *state, HXString(1, static_cast<wchar_t>(msg.Char)));
				}
				revealCursor = true;
				continue;
			}

			if (!msg.KeyPressed) continue;

			bool handled = true;
			switch (msg.KeyCode) {
			case VK_LEFT:
			case VK_RIGHT:
			case VK_UP:
			case VK_DOWN:
			case VK_HOME:
			case VK_END:
				MoveCursor(lines, *state, msg.KeyCode, msg.InShift);
				revealCursor = true;
				break;
			case VK_BACK: {
				if (!profile.ReadOnly) {
					if (HasRealSelection(*state)) {
						DeleteSelection(lines, *state);
					} else if (state->CursorColumn > 0) {
						lines[state->CursorLine].erase(state->CursorColumn - 1, 1);
						--state->CursorColumn;
					} else if (state->CursorLine > 0) {
						int prevLen = static_cast<int>(lines[state->CursorLine - 1].size());
						lines[state->CursorLine - 1] += lines[state->CursorLine];
						lines.erase(lines.begin() + state->CursorLine);
						--state->CursorLine;
						state->CursorColumn = prevLen;
					}
					state->DesiredColumn = state->CursorColumn;
					ClearSelection(*state);
					ResetBlink(*state);
					revealCursor = true;
				}
				break;
			}
			case VK_DELETE: {
				if (!profile.ReadOnly) {
					if (HasRealSelection(*state)) {
						DeleteSelection(lines, *state);
					} else if (state->CursorColumn < static_cast<int>(lines[state->CursorLine].size())) {
						lines[state->CursorLine].erase(state->CursorColumn, 1);
					} else if (state->CursorLine + 1 < static_cast<int>(lines.size())) {
						lines[state->CursorLine] += lines[state->CursorLine + 1];
						lines.erase(lines.begin() + state->CursorLine + 1);
					}
					ClearSelection(*state);
					ResetBlink(*state);
					revealCursor = true;
				}
				break;
			}
			default:
				handled = false;
				break;
			}
			if (handled) msg.Processed = true;
		}
	}

	// ---- Wheel scrolling ----
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.MouseWheel) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (controlRect.PointInside(mouse)) {
			int wheelDelta = msg.MouseWheelUp ? 1 : -1;
			if (msg.InShift && needHScroll) {
				state->ScrollX += wheelDelta * charWidth * 6;
			} else {
				state->ScrollY += wheelDelta * profile.LineHeight * 3;
			}
			msg.Processed = true;
		}
	}

	state->ScrollY = std::max(0, std::min(state->ScrollY, maxScrollY));
	state->ScrollX = std::max(0, std::min(state->ScrollX, maxScrollX));

	if (revealCursor) {
		int cursorY = state->CursorLine * profile.LineHeight;
		if (cursorY < state->ScrollY) {
			state->ScrollY = cursorY;
		} else if (cursorY + profile.LineHeight > state->ScrollY + textH) {
			state->ScrollY = cursorY + profile.LineHeight - textH;
		}
		int cursorXWorld = profile.PaddingX + state->CursorColumn * charWidth;
		if (cursorXWorld < state->ScrollX) {
			state->ScrollX = cursorXWorld;
		} else if (cursorXWorld + charWidth > state->ScrollX + textW) {
			state->ScrollX = cursorXWorld + charWidth - textW;
		}
	}
	state->ScrollY = std::max(0, std::min(state->ScrollY, maxScrollY));
	state->ScrollX = std::max(0, std::min(state->ScrollX, maxScrollX));
	vThumb = needVScroll ? makeVThumb() : HXRect{0, 0, 0, 0};
	hThumb = needHScroll ? makeHThumb() : HXRect{0, 0, 0, 0};

	// ---- Rendering ----
	HXColor editorBackground = theme.TextInputBackground;
	HXColor gutterBackground = profile.GutterBackground;
	HXColor currentLineColor = profile.CurrentLineBackground;
	HXColor focusAccent = theme.TextInputCursorColor;

	painter->Begin();
	painter->DrawFilledRectangle(controlRect, editorBackground, editorBackground);
	if (profile.ShowLineNumbers) {
		painter->DrawFilledRectangle(gutterRect, gutterBackground, gutterBackground);
		painter->DrawLine({contentX, position.Y}, {contentX, position.Y + textH}, theme.SplitLine);
	}
	painter->DrawRectangle(controlRect, state->Focused ? focusAccent : theme.TextInputBorder);
	if (state->Focused) {
		painter->DrawLine({controlRect.Left + 1, controlRect.Top + 1},
		                  {controlRect.Right - 1, controlRect.Top + 1}, focusAccent);
		painter->DrawLine({controlRect.Left + 1, controlRect.Top + 2},
		                  {controlRect.Right - 1, controlRect.Top + 2}, theme.TextInputCursorColor);
	}
	if (needVScroll) {
		painter->DrawFilledRectangle(vTrack, theme.ScrollerBackground, theme.ScrollerBackground);
		painter->DrawFilledRectangle(vThumb, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
	}
	if (needHScroll) {
		painter->DrawFilledRectangle(hTrack, theme.ScrollerBackground, theme.ScrollerBackground);
		painter->DrawFilledRectangle(hThumb, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
	}
	painter->End();

	int firstLine = state->ScrollY / profile.LineHeight;
	int lastLine  = std::min(static_cast<int>(lines.size()) - 1,
	                         firstLine + textH / profile.LineHeight + 1);

	for (int i = firstLine; i <= lastLine; ++i) {
		int rowY = position.Y + i * profile.LineHeight - state->ScrollY;

		// Line number
		if (profile.ShowLineNumbers) {
			painter->Begin();
			HXString num = std::to_wstring(i + 1);
			painter->DrawText(num, editorFont, {position.X + 8, rowY + 3}, profile.LineNumberColor, profile.FontSize - 2);
			painter->End();
		}

		// Highlight current line
		if (state->Focused && i == state->CursorLine) {
			painter->Begin();
			painter->DrawFilledRectangle({textRect.Left, rowY, textRect.Right, std::min(textRect.Bottom, rowY + profile.LineHeight)},
			                             currentLineColor, currentLineColor);
			painter->End();
		}

		// Selection highlight
		if (HasRealSelection(*state)) {
			int selSL = state->SelStartLine, selSC = state->SelStartCol;
			int selEL = state->SelEndLine,   selEC = state->SelEndCol;
			if (selSL > selEL || (selSL == selEL && selSC > selEC)) {
				std::swap(selSL, selEL); std::swap(selSC, selEC);
			}
			if (i >= selSL && i <= selEL) {
				int lineSelStart = (i == selSL) ? selSC : 0;
				int lineSelEnd   = (i == selEL) ? selEC : static_cast<int>(lines[i].size());
				if (lineSelStart < lineSelEnd) {
					int selX1 = contentX + profile.PaddingX + lineSelStart * charWidth - state->ScrollX;
					int selX2 = contentX + profile.PaddingX + lineSelEnd   * charWidth - state->ScrollX;
					selX1 = std::max(selX1, textRect.Left);
					selX2 = std::min(selX2, textRect.Right);
					if (selX1 >= selX2) continue;
					painter->Begin();
					painter->DrawFilledRectangle({selX1, rowY, selX2, std::min(textRect.Bottom, rowY + profile.LineHeight)},
					                             theme.TextInputSelectedBackground, theme.TextInputSelectedBackground);
					painter->End();
				}
			}
		}

		// Tokenized line rendering
		auto tokens = profile.Lexer ? profile.Lexer(lines[i]) : DefaultCppLexer(lines[i]);
		int  tx     = contentX + profile.PaddingX - state->ScrollX;
		bool compDrawn = false;
		HXGInt compWidth = 0;
		if (state->InIMEComposition && !state->IMECompositionString.empty() && i == state->CursorLine) {
			compWidth = painter->MeasureText(state->IMECompositionString, editorFont, profile.FontSize).CalWidth();
		}
		int charsDrawn = 0;
		for (const auto &tok : tokens) {
			HXColor color = GetTokenColor(tok.Type, theme, profile);
			for (int ch = 0; ch < static_cast<int>(tok.Text.size()); ++ch) {
				// Draw IME composition string at cursor column, pushing subsequent text to the right
				if (!compDrawn && i == state->CursorLine && charsDrawn == state->CursorColumn &&
				    state->InIMEComposition && !state->IMECompositionString.empty()) {
					int compX = tx + ch * charWidth;
					if (compX >= textRect.Left && compX <= textRect.Right && rowY + profile.LineHeight <= textRect.Bottom + profile.LineHeight) {
						painter->Begin();
						painter->DrawText(state->IMECompositionString, editorFont, {compX, rowY + 3}, theme.TextInputText, profile.FontSize);
						painter->DrawLine({compX, rowY + profile.LineHeight - 2}, {compX + compWidth, rowY + profile.LineHeight - 2}, theme.TextInputText);
						painter->End();
					}
					compDrawn = true;
				}

				int chX = tx + ch * charWidth + (compDrawn ? compWidth : 0);
				if (chX + charWidth >= textRect.Left && chX < textRect.Right && rowY + profile.LineHeight <= textRect.Bottom + profile.LineHeight) {
					painter->Begin();
					painter->DrawText(tok.Text.substr(ch, 1), editorFont, {chX, rowY + 3}, color, profile.FontSize);
					painter->End();
				}
				charsDrawn++;
			}
			tx += static_cast<int>(tok.Text.size()) * charWidth;
			if (tx > textRect.Right) break;
		}
		// Composition string at the very end of the line
		if (!compDrawn && i == state->CursorLine && charsDrawn == state->CursorColumn &&
		    state->InIMEComposition && !state->IMECompositionString.empty()) {
			int compX = tx;
			if (compX >= textRect.Left && compX <= textRect.Right && rowY + profile.LineHeight <= textRect.Bottom + profile.LineHeight) {
				painter->Begin();
				painter->DrawText(state->IMECompositionString, editorFont, {compX, rowY + 3}, theme.TextInputText, profile.FontSize);
				painter->DrawLine({compX, rowY + profile.LineHeight - 2}, {compX + compWidth, rowY + profile.LineHeight - 2}, theme.TextInputText);
				painter->End();
			}
		}
	}

	// Cursor
	if (clock() - state->LastCursorBlink > profile.BlinkIntervalMs) {
		state->CursorVisible = !state->CursorVisible;
		state->LastCursorBlink = clock();
	}
	// Cursor
	int cursorBaseX = contentX + profile.PaddingX + state->CursorColumn * charWidth - state->ScrollX;
	int cursorBaseY = position.Y + state->CursorLine * profile.LineHeight - state->ScrollY;
	int cursorDrawX = cursorBaseX;
	int cursorDrawY = cursorBaseY;
	HXGInt compWidth = 0;
	if (state->InIMEComposition && !state->IMECompositionString.empty()) {
		compWidth = painter->MeasureText(state->IMECompositionString, editorFont, profile.FontSize).CalWidth();
		cursorDrawX += compWidth;
	}
	if (state->Focused && state->CursorVisible) {
		if (cursorDrawX >= textRect.Left && cursorDrawX <= textRect.Right && cursorDrawY + profile.LineHeight > textRect.Top && cursorDrawY < textRect.Bottom) {
			painter->Begin();
			painter->DrawFilledRectangle({cursorDrawX, cursorDrawY + 2, cursorDrawX + 2, std::min(textRect.Bottom, cursorDrawY + profile.LineHeight - 2)},
			                             theme.TextInputCursorColor, theme.TextInputCursorColor);
			painter->End();
		}
	}

	UpdateEditorIMERect(*state, profile, position, contentX, charWidth, painter);

	LayoutNextControl(profile.Size);
}

}
