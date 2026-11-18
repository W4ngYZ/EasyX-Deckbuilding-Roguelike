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
 * \file hex_shortcut.h
 * \brief Global shortcut system for HiEasyX
 */

#pragma once

#include <include/hex_string.h>
#include <functional>

namespace HX {

/**
 * Keyboard shortcut descriptor
 */
struct HXShortcut {
	bool Ctrl  = false;
	bool Shift = false;
	bool Alt   = false;
	int  Key   = 0; // VK_* code

	bool operator==(const HXShortcut &other) const {
		return Ctrl == other.Ctrl && Shift == other.Shift && Alt == other.Alt && Key == other.Key;
	}
};

/**
 * Register a global shortcut. The callback is invoked when the shortcut keys are pressed.
 */
void RegisterShortcut(const HXShortcut &shortcut, std::function<void()> callback);

/**
 * Unregister a global shortcut by its key combination.
 */
void UnregisterShortcut(int key, bool ctrl, bool shift, bool alt);

/**
 * Process all registered shortcuts against the current frame's messages.
 * Called automatically by HX::End(); user code should not call this directly.
 */
void ProcessShortcuts();

}
