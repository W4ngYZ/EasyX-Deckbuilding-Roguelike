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
 * \file hex_focus.h
 * \brief Focus Stack and event capture-phase helpers for HiEasyX
 *
 * The Focus Stack solves the "multiple overlays competing for input" problem
 * (Modal + PopupMenu + Dropdown open simultaneously). Only the topmost scope
 * on the stack is allowed to consume messages that fall outside its capture
 * rect, ensuring that clicking outside a popup does not also dismiss the modal
 * underneath.
 *
 * Usage inside an overlay control:
 *   HX::PushFocusScope(&Profile, Id);
 *   if (HX::IsFocusScopeTop(&Profile)) {
 *       bool clickedOutside = HX::ConsumeMessagesOutsideRect(captureRect, true);
 *       if (clickedOutside) CloseOverlay();
 *   }
 *   // ... process inside messages ...
 *   HX::PopFocusScope();
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>
#include <vector>

/**
 * A single entry on the focus stack.
 */
struct HXFocusScope {
	void *   Profile = nullptr; ///< Pointer to the overlay's profile (unique identity)
	HXString ScopeId;           ///< Human-readable ID for debugging
};

/**
 * Focus stack for coordinating input among modal overlays.
 *
 * In IMGUI the stack is rebuilt every frame; each overlay pushes when it
 * begins and pops when it ends.  At frame boundaries the stack is empty.
 */
struct HXFocusStack {
	std::vector<void *>    Stack;    // Profile pointers, top = back
	std::vector<HXString>  ScopeIds; // Parallel debug IDs

	void Push(void *profile, const HXString &scopeId);
	void Pop();
	void PopUntil(void *profile);
	void Clear();

	bool IsTop(void *profile) const;
	bool IsWithinScope(void *profile) const;
	bool IsEmpty() const;

	void *      TopProfile() const;
	HXString    TopScopeId() const;
};

namespace HX {

// ------------------------------------------------------------------
// Global helpers
// ------------------------------------------------------------------

/**
 * Push a focus scope.  Call this immediately after computing the overlay's
 * geometry so that subsequent scopes can be correctly ordered.
 */
void PushFocusScope(void *profile, const HXString &scopeId);

/**
 * Pop the topmost focus scope.  Must match a previous PushFocusScope.
 */
void PopFocusScope();

/**
 * Pop scopes until the given profile is on top (inclusive).
 */
void PopFocusScopeUntil(void *profile);

/**
 * Check whether the given profile is the current top of the focus stack.
 */
bool IsFocusScopeTop(void *profile);

/**
 * Check whether the given profile exists anywhere on the focus stack.
 */
bool IsWithinFocusScope(void *profile);

/**
 * Return the ScopeId of the current top scope, or empty string if none.
 */
HXString GetTopFocusScopeId();

// ------------------------------------------------------------------
// Capture-phase message filtering
// ------------------------------------------------------------------

/**
 * Consume (mark as Processed) all unprocessed mouse messages that fall
 * outside the given rectangle.
 *
 * @param rect                Screen-space rectangle of the focused region.
 * @param detectLeftPressOutside  If true, the function also reports whether
 *                            an unprocessed left-mouse-press occurred outside.
 * @return true iff detectLeftPressOutside was set and a left press was found
 *         outside the rectangle.
 */
bool ConsumeMessagesOutsideRect(const HXRect &rect, bool detectLeftPressOutside);

} // namespace HX
