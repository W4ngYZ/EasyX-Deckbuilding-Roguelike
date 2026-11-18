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
 * \file hex_shortcut.cpp
 * \brief Global shortcut system implementation
 */

#include <include/hex_shortcut.h>
#include <include/hex.h>

#include <vector>

namespace HX {

struct HXRegisteredShortcut {
	HXShortcut          Shortcut;
	std::function<void()> Callback;
};

static std::vector<HXRegisteredShortcut> G_Shortcuts;

void RegisterShortcut(const HXShortcut &shortcut, std::function<void()> callback) {
	G_Shortcuts.push_back({shortcut, std::move(callback)});
}

void UnregisterShortcut(int key, bool ctrl, bool shift, bool alt) {
	for (auto it = G_Shortcuts.begin(); it != G_Shortcuts.end();) {
		if (it->Shortcut.Key == key && it->Shortcut.Ctrl == ctrl &&
		    it->Shortcut.Shift == shift && it->Shortcut.Alt == alt) {
			it = G_Shortcuts.erase(it);
		} else {
			++it;
		}
	}
}

void ProcessShortcuts() {
	auto &context = GetContext();
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.KeyPressed) continue;
		for (const auto &reg : G_Shortcuts) {
			if (reg.Shortcut.Key == msg.KeyCode &&
			    reg.Shortcut.Ctrl == msg.InCtrl &&
			    reg.Shortcut.Shift == msg.InShift &&
			    reg.Shortcut.Alt == msg.InAlt) {
				if (reg.Callback) reg.Callback();
				msg.Processed = true;
				break;
			}
		}
	}
}

}
