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
 * \file hex_easyx.cpp
 * \brief The easyx enhance layer for HiEasyX
 */

#include <include/impl/EasyX/hex_easyx.h>

#include <graphics.h>

#include <mutex>

#ifdef UNICODE
	#pragma comment(lib,"EasyXw.lib")
#else
	#pragma comment(lib,"EasyXa.lib")
#endif

namespace HX {

std::mutex EasyXMuteLock;
WNDPROC    EasyXProcess;

LRESULT HiEasyXWindowProc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
	case WM_SIZE: {
		if (wparam != SIZE_MINIMIZED) {
			if (wparam != SIZE_RESTORED) {
				RECT Rect;

				GetWindowRect(handle, &Rect);

				Resize(nullptr, Rect.right - Rect.left - 16, Rect.bottom - Rect.top - 39);
			}
			else {
				RECT Rect;

				GetWindowRect(handle, &Rect);

				Resize(nullptr, LOWORD(lparam), HIWORD(lparam));
			}
		}

		return 0;
	}
	case WM_SIZING: {
		LockEasyXLock();

		if (wparam != SIZE_MINIMIZED) {
			if (wparam != SIZE_RESTORED) {
				RECT Rect;

				GetWindowRect(handle, &Rect);

				Resize(nullptr, Rect.right - Rect.left - 16, Rect.bottom - Rect.top - 39);
			} else {
				RECT Rect;

				GetWindowRect(handle, &Rect);

				Resize(nullptr, LOWORD(lparam), HIWORD(lparam));
			}
		}

		UnlockEasyXLock();

		return 0;
	}
	default: {
		return EasyXProcess(handle, message, wparam, lparam);
	}
	}
}

void EnableEasyXWindowEnhance() {
	const auto handle = GetHWnd();
	if (handle == nullptr) {
		return;
	}

	SetWindowLong(handle, GWL_STYLE, GetWindowLong(GetHWnd(), GWL_STYLE) | (WS_MAXIMIZEBOX));
	SetWindowLong(handle, GWL_STYLE, (GetWindowLong(GetHWnd(), GWL_STYLE) | WS_THICKFRAME));

	EasyXProcess = reinterpret_cast<WNDPROC>(GetWindowLongPtr(GetHWnd(), GWLP_WNDPROC));

	SetWindowLongPtr(handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HiEasyXWindowProc));
}

void LockEasyXLock() {
	while (!EasyXMuteLock.try_lock());
}

void UnlockEasyXLock() {
	EasyXMuteLock.unlock();
}

}
