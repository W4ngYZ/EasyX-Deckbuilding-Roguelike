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
 * \file hex_editor.h
 * \brief Code editor with virtual scrolling and token coloring
 */

#pragma once

#include <include/hex_string.h>
#include <include/hex_geo.h>
#include <include/impl/hex_impl.h>
#include <vector>
#include <functional>

namespace HX {

/**
 * Token type produced by the lexer
 */
enum class EditorTokenType {
	Unknown,
	Keyword,
	String,
	Comment,
	Number,
	Identifier,
	Operator,
	Whitespace
};

/**
 * A single token on a line
 */
struct EditorToken {
	HXString        Text;
	EditorTokenType Type = EditorTokenType::Unknown;
};

/**
 * Profile for the code editor
 */
struct TextEditorProfile {
	HXPoint Size          = {600, 400};
	int     LineHeight    = 22;
	int     CharWidth     = 0;  // 0 = auto-measure from the editor font
	int     FontSize      = 20;
	int     PaddingX      = 10;
	int     ScrollbarSize = 12;
	bool    ShowLineNumbers = true;
	int     LineNumberWidth = 50;
	bool    ReadOnly      = false;
	int     JumpToLine    = -1;
	int     JumpToColumn  = 0;
	int     BlinkIntervalMs = 530;
	HXColor LineNumberColor{99, 116, 139, 255};
	HXColor GutterBackground{13, 17, 23, 255};
	HXColor CurrentLineBackground{24, 31, 42, 255};

	/**
	 * Optional custom lexer. Receives a single line and returns tokens.
	 * If empty, the built-in C++ lexer is used.
	 */
	std::function<std::vector<EditorToken>(const HXString &)> Lexer;

	/**
	 * Optional custom token color mapping. If empty, theme-backed defaults are used.
	 */
	std::function<HXColor(EditorTokenType)> TokenColorCallback;
};

/**
 * Render a code editor.
 * Mutates the lines vector (insert/delete/split lines) and cursor state on interaction.
 */
void TextEditor(const HXString &id, std::vector<HXString> &lines, TextEditorProfile &profile);

}
