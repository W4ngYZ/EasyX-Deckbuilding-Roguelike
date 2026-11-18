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
 * \file hex_progress.cpp
 * \brief Progress bar implementation
 */

#include <include/hex.h>
#include <include/hex_progress.h>

#include <algorithm>

#undef max
#undef min

namespace HX {

static ProgressBarProfile GDefaultProgressBarProfile;

static void ProgressBarImpl(float Fraction, HXPoint Size, ProgressBarProfile &Profile) {
	if (RoughVisibility()) {
		return;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();
	auto  layout  = CurrentLayout();

	auto position = CurrentLayoutPosition();

	if (Size.X < 0) {
		Size.X = layout->AvailableRegion.Right - layout->AvailableRegion.Left - 10;
	}
	if (Size.Y < 0) {
		Size.Y = Profile.Height;
	}

	if (AccVisibility(Size)) {
		return;
	}

	Fraction = std::max(0.0f, std::min(1.0f, Fraction));

	auto barRect = HXRect{position.X, position.Y, position.X + Size.X, position.Y + Size.Y};
	auto fillWidth = static_cast<HXGInt>(Size.X * Fraction);
	auto fillRect  = HXRect{position.X, position.Y, position.X + fillWidth, position.Y + Size.Y};

	HXColor trackFill = Profile.TrackColor.A > 0 ? Profile.TrackColor : theme.ProgressTrack;
	HXColor fillColor = Profile.FillColor.A > 0 ? Profile.FillColor : theme.ProgressFill;

	painter->Begin();
	painter->DrawFilledRoundedRectangle(barRect, {0,0,0,0}, trackFill, theme.RadiusMd);
	if (fillWidth > 0) {
		painter->DrawFilledRoundedRectangle(fillRect, {0,0,0,0}, fillColor, theme.RadiusMd);
	}
	painter->End();

	LayoutNextControl(Size);
}

static void ProgressBarLabeledImpl(const HXString &Title, float Fraction, HXPoint Size, ProgressBarProfile &Profile) {
	if (RoughVisibility()) {
		return;
	}

	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();
	auto  layout  = CurrentLayout();

	auto position = CurrentLayoutPosition();
	auto textRect = painter->MeasureText(Title, HXFont{}, Profile.FontSize);

	constexpr HXGInt textGap = 8;
	HXGInt           barWidth = Size.X;
	if (barWidth < 0) {
		barWidth = layout->AvailableRegion.Right - layout->AvailableRegion.Left - textRect.Right - textGap - 10;
	}
	if (Size.Y < 0) {
		Size.Y = Profile.Height;
	}

	HXPoint totalSize{textRect.Right + textGap + barWidth, std::max(textRect.Bottom, Size.Y)};
	if (AccVisibility(totalSize)) {
		return;
	}

	HXGInt barY = position.Y + totalSize.Y / 2 - Size.Y / 2;
	HXGInt textY = position.Y + totalSize.Y / 2 - textRect.Bottom / 2;

	painter->Begin();
	painter->DrawText(Title, HXFont{}, {position.X, textY},
	                  theme.WindowTitleText, Profile.FontSize);
	painter->End();

	HXPoint barPos{position.X + textRect.Right + textGap, barY};

	Fraction = std::max(0.0f, std::min(1.0f, Fraction));
	auto barRect   = HXRect{barPos.X, barPos.Y, barPos.X + barWidth, barPos.Y + Size.Y};
	auto fillWidth = static_cast<HXGInt>(barWidth * Fraction);
	auto fillRect  = HXRect{barPos.X, barPos.Y, barPos.X + fillWidth, barPos.Y + Size.Y};

	HXColor trackFill = Profile.TrackColor.A > 0 ? Profile.TrackColor : theme.ProgressTrack;
	HXColor fillColor = Profile.FillColor.A > 0 ? Profile.FillColor : theme.ProgressFill;

	painter->Begin();
	painter->DrawFilledRoundedRectangle(barRect, {0,0,0,0}, trackFill, theme.RadiusMd);
	if (fillWidth > 0) {
		painter->DrawFilledRoundedRectangle(fillRect, {0,0,0,0}, fillColor, theme.RadiusMd);
	}
	painter->End();

	LayoutNextControl(totalSize);
}

void ProgressBar(float Fraction, HXPoint Size) {
	ProgressBarImpl(Fraction, Size, GDefaultProgressBarProfile);
}

void ProgressBar(float Fraction, HXPoint Size, ProgressBarProfile &Profile) {
	ProgressBarImpl(Fraction, Size, Profile);
}

void ProgressBar(const HXString &Title, float Fraction, HXPoint Size) {
	ProgressBarLabeledImpl(Title, Fraction, Size, GDefaultProgressBarProfile);
}

void ProgressBar(const HXString &Title, float Fraction, HXPoint Size, ProgressBarProfile &Profile) {
	ProgressBarLabeledImpl(Title, Fraction, Size, Profile);
}

}
