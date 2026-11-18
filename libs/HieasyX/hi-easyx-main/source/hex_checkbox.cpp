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
 * \file hex_checkbox.cpp
 * \brief The checkbox for HiEasyX
 */

#include <include/hex_checkbox.h>
#include <include/hex.h>

namespace HX {
bool Checkbox(const HXString &Title, CheckboxProfile &Profile) {
	if (RoughVisibility()) {
		return false;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto position = CurrentLayoutPosition();

	auto checkbox = HXRect{position.X, position.Y, position.X + Profile.BoxSize, position.Y + Profile.BoxSize};

	auto painter = CurrentPainter();

	auto textRectangle = painter->MeasureText(Title, HXFont{}, Profile.FontSize);
	auto textPosition  = HXPoint{position.X + Profile.BoxSize + 8, position.Y + (Profile.BoxSize - textRectangle.Bottom) / 2};

	auto availableRectangle = HXRect{position.X, position.Y, position.X + Profile.BoxSize + 8 + textRectangle.Right,
									 position.Y + Profile.BoxSize};

	if (AccVisibility({availableRectangle.CalWidth(), availableRectangle.CalHeight()})) {
		return false;
	}

	if (!Profile.Disabled) {
		RegisterFocus(&Profile);
		Profile.OnFocus = IsFocused(&Profile);

		// Process keyboard activation when focused
		if (Profile.OnFocus) {
			for (auto &message : context.MessageQuery) {
				if (message.Processed) {
					continue;
				}
				if (message.KeyPressed && message.KeyCode == VK_SPACE) {
					message.Processed = true;
					Profile.Checked = !Profile.Checked;
				}
			}
		}

		// Process the presse or hover
		for (auto &message : context.MessageQuery) {
			if (message.Processed) {
				continue;
			}

			auto mouse = ClipCoord({message.MouseX, message.MouseY});

			if (message.MouseLeftRelease && availableRectangle.PointInside(mouse)) {
				Profile.Checked = !Profile.Checked;
				context.FocusedProfile = &Profile;
			}
		}
	} else {
		Profile.OnFocus = false;
	}

	painter->Begin();

	if (Profile.Checked) {
		// Checked: blue fill, white checkmark
		painter->DrawFilledRoundedRectangle(checkbox, theme.AccentPrimary, theme.AccentPrimary, theme.RadiusSm);
		// Draw white checkmark (polyline)
		HXLineStyle checkStyle;
		checkStyle.Thickness = 2;
		int cx = position.X + Profile.BoxSize / 2;
		int cy = position.Y + Profile.BoxSize / 2;
		// Checkmark points relative to center
		painter->DrawLineStyled({cx - 4, cy}, {cx - 1, cy + 3}, {255,255,255,255}, checkStyle);
		painter->DrawLineStyled({cx - 1, cy + 3}, {cx + 4, cy - 4}, {255,255,255,255}, checkStyle);
	} else {
		// Unchecked: dark fill with border
		HXColor boxBorder = Profile.Disabled ? theme.DisabledText : theme.BorderSubtle;
		painter->DrawFilledRoundedRectangle(checkbox, theme.CheckboxBackground, boxBorder, theme.RadiusSm);
	}

	auto textColor = Profile.Disabled ? theme.DisabledText : theme.CheckboxText;
	painter->DrawText(Title, HXFont{}, textPosition, textColor, Profile.FontSize);

	if (Profile.OnFocus && !Profile.Disabled) {
		painter->DrawFilledRoundedRectangle({checkbox.Left - 2, checkbox.Top - 2,
								checkbox.Right + 2, checkbox.Bottom + 2},
								   theme.SelectedBorder, {0,0,0,0}, theme.RadiusSm + 2);
	}

	painter->End();

	LayoutNextControl({ availableRectangle.CalWidth(), availableRectangle.CalHeight() });

	return Profile.Checked;
}
}
