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
 * \file hex_colorpicker.cpp
 * \brief Color picker control implementation
 */


#include <include/hex_colorpicker.h>
#include <include/hex.h>

#include <algorithm>
#include <cmath>

#undef max
#undef min

namespace HX {

static HXColor HSVToRGB(float h, float s, float v) {
	float c = v * s;
	float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
	float m = v - c;
	float r = 0, g = 0, b = 0;
	if (h < 60)       { r = c; g = x; b = 0; }
	else if (h < 120) { r = x; g = c; b = 0; }
	else if (h < 180) { r = 0; g = c; b = x; }
	else if (h < 240) { r = 0; g = x; b = c; }
	else if (h < 300) { r = x; g = 0; b = c; }
	else              { r = c; g = 0; b = x; }
	return {
		static_cast<unsigned char>((r + m) * 255),
		static_cast<unsigned char>((g + m) * 255),
		static_cast<unsigned char>((b + m) * 255),
		255
	};
}

static void RGBToHSV(const HXColor &col, float &h, float &s, float &v) {
	float r = col.R / 255.0f, g = col.G / 255.0f, b = col.B / 255.0f;
	float mx = std::max(r, std::max(g, b));
	float mn = std::min(r, std::min(g, b));
	float df = mx - mn;
	v = mx;
	if (mx < 0.001f) s = 0; else s = df / mx;
	if (df < 0.001f) { h = 0; return; }
	if (mx == r) h = 60.0f * fmod((g - b) / df, 6.0f);
	else if (mx == g) h = 60.0f * ((b - r) / df + 2.0f);
	else h = 60.0f * ((r - g) / df + 4.0f);
	if (h < 0) h += 360.0f;
}

bool ColorPicker(const HXString &id, ColorPickerProfile &profile) {
	(void)id;
	auto &context = GetContext();
	auto &theme   = GetTheme();

	if (RoughVisibility()) {
		LayoutNextControl(profile.Size);
		return false;
	}
	if (AccVisibility(profile.Size)) {
		LayoutNextControl(profile.Size);
		return false;
	}

	auto position = CurrentLayoutPosition();
	auto painter  = CurrentPainter();

	// Sync HSV from RGB
	float h, s, v;
	RGBToHSV(profile.Color, h, s, v);

	constexpr HXGInt sqSize = 160;
	constexpr HXGInt hueH   = 16;
	constexpr HXGInt margin = 8;
	constexpr HXGInt grid   = 32; // 32x32 cells for Sat/Val square

	// 1) Sat/Val square — draw with small rectangles (grid x grid)
	HXRect sqRect{position.X + margin, position.Y + margin, position.X + margin + sqSize, position.Y + margin + sqSize};
	HXGInt cellSize = sqSize / grid;
	painter->Begin();
	for (int gy = 0; gy < grid; ++gy) {
		for (int gx = 0; gx < grid; ++gx) {
			float cs = (gx + 0.5f) / grid;
			float cv = 1.0f - (gy + 0.5f) / grid;
			HXColor c = HSVToRGB(h, cs, cv);
			HXRect cr{sqRect.Left + gx * cellSize, sqRect.Top + gy * cellSize,
			          sqRect.Left + (gx + 1) * cellSize, sqRect.Top + (gy + 1) * cellSize};
			painter->DrawFilledRectangle(cr, c, c);
		}
	}
	// Selection cursor on Sat/Val square
	HXGInt selX = sqRect.Left + static_cast<HXGInt>(s * sqSize);
	HXGInt selY = sqRect.Top + static_cast<HXGInt>((1.0f - v) * sqSize);
	painter->DrawRectangle({selX - 4, selY - 4, selX + 4, selY + 4}, profile.CursorColor);
	painter->End();

	// 2) Hue bar
	HXRect hueRect{position.X + margin, position.Y + margin + sqSize + 6, position.X + margin + sqSize, position.Y + margin + sqSize + 6 + hueH};
	painter->Begin();
	int segW = sqSize / 6;
	for (int seg = 0; seg < 6; ++seg) {
		float hh = seg * 60.0f;
		HXColor c1 = HSVToRGB(hh, 1.0f, 1.0f);
		HXColor c2 = HSVToRGB(hh + 60.0f, 1.0f, 1.0f);
		// Gradient approximation: split each segment into 2 colors
		HXRect cr1{hueRect.Left + seg * segW, hueRect.Top, hueRect.Left + seg * segW + segW / 2, hueRect.Bottom};
		HXRect cr2{hueRect.Left + seg * segW + segW / 2, hueRect.Top, hueRect.Left + (seg + 1) * segW, hueRect.Bottom};
		painter->DrawFilledRectangle(cr1, c1, c1);
		painter->DrawFilledRectangle(cr2, c2, c2);
	}
	// Hue cursor
	HXGInt hueX = hueRect.Left + static_cast<HXGInt>((h / 360.0f) * sqSize);
	painter->DrawLine({hueX, hueRect.Top - 2}, {hueX, hueRect.Bottom + 2}, profile.CursorColor);
	painter->End();

	// 3) Preview + RGB text
	HXGInt textY = hueRect.Bottom + 8;
	HXRect previewRect{position.X + margin, textY, position.X + margin + 40, textY + 24};
	painter->Begin();
	painter->DrawFilledRectangle(previewRect, profile.Color, profile.Color);
	painter->DrawRectangle(previewRect, profile.PreviewBorderColor);
	HXString rgbText = std::to_wstring(profile.Color.R) + HXStr(",") + std::to_wstring(profile.Color.G) + HXStr(",") + std::to_wstring(profile.Color.B);
	painter->DrawText(rgbText, HXFont{}, {previewRect.Right + 8, textY + 4}, theme.ButtonText, 12);
	painter->End();

	// ---- Input handling ----
	HXRect controlRect{position.X, position.Y, position.X + profile.Size.X, position.Y + profile.Size.Y};
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed) continue;
		if (!msg.MouseAction) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (!controlRect.PointInside(mouse)) continue;

		// Sat/Val square drag
		if (sqRect.PointInside(mouse)) {
			if (msg.MouseLeftPressed || msg.MouseLeftRelease || (msg.MouseAction && !msg.MouseLeftRelease)) {
				s = static_cast<float>(mouse.X - sqRect.Left) / sqSize;
				v = 1.0f - static_cast<float>(mouse.Y - sqRect.Top) / sqSize;
				s = std::clamp(s, 0.0f, 1.0f);
				v = std::clamp(v, 0.0f, 1.0f);
				profile.Color = HSVToRGB(h, s, v);
				msg.Processed = true;
			}
		}
		// Hue bar drag
		if (hueRect.PointInside(mouse)) {
			if (msg.MouseLeftPressed || msg.MouseLeftRelease || (msg.MouseAction && !msg.MouseLeftRelease)) {
				h = static_cast<float>(mouse.X - hueRect.Left) / sqSize * 360.0f;
				h = std::clamp(h, 0.0f, 360.0f);
				profile.Color = HSVToRGB(h, s, v);
				msg.Processed = true;
			}
		}
	}

	LayoutNextControl(profile.Size);
	return true;
}

} // namespace HX
