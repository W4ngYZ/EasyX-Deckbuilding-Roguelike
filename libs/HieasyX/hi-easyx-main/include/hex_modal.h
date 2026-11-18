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
 * \file hex_modal.h
 * \brief Modal dialog system for HiEasyX
 *
 * Modal dialogs block interaction with the underlying UI and render a centered
 * dialog box with a backdrop. Controls placed between BeginModal/EndModal are
 * laid out inside the dialog content area.
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {

/**
 * Profile for a modal dialog.
 */
struct ModalProfile {
	HXPoint Size = {400, 300};
	HXString Title;
	bool CloseOnClickOutside = true;
	bool CloseButton = true;
	HXColor BackdropColor = {0, 0, 0, 115};

	int TitleBarHeight   = 36;
	int CloseButtonSize  = 32;
};

/**
 * Open a modal dialog by Id. The dialog will appear on the next frame.
 */
void OpenModal(const HXString &Id);

/**
 * Close a modal dialog by Id.
 */
void CloseModal(const HXString &Id);

/**
 * Check whether a modal dialog is currently open.
 */
bool IsModalOpen(const HXString &Id);

/**
 * Begin a modal dialog region.
 * @return True while the modal is open. Place controls inside the if-block.
 */
bool BeginModal(const HXString &Id, ModalProfile &Profile);

/**
 * End a modal dialog region. Must match a previous BeginModal().
 */
void EndModal();

/**
 * Render backdrop overlays for all active modals.
 * Called automatically by HX::Render(); user code should not call this.
 */
void RenderModalBackdrops(HXBufferPainter *Painter);

}
