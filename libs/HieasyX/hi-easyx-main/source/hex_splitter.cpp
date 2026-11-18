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
 * \file hex_splitter.cpp
 * \brief Draggable splitter implementation
 */

#include <include/hex.h>
#include <include/hex_splitter.h>

#include <algorithm>

#undef min
#undef max

namespace HX {

bool Splitter(const HXString &id, HXGInt &Size, SplitterProfile &Profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  layout  = CurrentLayout();
	auto  painter = CurrentPainter();

	if (!layout) {
		return false;
	}

	auto position = CurrentLayoutPosition();

	HXRect splitRect;
	if (Profile.Horizontal) {
		// Horizontal splitter: spans full available width, thickness height
		HXGInt width = layout->AvailableRegion.Right - layout->AvailableRegion.Left;
		splitRect = HXRect{position.X, position.Y, position.X + width, position.Y + Profile.Thickness};
	} else {
		// Vertical splitter: thickness width, spans full available height
		HXGInt height = layout->AvailableRegion.Bottom - layout->AvailableRegion.Top;
		splitRect = HXRect{position.X, position.Y, position.X + Profile.Thickness, position.Y + height};
	}

	// Hit-test for hover / drag
	Profile.OnHover = false;
	bool dragging   = Profile.InDrag;

	for (auto &msg : context.MessageQuery) {
		if (!msg.MouseAction) continue;
		if (msg.Processed) continue;

		HXPoint mouse = ClipCoord({msg.MouseX, msg.MouseY});

		if (splitRect.PointInside(mouse)) {
			Profile.OnHover = true;
			if (msg.MouseLeftPressed) {
				msg.Processed  = true;
				Profile.InDrag = true;
				Profile.DragStart = Profile.Horizontal ? msg.MouseY : msg.MouseX;
			}
		}

		if (msg.MouseLeftRelease) {
			Profile.InDrag = false;
		}
	}

	if (Profile.InDrag) {
		for (auto &msg : context.MessageQuery) {
			if (!msg.MouseAction) continue;
			if (msg.Processed) continue;
			msg.Processed = true;

			HXGInt currentPos = Profile.Horizontal ? msg.MouseY : msg.MouseX;
			HXGInt delta      = currentPos - Profile.DragStart;
			Profile.DragStart = currentPos;

			Size += delta;
			Size = std::max(Profile.MinSize, std::min(Profile.MaxSize, Size));
		}
	}

	// Draw splitter
	HXColor color = theme.SplitLine;
	if (Profile.OnHover || Profile.InDrag) {
		color = theme.ButtonOnHoverBorder;
	}

	if (Profile.Horizontal) {
		painter->Begin();
		painter->DrawFilledRectangle(splitRect, color, color);
		painter->End();
		LayoutNextControl({splitRect.CalWidth(), splitRect.CalHeight()});
	} else {
		painter->Begin();
		painter->DrawFilledRectangle(splitRect, color, color);
		painter->End();
		LayoutNextControl({splitRect.CalWidth(), splitRect.CalHeight()});
	}

	return Profile.InDrag;
}

} // namespace HX
