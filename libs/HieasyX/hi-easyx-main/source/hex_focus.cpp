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
 * \file hex_focus.cpp
 * \brief Focus Stack implementation
 */

#include <include/hex.h>
#include <include/hex_focus.h>

////////////////////////////////////////////////////////////
/// HXFocusStack

void HXFocusStack::Push(void *profile, const HXString &scopeId) {
	Stack.push_back(profile);
	ScopeIds.push_back(scopeId);
}

void HXFocusStack::Pop() {
	if (!Stack.empty()) {
		Stack.pop_back();
		ScopeIds.pop_back();
	}
}

void HXFocusStack::PopUntil(void *profile) {
	while (!Stack.empty() && Stack.back() != profile) {
		Stack.pop_back();
		ScopeIds.pop_back();
	}
}

void HXFocusStack::Clear() {
	Stack.clear();
	ScopeIds.clear();
}

bool HXFocusStack::IsTop(void *profile) const {
	return !Stack.empty() && Stack.back() == profile;
}

bool HXFocusStack::IsWithinScope(void *profile) const {
	for (void *p : Stack) {
		if (p == profile) return true;
	}
	return false;
}

bool HXFocusStack::IsEmpty() const {
	return Stack.empty();
}

void *HXFocusStack::TopProfile() const {
	return Stack.empty() ? nullptr : Stack.back();
}

HXString HXFocusStack::TopScopeId() const {
	return ScopeIds.empty() ? HXString{} : ScopeIds.back();
}

namespace HX {

////////////////////////////////////////////////////////////
/// Global helpers

void PushFocusScope(void *profile, const HXString &scopeId) {
	GetContext().FocusStack.Push(profile, scopeId);
}

void PopFocusScope() {
	GetContext().FocusStack.Pop();
}

void PopFocusScopeUntil(void *profile) {
	GetContext().FocusStack.PopUntil(profile);
}

bool IsFocusScopeTop(void *profile) {
	return GetContext().FocusStack.IsTop(profile);
}

bool IsWithinFocusScope(void *profile) {
	return GetContext().FocusStack.IsWithinScope(profile);
}

HXString GetTopFocusScopeId() {
	return GetContext().FocusStack.TopScopeId();
}

////////////////////////////////////////////////////////////
/// Capture-phase helpers

bool ConsumeMessagesOutsideRect(const HXRect &rect, bool detectLeftPressOutside) {
	return ConsumeMouseEventsOutsideRect(rect, detectLeftPressOutside);
}

} // namespace HX
