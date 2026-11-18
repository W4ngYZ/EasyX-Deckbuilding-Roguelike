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
 * \file hex_menu.h
 * \brief Menu bar and dropdown menus for toolbars
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {

/**
 * Global profile for menu bar and dropdown menus.
 */
struct MenuBarProfile {
	int BarHeight       = 28;
	int ItemHeight      = 28;
	int FontSize        = 20;
	int SeparatorHeight = 8;
};

/**
 * Access the global menu bar profile for customization.
 */
MenuBarProfile& GetMenuBarProfile();

/**
 * Begin a menu bar region at the top of the current window.
 * Must be called right after HX::Window().
 */
void BeginMenuBar();

/**
 * End the menu bar region.
 */
void EndMenuBar();

/**
 * Begin a dropdown menu attached to the previous menu-bar label.
 * Returns true while the dropdown is open (hovered or clicked).
 *
 * Usage:
 *   if (HX::BeginMenu(HXStr("File"))) {
 *       if (HX::MenuItem(HXStr("New"))) { }
 *       HX::MenuSeparator();
 *       if (HX::MenuItem(HXStr("Exit"))) { }
 *       HX::EndMenu();
 *   }
 */
bool BeginMenu(const HXString &Label);

/**
 * End the current dropdown menu.
 */
void EndMenu();

/**
 * Insert a horizontal separator line inside a dropdown menu.
 * Must be called between BeginMenu/EndMenu.
 */
void MenuSeparator();

/**
 * A clickable menu item.
 * When called inside BeginMenu/EndMenu it renders as a dropdown row.
 * When called inside BeginMenuBar/EndMenuBar it renders as a top-level label.
 * @param Title The label of the item
 * @param Selected If true, the item is drawn as selected/active
 * @param Enabled If false, the item is grayed out and not interactive
 * @return True if the item was clicked this frame
 */
bool MenuItem(const HXString &Title, bool Selected = false, bool Enabled = true);

/**
 * Render any pending menu-bar dropdown overlays.
 * Called automatically by HX::Render(); user code should not call this.
 */
void RenderPendingMenuBarDropdowns(HXBufferPainter *Painter);

}
