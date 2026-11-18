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
 * \file hex_drawlist.cpp
 * \brief DrawList implementation
 */

#include <include/hex_drawlist.h>
#include <include/impl/hex_impl.h>

#include <algorithm>

namespace HX {

void HXDrawList::AddRect(const HXRect &r, HXColor col, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type     = DrawCmdType::Rect;
	cmd.ClipRect = clip;
	cmd.Color    = col;
	cmd.Rect     = r;
	Commands.push_back(cmd);
}

void HXDrawList::AddFilledRect(const HXRect &r, HXColor fill, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type      = DrawCmdType::FilledRect;
	cmd.ClipRect  = clip;
	cmd.FillColor = fill;
	cmd.Rect      = r;
	Commands.push_back(cmd);
}

void HXDrawList::AddFilledRoundRect(const HXRect &r, HXColor border, HXColor fill, HXGInt radius, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type      = DrawCmdType::FilledRoundRect;
	cmd.ClipRect  = clip;
	cmd.Color     = border;
	cmd.FillColor = fill;
	cmd.Rect      = r;
	cmd.Radius    = radius;
	Commands.push_back(cmd);
}

void HXDrawList::AddText(const HXString &text, HXFont font, HXPoint pos, HXColor col, HXGUInt height, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type      = DrawCmdType::Text;
	cmd.ClipRect  = clip;
	cmd.Color     = col;
	cmd.Text      = text;
	cmd.Font      = font;
	cmd.TextPos   = pos;
	cmd.TextHeight = height;
	Commands.push_back(cmd);
}

void HXDrawList::AddLine(HXPoint p1, HXPoint p2, HXColor col, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type     = DrawCmdType::Line;
	cmd.ClipRect = clip;
	cmd.Color    = col;
	cmd.LineP1   = p1;
	cmd.LineP2   = p2;
	Commands.push_back(cmd);
}

void HXDrawList::AddLineStyled(HXPoint p1, HXPoint p2, HXColor col, HXLineStyle style, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type      = DrawCmdType::LineStyled;
	cmd.ClipRect  = clip;
	cmd.Color     = col;
	cmd.LineP1    = p1;
	cmd.LineP2    = p2;
	cmd.LineStyle = style;
	Commands.push_back(cmd);
}

void HXDrawList::AddFilledPolygon(const std::vector<HXPoint> &points, HXColor col, HXColor fill, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type          = DrawCmdType::FilledPolygon;
	cmd.ClipRect      = clip;
	cmd.Color         = col;
	cmd.FillColor     = fill;
	cmd.PolygonPoints = points;
	Commands.push_back(cmd);
}

void HXDrawList::AddImage(void *buffer, HXPoint pos, HXPoint size, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type       = DrawCmdType::Image;
	cmd.ClipRect   = clip;
	cmd.ImageBuffer = buffer;
	cmd.ImagePos   = pos;
	cmd.ImageSize  = size;
	Commands.push_back(cmd);
}

void HXDrawList::AddPainter(HXBufferPainter *painter, HXPoint pos, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type         = DrawCmdType::Painter;
	cmd.ClipRect     = clip;
	cmd.SourcePainter = painter;
	cmd.BlitPos      = pos;
	Commands.push_back(cmd);
}

void HXDrawList::AddShadowRect(const HXRect &r, HXColor shadowColor, HXGInt radius, HXGInt blur, HXGInt offsetX, HXGInt offsetY, const HXRect &clip) {
	HXDrawCmd cmd;
	cmd.Type          = DrawCmdType::ShadowRect;
	cmd.ClipRect      = clip;
	cmd.Color         = shadowColor;
	cmd.Rect          = r;
	cmd.Radius        = radius;
	cmd.ShadowBlur    = blur;
	cmd.ShadowOffsetX = offsetX;
	cmd.ShadowOffsetY = offsetY;
	Commands.push_back(cmd);
}

////////////////////////////////////////////////////////////
/// Flush

void FlushDrawList(HXBufferPainter *painter, HXDrawList &list) {
	if (!painter || list.Commands.empty()) return;

	// Sort by clip rect to batch same-scissor commands together
	auto &cmds = list.Commands;
	std::sort(cmds.begin(), cmds.end(), [](const HXDrawCmd &a, const HXDrawCmd &b) {
		if (a.ClipRect.Left != b.ClipRect.Left) return a.ClipRect.Left < b.ClipRect.Left;
		if (a.ClipRect.Top != b.ClipRect.Top) return a.ClipRect.Top < b.ClipRect.Top;
		if (a.ClipRect.Right != b.ClipRect.Right) return a.ClipRect.Right < b.ClipRect.Right;
		return a.ClipRect.Bottom < b.ClipRect.Bottom;
	});

	painter->Begin();
	HXRect currentClip{-1, -1, -1, -1};

	for (const auto &cmd : cmds) {
		// Apply clip rect if changed
		if (cmd.ClipRect.Left != currentClip.Left || cmd.ClipRect.Top != currentClip.Top ||
		    cmd.ClipRect.Right != currentClip.Right || cmd.ClipRect.Bottom != currentClip.Bottom) {
			currentClip = cmd.ClipRect;
			// Note: HXBufferPainter doesn't have a SetClipRect API yet.
			// For now we rely on the sub-painter size to naturally clip.
		}

		switch (cmd.Type) {
		case DrawCmdType::Rect:
			painter->DrawRectangle(cmd.Rect, cmd.Color);
			break;
		case DrawCmdType::FilledRect:
			painter->DrawFilledRectangle(cmd.Rect, cmd.FillColor, cmd.FillColor);
			break;
		case DrawCmdType::FilledRoundRect:
			painter->DrawFilledRoundedRectangle(cmd.Rect, cmd.Color, cmd.FillColor, cmd.Radius);
			break;
		case DrawCmdType::Text:
			painter->DrawText(cmd.Text, cmd.Font, cmd.TextPos, cmd.Color, cmd.TextHeight);
			break;
		case DrawCmdType::Line:
			painter->DrawLine(cmd.LineP1, cmd.LineP2, cmd.Color);
			break;
		case DrawCmdType::LineStyled:
			painter->DrawLineStyled(cmd.LineP1, cmd.LineP2, cmd.Color, cmd.LineStyle);
			break;
		case DrawCmdType::FilledPolygon:
			painter->DrawFilledPolygon(cmd.PolygonPoints, cmd.Color, cmd.FillColor);
			break;
		case DrawCmdType::Image:
			painter->DrawImage(cmd.ImageBuffer, cmd.ImagePos, cmd.ImageSize);
			break;
		case DrawCmdType::Painter:
			if (cmd.SourcePainter) {
				painter->DrawPainter(cmd.SourcePainter, cmd.BlitPos);
			}
			break;
		case DrawCmdType::ShadowRect:
			painter->DrawShadowRect(cmd.Rect, cmd.Color, cmd.Radius, cmd.ShadowBlur, cmd.ShadowOffsetX, cmd.ShadowOffsetY);
			break;
		}
	}

	painter->End();
}

}
