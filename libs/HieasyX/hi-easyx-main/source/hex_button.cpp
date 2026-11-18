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
 * \file hex_button.cpp
 * \brief The button for HiEasyX
 */

#include <include/hex.h>
#include <include/hex_button.h>

namespace HX {
HXRect CalButtonRect(ButtonProfile &Profile, HXPoint &Position, const HXRect &fontRect) {
	auto buttonRectangle = HXRect{Position.X, Position.Y,
								  Position.X + fontRect.Right + Profile.ContentGap,
								  Position.Y + Profile.ContentGap + fontRect.Bottom};
	if (Profile.Size.X > 0) {
		buttonRectangle.Right = buttonRectangle.Left + Profile.Size.X;
	}
	if (Profile.Size.Y > 0) {
		buttonRectangle.Bottom = buttonRectangle.Top + Profile.Size.Y;
	}

	return buttonRectangle;
}
bool Button(const HXString &Title, ButtonProfile &Profile) {
	if (RoughVisibility()) {
		return false;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto position = CurrentLayoutPosition();

	auto painter = CurrentPainter();

	const auto fontRect = painter->MeasureText(Title, HXFont{}, theme.FontSizeLabel);

	auto buttonRectangle = CalButtonRect(Profile, position, fontRect);

	const auto width  = buttonRectangle.CalWidth();
	const auto height = buttonRectangle.CalHeight();

	if (AccVisibility({width, height})) {
		return false;
	}
	if (!CurrentVisibility({ width, height })) {
		if (NeedLayoutCal()) {
			LayoutNextControl({ width, height });
		}

		return false;
	}

	RegisterFocus(&Profile);
	Profile.OnFocus = IsFocused(&Profile);

	bool pressed = false;

	// Repeat-on-hold
	if (Profile.RepeatOnHold && Profile.OnHold) {
		if (clock() >= Profile.NextRepeatClock) {
			pressed = true;
			Profile.NextRepeatClock += (Profile.RepeatIntervalMs * CLOCKS_PER_SEC / 1000);
		}
	}

	// Keyboard activation when focused
	if (Profile.OnFocus) {
		for (auto &Message : context.MessageQuery) {
			if (Message.Processed) {
				continue;
			}
			if (Message.KeyPressed && (Message.KeyCode == VK_SPACE || Message.KeyCode == VK_RETURN)) {
				Message.Processed = true;
				pressed = true;
			}
		}
	}

	// Process the presse or hover
	for (auto &Message : context.MessageQuery) {
		auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
		if (Message.Processed) {
			continue;
		}
		if (Message.MouseAction) {
			if (buttonRectangle.PointInside(mouse)) {
				Profile.OnHover = true;
				if (Message.MouseLeftPressed) {
					Message.Processed = true;
					context.FocusedProfile = &Profile;

					Profile.OnHold = true;
					if (Profile.RepeatOnHold) {
						Profile.HoldStartClock  = clock();
						Profile.NextRepeatClock = Profile.HoldStartClock + (Profile.RepeatInitialDelayMs * CLOCKS_PER_SEC / 1000);
					}
				}
				if (Message.MouseLeftRelease) {
					Profile.OnHold = false;

					pressed = true;
				}
			} else if (Profile.OnHover) {
				Profile.OnHover = false;
				Profile.OnHold  = false;
			}
		}
	}

	painter->Begin();

	HXColor bgColor = Profile.OnHold ? theme.ButtonPressedBackground :
					  (Profile.OnHover ? theme.ButtonOnHoverBackground : theme.ButtonBackground);
	painter->DrawFilledRoundedRectangle(buttonRectangle, bgColor, bgColor, theme.RadiusMd);

	HXColor textColor = Profile.OnHold ? theme.ButtonPressedText :
						(Profile.OnHover ? theme.ButtonOnHoverText : theme.ButtonText);
	painter->DrawText(Title, HXFont{},
					  {position.X + width / 2 - fontRect.Right / 2,
					   position.Y + height / 2 - fontRect.Bottom / 2},
					  textColor, theme.FontSizeLabel);

	if (Profile.OnFocus) {
		painter->DrawFilledRoundedRectangle({buttonRectangle.Left - 2, buttonRectangle.Top - 2,
					buttonRectangle.Right + 2, buttonRectangle.Bottom + 2},
					   theme.SelectedBorder, {0,0,0,0}, theme.RadiusMd + 2);
	}

	painter->End();

	Profile.OnPressed = pressed;

	LayoutNextControl({width, height});

	return pressed;
}
}
