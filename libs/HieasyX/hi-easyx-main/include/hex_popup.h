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
 * \file hex_popup.h
 * \brief Popup menus and context menus for HiEasyX
 *
 * Provides lightweight overlay menus that render above all windows.
 * Suitable for right-click context menus, overflow menus, etc.
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>
#include <vector>

namespace HX {

/**
 * A single item inside a popup menu.
 */
struct PopupMenuItem {
	HXString Label;
	bool     Enabled   = true;
	bool     Separator = false;
};

/**
 * Profile for a popup / context menu.
 */
struct PopupMenuProfile {
	HXPoint Position = {0, 0};
	bool    Open     = false;
	std::vector<PopupMenuItem> Items;
	HXGInt  Width = 180;

	int ItemHeight      = 28;
	int SeparatorHeight = 8;
	int Padding         = 8;
	int FontSize        = 16;
	HXColor BackgroundColor = {0, 0, 0, 0};
	HXColor BorderColor     = {0, 0, 0, 0};
	HXColor TextColor       = {0, 0, 0, 0};

	// Internal state
	HXGInt _hoveredItem = -1;
};

/**
 * Open a popup menu at the given screen position.
 */
void OpenPopupMenu(PopupMenuProfile &Profile, HXPoint ScreenPos);

/**
 * Close a popup menu.
 */
void ClosePopupMenu(PopupMenuProfile &Profile);

/**
 * Check whether a popup menu is currently open.
 */
bool IsPopupMenuOpen(PopupMenuProfile &Profile);

/**
 * Check whether any popup menu is currently open.
 */
bool IsAnyPopupMenuOpen();

/**
 * Process and queue a popup menu for rendering.
 * Call this every frame while the menu may be open.
 * @return The index of the clicked item, or -1 if nothing was clicked.
 */
HXGInt PopupMenu(PopupMenuProfile &Profile);

/**
 * Render all pending popup menus on top of everything else.
 * Called automatically by HX::Render(); user code should not call this.
 */
void RenderPendingPopupMenus(HXBufferPainter *Painter);

}
