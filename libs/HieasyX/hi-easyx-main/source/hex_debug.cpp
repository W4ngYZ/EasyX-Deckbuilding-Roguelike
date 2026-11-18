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
 * \file hex_debug.cpp
 * \brief UI Debugger implementation
 */

#include <include/hex_debug.h>
#include <include/hex.h>
#include <include/hex_pool.h>

#include <ctime>
#include <sstream>

#undef max
#undef min

namespace HX {

static clock_t G_LastMetricsClock = 0;
static float   G_LastFrameTimeMs  = 0.0f;

static HXString FormatFloat(float v, int decimals = 2) {
	std::wostringstream oss;
	oss.precision(decimals);
	oss << std::fixed << v;
	return oss.str();
}

static HXString FormatInt(int v) {
	return std::to_wstring(v);
}

void ShowMetricsWindow() {
	auto &context = GetContext();
	auto &theme   = GetTheme();
	auto  painter = CurrentPainter();

	clock_t now = clock();
	if (G_LastMetricsClock != 0) {
		G_LastFrameTimeMs = static_cast<float>(now - G_LastMetricsClock) * 1000.0f / CLOCKS_PER_SEC;
	}
	G_LastMetricsClock = now;

	// Count messages
	int processedMsgs = 0;
	for (const auto &msg : context.MessageQuery) {
		if (msg.Processed) ++processedMsgs;
	}
	int totalMsgs = static_cast<int>(context.MessageQuery.size());

	auto position = CurrentLayoutPosition();

	HXString lines[] = {
		HXStr("Frame: ") + FormatFloat(G_LastFrameTimeMs) + HXStr(" ms ("),
		HXStr("FPS: ") + FormatFloat(G_LastFrameTimeMs > 0 ? 1000.0f / G_LastFrameTimeMs : 0),
		HXStr("Messages: ") + FormatInt(processedMsgs) + HXStr(" / ") + FormatInt(totalMsgs),
		HXStr("Windows: ") + FormatInt(static_cast<int>(context.Windows.size())),
		HXStr("LayoutStack: ") + FormatInt(static_cast<int>(context.LayoutStack.size())),
		HXStr("FocusStack: ") + FormatInt(static_cast<int>(context.FocusStack.Stack.size())),
		HXStr("SubPainter pool: alloc=") + FormatInt(static_cast<int>(GetSubPainterPool().GetTotalAllocated())) +
			HXStr(" reuse=") + FormatInt(static_cast<int>(GetSubPainterPool().GetTotalReused())),
	};

	constexpr int lineHeight = 18;
	constexpr int pad        = 8;
	int           maxW       = 280;
	for (const auto &line : lines) {
		auto r = painter->MeasureText(line, HXFont{}, lineHeight);
		maxW   = std::max(maxW, r.CalWidth() + pad * 2);
	}
	int h = static_cast<int>(sizeof(lines) / sizeof(lines[0])) * lineHeight + pad * 2;

	HXRect bg{position.X, position.Y, position.X + maxW, position.Y + h};
	painter->Begin();
	painter->DrawFilledRoundedRectangle(bg, theme.TextInputBorder, {20, 20, 20, 220}, 6);
	painter->DrawRectangle(bg, theme.TextInputBorder);

	int y = position.Y + pad;
	for (const auto &line : lines) {
		painter->DrawText(line, HXFont{}, {position.X + pad, y}, theme.WindowTitleText, lineHeight);
		y += lineHeight;
	}
	painter->End();

	LayoutNextControl({maxW, h});
}

void ShowLayoutDebugger() {
	auto &context = GetContext();
	auto  painter = CurrentPainter();

	for (size_t i = 0; i < context.LayoutStack.size(); ++i) {
		auto *layout = context.LayoutStack[i];
		if (!layout) continue;

		HXRect region = layout->AvailableRegion;
		// Draw semi-transparent fill
		painter->Begin();
		painter->DrawFilledRectangle(region, {255, 0, 0, 20}, {255, 0, 0, 20});
		painter->DrawRectangle(region, {255, 0, 0, 80});

		// Label
		HXString label = HXStr("L") + std::to_wstring(i) + HXStr(" B=") + std::to_wstring(layout->BaseLine) +
			HXStr(" H=") + std::to_wstring(layout->CurrentHeightMax);
		painter->DrawText(label, HXFont{}, {region.Left + 2, region.Top + 2}, {255, 255, 0, 200}, 12);
		painter->End();
	}
}

}
