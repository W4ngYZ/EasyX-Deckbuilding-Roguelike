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
 * \file hex_tooltip.cpp
 * \brief Tooltip implementation
 */

#include <include/hex.h>
#include <include/hex_tooltip.h>

#include <ctime>

namespace HX {

struct HXTooltipState {
	HXString Text;
	HXPoint  Pos{0, 0};
	clock_t  LastSetClock = 0;
	bool     Active       = false;
	bool     WasUpdatedThisFrame = false;
};

static HXTooltipState GTooltipState;
static TooltipProfile   GTooltipProfile;

TooltipProfile &GetTooltipProfile() {
	return GTooltipProfile;
}

void Tooltip(const HXString &Text) {
	auto &context = GetContext();

	HXPoint mousePos{0, 0};
	bool    hasMouse = false;
	for (auto it = context.MessageQuery.rbegin(); it != context.MessageQuery.rend(); ++it) {
		if (it->MouseAction) {
			mousePos  = {it->MouseX, it->MouseY};
			hasMouse  = true;
			break;
		}
	}

	if (!hasMouse) {
		GTooltipState.Active = false;
		return;
	}

	// Only reset the timer when the tooltip newly becomes active or text changes
	if (!GTooltipState.Active || GTooltipState.Text != Text) {
		GTooltipState.LastSetClock = clock();
	}

	GTooltipState.Text   = Text;
	GTooltipState.Active = true;
	GTooltipState.Pos    = mousePos;
	GTooltipState.WasUpdatedThisFrame = true;
}

void RenderTooltip() {
	if (!GTooltipState.WasUpdatedThisFrame) {
		GTooltipState.Active = false;
	}
	if (!GTooltipState.Active || GTooltipState.Text.empty()) {
		return;
	}

	auto &profile = GetTooltipProfile();

	// Delay: show after configured hover delay
	if (clock() - GTooltipState.LastSetClock < profile.ShowDelayMs) {
		return;
	}

	auto &theme = GetTheme();
	auto &context = GetContext();
	if (!context.RenderContext) {
		return;
	}

	auto *painter = context.RenderContext->DefaultPainter();
	if (!painter) {
		return;
	}

	auto textRect = painter->MeasureText(GTooltipState.Text, HXFont{}, theme.FontSizeCaption);
	int padH = 10;
	int padV = 6;
	auto tooltipRect = HXRect{
		GTooltipState.Pos.X + profile.OffsetXY,
		GTooltipState.Pos.Y + profile.OffsetXY,
		GTooltipState.Pos.X + profile.OffsetXY + textRect.Right + padH * 2,
		GTooltipState.Pos.Y + profile.OffsetXY + textRect.Bottom + padV * 2
	};

	HXColor bgColor   = profile.BackgroundColor.A > 0 ? profile.BackgroundColor : theme.TooltipBackground;
	HXColor borderColor = profile.BorderColor.A > 0 ? profile.BorderColor : theme.TooltipBorder;
	HXColor textColor = profile.TextColor.A > 0 ? profile.TextColor : theme.TooltipText;

	// Shadow
	HXRect shadowRect = tooltipRect;
	shadowRect.Left += 2;
	shadowRect.Top += 2;
	shadowRect.Right += 2;
	shadowRect.Bottom += 2;
	painter->DrawFilledRoundedRectangle(shadowRect, theme.ShadowColor, theme.ShadowColor, theme.RadiusMd);

	painter->DrawFilledRoundedRectangle(tooltipRect, borderColor, bgColor, theme.RadiusMd);
	painter->DrawText(GTooltipState.Text, HXFont{},
	                  {tooltipRect.Left + padH, tooltipRect.Top + padV},
	                  textColor, theme.FontSizeCaption);
}

void ResetTooltipFrame() {
	GTooltipState.WasUpdatedThisFrame = false;
}

}
