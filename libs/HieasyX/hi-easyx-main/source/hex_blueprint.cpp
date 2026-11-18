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
  * \file hex_blueprint.cpp
  * \brief Blueprint canvas rendering and interaction
  */

#include <include/hex.h>
#include <include/hex_blueprint.h>
#include <include/hex_inline_editor.h>
#include <include/hex_pool.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <unordered_set>

#include <graphics.h>

#undef max
#undef min

namespace HX {

	////////////////////////////////////////////////////////////
	/// Texture image cache (simple in-memory cache for preview thumbnails)

	static std::map<HXString, IMAGE*> G_TextureImageCache;

	static IMAGE* GetCachedTextureImage(const HXString& path) {
		if (path.empty())
			return nullptr;
		auto it = G_TextureImageCache.find(path);
		if (it != G_TextureImageCache.end())
			return it->second;
		IMAGE* img = new IMAGE();
		loadimage(img, path.c_str());
		if (img->getwidth() == 0 || img->getheight() == 0) {
			delete img;
			return nullptr;
		}
		G_TextureImageCache[path] = img;
		return img;
	}

	////////////////////////////////////////////////////////////
	/// HSV / RGB helpers

	static HXColor HSVToRGB(float h, float s, float v) {
		float c = v * s;
		float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
		float m = v - c;
		float r = 0, g = 0, b = 0;
		if (h < 60) {
			r = c;
			g = x;
			b = 0;
		}
		else if (h < 120) {
			r = x;
			g = c;
			b = 0;
		}
		else if (h < 180) {
			r = 0;
			g = c;
			b = x;
		}
		else if (h < 240) {
			r = 0;
			g = x;
			b = c;
		}
		else if (h < 300) {
			r = x;
			g = 0;
			b = c;
		}
		else {
			r = c;
			g = 0;
			b = x;
		}
		return { static_cast<unsigned char>((r + m) * 255), static_cast<unsigned char>((g + m) * 255),
				static_cast<unsigned char>((b + m) * 255), 255 };
	}

	static void RGBToHSV(const HXColor& col, float& h, float& s, float& v) {
		float r = col.R / 255.0f, g = col.G / 255.0f, b = col.B / 255.0f;
		float mx = std::max(r, std::max(g, b));
		float mn = std::min(r, std::min(g, b));
		float df = mx - mn;
		v = mx;
		if (mx < 0.001f)
			s = 0;
		else
			s = df / mx;
		if (df < 0.001f) {
			h = 0;
			return;
		}
		if (mx == r)
			h = 60.0f * fmod((g - b) / df, 6.0f);
		else if (mx == g)
			h = 60.0f * ((b - r) / df + 2.0f);
		else
			h = 60.0f * ((r - g) / df + 4.0f);
		if (h < 0)
			h += 360.0f;
	}

	static HXColor ParseColorString(const HXString& str) {
		HXColor c{ 255, 255, 255, 255 };
		float	r = 1.0f, g = 1.0f, b = 1.0f;
		// Manual parse to avoid swscanf deprecation warning
		try {
			size_t p1 = str.find(L',');
			size_t p2 = str.find(L',', p1 + 1);
			if (p1 != HXString::npos)
				r = std::stof(str.substr(0, p1));
			if (p2 != HXString::npos) {
				g = std::stof(str.substr(p1 + 1, p2 - p1 - 1));
				b = std::stof(str.substr(p2 + 1));
			}
			else if (p1 != HXString::npos) {
				g = std::stof(str.substr(p1 + 1));
			}
		}
		catch (...) {
		}
		c.R = static_cast<unsigned char>(std::clamp(r * 255.0f, 0.0f, 255.0f));
		c.G = static_cast<unsigned char>(std::clamp(g * 255.0f, 0.0f, 255.0f));
		c.B = static_cast<unsigned char>(std::clamp(b * 255.0f, 0.0f, 255.0f));
		return c;
	}

	static HXString ColorToString(const HXColor& c) {
		float	r = c.R / 255.0f;
		float	g = c.G / 255.0f;
		float	b = c.B / 255.0f;
		wchar_t buf[64];
		swprintf(buf, 64, L"%.2f,%.2f,%.2f", r, g, b);
		return HXString(buf);
	}

	////////////////////////////////////////////////////////////
	/// Coordinate transforms

	HXPoint WorldToScreen(const HXPoint& worldPos, float zoom, const HXPoint& offset) {
		return { static_cast<HXGInt>((worldPos.X - offset.X) * zoom), static_cast<HXGInt>((worldPos.Y - offset.Y) * zoom) };
	}

	HXPoint ScreenToWorld(const HXPoint& screenPos, float zoom, const HXPoint& offset) {
		if (zoom == 0.0f)
			zoom = 0.001f;
		return { static_cast<HXGInt>(screenPos.X / zoom + offset.X), static_cast<HXGInt>(screenPos.Y / zoom + offset.Y) };
	}

	static HXRect WorldToScreenRect(const HXPoint& worldPos, const HXPoint& worldSize, float zoom, const HXPoint& offset) {
		HXPoint screenPos = WorldToScreen(worldPos, zoom, offset);
		HXPoint screenSize = { static_cast<HXGInt>(worldSize.X * zoom), static_cast<HXGInt>(worldSize.Y * zoom) };
		return { screenPos.X, screenPos.Y, screenPos.X + screenSize.X, screenPos.Y + screenSize.Y };
	}

	////////////////////////////////////////////////////////////
	/// Data model helpers

	HXNode* HXNodeGraph::FindNode(const HXString& id) {
		for (auto& n : Nodes)
			if (n.Id == id)
				return &n;
		return nullptr;
	}

	HXNodePin* HXNodeGraph::FindPin(const HXString& id) {
		for (auto& n : Nodes) {
			for (auto& p : n.Inputs)
				if (p.Id == id)
					return &p;
			for (auto& p : n.Outputs)
				if (p.Id == id)
					return &p;
		}
		return nullptr;
	}

	HXNode* HXNodeGraph::FindNodeContainingPin(const HXString& pinId) {
		for (auto& n : Nodes) {
			for (auto& p : n.Inputs)
				if (p.Id == pinId)
					return &n;
			for (auto& p : n.Outputs)
				if (p.Id == pinId)
					return &n;
		}
		return nullptr;
	}

	void HXNodeGraph::DeleteNode(const HXString& id) {
		Links.erase(std::remove_if(Links.begin(), Links.end(),
			[&](const HXNodeLink& link) {
				auto* a = FindNodeContainingPin(link.StartPinId);
				auto* b = FindNodeContainingPin(link.EndPinId);
				return (a && a->Id == id) || (b && b->Id == id);
			}),
			Links.end());
		Nodes.erase(std::remove_if(Nodes.begin(), Nodes.end(), [&](const HXNode& n) { return n.Id == id; }), Nodes.end());
	}

	////////////////////////////////////////////////////////////
	/// Color helpers

	HXColor GetPinColor(HXPinType type, const HXTheme* theme, const BlueprintProfile* profile) {
		if (profile && profile->PinColorCallback) {
			return profile->PinColorCallback(type);
		}
		if (theme) {
			switch (type) {
			case HXPinType::Flow:
				return theme->PinTypeFlow;
			case HXPinType::Bool:
				return theme->PinTypeBool;
			case HXPinType::Int:
				return theme->PinTypeInt;
			case HXPinType::Float:
				return theme->PinTypeFloat;
			case HXPinType::String:
				return theme->PinTypeString;
			case HXPinType::Vector:
				return theme->PinTypeVector;
			case HXPinType::Color:
				return theme->PinTypeColor;
			case HXPinType::Object:
				return theme->PinTypeTexture; // fallback
			case HXPinType::Texture:
				return theme->PinTypeTexture;
			default:
				return theme->TextInputText;
			}
		}
		switch (type) {
		case HXPinType::Flow:
			return { 226, 232, 240, 255 };
		case HXPinType::Bool:
			return { 248, 113, 113, 255 };
		case HXPinType::Int:
			return { 52, 211, 153, 255 };
		case HXPinType::Float:
			return { 96, 165, 250, 255 };
		case HXPinType::String:
			return { 244, 114, 182, 255 };
		case HXPinType::Vector:
			return { 251, 191, 36, 255 };
		case HXPinType::Color:
			return { 45, 212, 191, 255 };
		case HXPinType::Object:
			return { 167, 139, 250, 255 };
		case HXPinType::Texture:
			return { 251, 146, 60, 255 };
		default:
			return { 148, 163, 184, 255 };
		}
	}

	HXColor GetCategoryColor(HXNodeCategory cat, const HXTheme* theme, const BlueprintProfile* profile) {
		if (profile && profile->CategoryColorCallback) {
			return profile->CategoryColorCallback(cat);
		}
		if (theme) {
			switch (cat) {
			case HXNodeCategory::Math:
				return theme->CategoryMathColor;
			case HXNodeCategory::FlowControl:
				return theme->CategoryFlowColor;
			case HXNodeCategory::Function:
				return theme->CategoryFunctionColor;
			case HXNodeCategory::Variable:
				return theme->CategoryVariableColor;
			case HXNodeCategory::Material:
				return theme->CategoryConversionColor; // fallback
			case HXNodeCategory::Output:
				return theme->CategoryUtilityColor; // fallback
			case HXNodeCategory::Comment:
				return theme->CategoryConstantColor; // fallback
			default:
				return theme->CategoryUtilityColor;
			}
		}
		switch (cat) {
		case HXNodeCategory::Event:
			return { 225, 86, 86, 255 };
		case HXNodeCategory::Function:
			return { 38, 150, 190, 255 };
		case HXNodeCategory::Variable:
			return { 42, 166, 125, 255 };
		case HXNodeCategory::Math:
			return { 209, 156, 42, 255 };
		case HXNodeCategory::FlowControl:
			return { 210, 116, 74, 255 };
		case HXNodeCategory::Material:
			return { 115, 136, 230, 255 };
		case HXNodeCategory::Output:
			return { 64, 185, 165, 255 };
		case HXNodeCategory::Comment:
			return { 39, 115, 104, 145 };
		default:
			return { 91, 105, 105, 255 };
		}
	}

	////////////////////////////////////////////////////////////
	/// Drawing helpers

	static void DrawBezierCubic(HXBufferPainter* p, HXPoint start, HXPoint end, HXColor color, float zoom,
		float thickness = 2.0f, bool dashed = false) {
		float		  tangent = 50.0f * std::max(zoom, 0.5f);
		HXPoint		  cp1 = { start.X + static_cast<HXGInt>(tangent), start.Y };
		HXPoint		  cp2 = { end.X - static_cast<HXGInt>(tangent), end.Y };
		constexpr int segments = 24;
		HXPoint		  prev = start;
		HXLineStyle	  style;
		style.Thickness = static_cast<HXColInt>(std::clamp(thickness * zoom, 0.5f, 4.0f));
		style.Shape = dashed ? HXLineShapeStyle::Dash : HXLineShapeStyle::Solid;
		for (int i = 1; i <= segments; ++i) {
			float	t = i / static_cast<float>(segments);
			float	u = 1.0f - t;
			float	tt = t * t, uu = u * u;
			float	uuu = uu * u, ttt = tt * t;
			float	x = uuu * start.X + 3.0f * uu * t * cp1.X + 3.0f * u * tt * cp2.X + ttt * end.X;
			float	y = uuu * start.Y + 3.0f * uu * t * cp1.Y + 3.0f * u * tt * cp2.Y + ttt * end.Y;
			HXPoint curr{ static_cast<HXGInt>(x + 0.5f), static_cast<HXGInt>(y + 0.5f) };
			if (dashed)
				p->DrawLineStyled(prev, curr, color, style);
			else
				p->DrawLine(prev, curr, color);
			prev = curr;
		}
	}

	// Compute minimum distance from point P to a cubic Bezier curve (sampled)
	static float DistancePointToBezier(HXPoint p, HXPoint start, HXPoint end, float zoom) {
		float		  tangent = 50.0f * std::max(zoom, 0.5f);
		HXPoint		  cp1 = { start.X + static_cast<HXGInt>(tangent), start.Y };
		HXPoint		  cp2 = { end.X - static_cast<HXGInt>(tangent), end.Y };
		constexpr int segments = 32;
		float		  minDist = 1e9f;
		for (int i = 0; i <= segments; ++i) {
			float t = i / static_cast<float>(segments);
			float u = 1.0f - t;
			float tt = t * t, uu = u * u;
			float uuu = uu * u, ttt = tt * t;
			float x = uuu * start.X + 3.0f * uu * t * cp1.X + 3.0f * u * tt * cp2.X + ttt * end.X;
			float y = uuu * start.Y + 3.0f * uu * t * cp1.Y + 3.0f * u * tt * cp2.Y + ttt * end.Y;
			float dx = x - p.X, dy = y - p.Y;
			float d = dx * dx + dy * dy;
			if (d < minDist)
				minDist = d;
		}
		return std::sqrt(minDist);
	}

	static void DrawGrid(HXBufferPainter* painter, const HXRect& canvasRect, float zoom, const HXPoint& offset,
		HXColor color) {
		float worldGrid = 50.0f;
		float invZoom = 1.0f / std::max(zoom, 0.001f);
		float worldLeft = offset.X;
		float worldRight = offset.X + canvasRect.CalWidth() * invZoom;
		float worldTop = offset.Y;
		float worldBottom = offset.Y + canvasRect.CalHeight() * invZoom;
		float firstGridX = std::floor(worldLeft / worldGrid) * worldGrid;
		float firstGridY = std::floor(worldTop / worldGrid) * worldGrid;
		for (float gx = firstGridX; gx <= worldRight; gx += worldGrid) {
			HXGInt sx = canvasRect.Left + static_cast<HXGInt>((gx - offset.X) * zoom);
			painter->DrawLine({ sx, canvasRect.Top }, { sx, canvasRect.Bottom }, color);
		}
		for (float gy = firstGridY; gy <= worldBottom; gy += worldGrid) {
			HXGInt sy = canvasRect.Top + static_cast<HXGInt>((gy - offset.Y) * zoom);
			painter->DrawLine({ canvasRect.Left, sy }, { canvasRect.Right, sy }, color);
		}
	}

	////////////////////////////////////////////////////////////
	/// Auto layout & node rendering

	static HXPoint CalculateNodeSize(const HXNode& node, float zoom) {
		if (node.Category == HXNodeCategory::Comment) {
			return node.Size; // Comment size is manually set
		}
		HXGInt titleH = std::max(static_cast<HXGInt>(32 * zoom), 24);
		HXGInt pinSpacing = std::max(static_cast<HXGInt>(26 * zoom), 20);
		HXGInt pinCount = static_cast<HXGInt>(std::max(node.Inputs.size(), node.Outputs.size()));
		HXGInt bodyH = 10 + pinCount * pinSpacing + 10;
		if (!node.Values.empty()) {
			for (const auto& val : node.Values) {
				if (val.Type == HXPinType::Texture) {
					bodyH += 6 + 64 + std::max(static_cast<HXGInt>(16 * zoom), 12);
				}
				else {
					bodyH += 6 + std::max(static_cast<HXGInt>(20 * zoom), 14);
				}
			}
		}
		return { 240, titleH + bodyH };
	}

	static void DrawPin(HXBufferPainter* painter, HXNodePin& pin, HXPoint center, float zoom, bool isInput,
		const HXTheme* theme, const BlueprintProfile* profile) {
		pin.ScreenCenter = center;
		HXColor typeColor = GetPinColor(pin.Type, theme, profile);
		HXGInt	baseRadius = std::max(static_cast<HXGInt>(5 * zoom), 4); // 10px diameter
		HXGInt	radius = baseRadius;

		// Hover glow: scale 1.25 + outer glow (type color 30% opacity, blur 4px)
		if (pin.Hovered) {
			radius = static_cast<HXGInt>(baseRadius * 1.25f);
			for (int g = 3; g >= 1; --g) {
				int			  glowR = radius + g * 2;
				unsigned char alpha = static_cast<unsigned char>(typeColor.A * 0.10f * g);
				HXColor		  glow = { typeColor.R, typeColor.G, typeColor.B, alpha };
				if (pin.Type == HXPinType::Flow) {
					std::vector<HXPoint> pts = { { center.X, center.Y - glowR },
												{ center.X + glowR, center.Y },
												{ center.X, center.Y + glowR },
												{ center.X - glowR, center.Y } };
					painter->DrawFilledPolygon(pts, glow, glow);
				}
				else {
					HXRect gr{ center.X - glowR, center.Y - glowR, center.X + glowR, center.Y + glowR };
					painter->DrawFilledRoundedRectangle(gr, glow, glow, glowR);
				}
			}
		}

		if (pin.Type == HXPinType::Flow) {
			// Flow pin: diamond shape, 10x10px
			std::vector<HXPoint> diamond = { { center.X, center.Y - radius },
											  { center.X + radius, center.Y },
											  { center.X, center.Y + radius },
											  { center.X - radius, center.Y } };
			HXColor				 fillColor = pin.Connected ? theme->TextPrimary : theme->TextSecondary;
			painter->DrawFilledPolygon(diamond, fillColor, fillColor);
			// Subtle outline
			HXColor outline = theme->TextSecondary;
			painter->DrawLine(diamond[0], diamond[1], outline);
			painter->DrawLine(diamond[1], diamond[2], outline);
			painter->DrawLine(diamond[2], diamond[3], outline);
			painter->DrawLine(diamond[3], diamond[0], outline);
		}
		else {
			// Data pin: circle
			HXRect pinRect{ center.X - radius, center.Y - radius, center.X + radius, center.Y + radius };
			if (pin.Connected) {
				// Filled type color + 3px white highlight center
				painter->DrawFilledRoundedRectangle(pinRect, typeColor, typeColor, radius);
				HXGInt	hlR = std::max(static_cast<HXGInt>(1.5f * zoom), 1);
				HXColor hl{ 255, 255, 255, 200 };
				HXRect	hlRect{ center.X - hlR, center.Y - hlR, center.X + hlR, center.Y + hlR };
				painter->DrawFilledRoundedRectangle(hlRect, hl, hl, hlR);
			}
			else {
				// Hollow: 2px border in type color, body color inside
				painter->DrawFilledRoundedRectangle(pinRect, typeColor, typeColor, radius);
				HXGInt innerR = radius - std::max(static_cast<HXGInt>(2 * zoom), 2);
				if (innerR > 0) {
					HXRect innerRect{ center.X - innerR, center.Y - innerR, center.X + innerR, center.Y + innerR };
					painter->DrawFilledRoundedRectangle(innerRect, theme->PanelBackground, theme->PanelBackground, innerR);
				}
			}
		}
	}

	static void DrawNode(HXBufferPainter* painter, HXNode& node, float zoom, const HXPoint& offset,
		const HXRect& canvasRect, const HXTheme& theme, bool selected, bool hovered,
		BlueprintProfile* profile) {
		const HXTheme* themePtr = &theme;
		HXColor		   nodeBodyBorderColor = profile ? profile->NodeBodyBorderColor : theme.BorderSubtle;
		HXColor		   nodeBodyTopColor = profile ? profile->NodeBodyTopColor : theme.WindowTitleBackground;
		HXColor		   nodeBodyBottomColor = profile ? profile->NodeBodyBottomColor : theme.PanelBackground;
		HXColor		   nodeShadowColor = profile ? profile->NodeShadowColor : theme.ShadowColor;
		HXColor		   nodeTitleTextColor = profile ? profile->NodeTitleTextColor : theme.TextPrimary;
		HXColor		   nodeDefaultValueTextColor = profile ? profile->NodeDefaultValueTextColor : theme.TextTertiary;
		HXColor		   nodeSelectionBorderColor = profile ? profile->NodeSelectionBorderColor : theme.SelectedBorder;
		HXColor		   nodeHoverBorderColor = profile ? profile->NodeHoverBorderColor : theme.HoverBorder;
		HXColor		   commentBorderColor = profile ? profile->CommentBorderColor : HXColor{ 94, 234, 212, 170 };
		HXColor		   commentTextColor = profile ? profile->CommentTextColor : HXColor{ 255, 255, 255, 230 };

		// Comment node: large translucent box with big centered wrapped text
		if (node.Category == HXNodeCategory::Comment) {
			HXRect sr = WorldToScreenRect(node.Position, node.Size, zoom, offset);
			sr.Left += canvasRect.Left;
			sr.Right += canvasRect.Left;
			sr.Top += canvasRect.Top;
			sr.Bottom += canvasRect.Top;
			if (sr.Right < canvasRect.Left || sr.Left > canvasRect.Right || sr.Bottom < canvasRect.Top ||
				sr.Top > canvasRect.Bottom)
				return;

			HXColor col = GetCategoryColor(HXNodeCategory::Comment, themePtr, profile);
			painter->DrawFilledRoundedRectangle(sr, col, col, 8);
			painter->DrawRectangle(sr, commentBorderColor);

			HXGInt padding = std::max(static_cast<HXGInt>(16 * zoom), 12);
			HXGInt fontSize = std::max(static_cast<HXGInt>(24 * zoom), 16);
			HXGInt maxWidth = sr.CalWidth() - padding * 2;
			HXGInt maxHeight = sr.CalHeight() - padding * 2;
			if (maxWidth > 0 && maxHeight > 0 && !node.Title.empty()) {
				std::vector<HXString> lines;
				HXString			  line;
				HXGInt				  lineW = 0;
				for (size_t i = 0; i < node.Title.size(); ++i) {
					HXString ch = node.Title.substr(i, 1);
					HXGInt	 chW = painter->MeasureText(ch, HXFont{}, fontSize).CalWidth();
					if (lineW + chW > maxWidth && !line.empty()) {
						lines.push_back(line);
						line = ch;
						lineW = chW;
					}
					else {
						line += ch;
						lineW += chW;
					}
				}
				if (!line.empty())
					lines.push_back(line);

				HXGInt lineH = fontSize + 4;
				HXGInt totalH = static_cast<HXGInt>(lines.size()) * lineH;
				HXGInt startY = sr.Top + padding + (maxHeight - totalH) / 2;
				if (startY < sr.Top + padding)
					startY = sr.Top + padding;

				for (const auto& l : lines) {
					HXGInt lw = painter->MeasureText(l, HXFont{}, fontSize).CalWidth();
					HXGInt lx = sr.Left + padding + (maxWidth - lw) / 2;
					painter->DrawText(l, HXFont{}, { lx, startY }, commentTextColor, fontSize);
					startY += lineH;
				}
			}

			if (selected) {
				HXRect hl{ sr.Left - 2, sr.Top - 2, sr.Right + 2, sr.Bottom + 2 };
				painter->DrawRectangle(hl, nodeSelectionBorderColor);
			}
			else if (hovered) {
				HXRect hl{ sr.Left - 1, sr.Top - 1, sr.Right + 1, sr.Bottom + 1 };
				painter->DrawRectangle(hl, nodeHoverBorderColor);
			}
			return;
		}

		// Auto-calculate size
		node.Size = CalculateNodeSize(node, 1.0f);
		HXRect sr = WorldToScreenRect(node.Position, node.Size, zoom, offset);
		sr.Left += canvasRect.Left;
		sr.Right += canvasRect.Left;
		sr.Top += canvasRect.Top;
		sr.Bottom += canvasRect.Top;

		// Cache pin screen positions BEFORE visibility cull
		HXGInt pinSpacing = std::max(static_cast<HXGInt>(26 * zoom), 20);
		HXGInt pinRadius = std::max(static_cast<HXGInt>(5 * zoom), 4);
		HXGInt titleH = std::max(static_cast<HXGInt>(32 * zoom), 24);
		HXGInt bodyTop = sr.Top + titleH + 10;
		for (size_t i = 0; i < node.Inputs.size(); ++i) {
			HXGInt py = bodyTop + static_cast<HXGInt>(i) * pinSpacing + pinSpacing / 2;
			node.Inputs[i].ScreenCenter = { sr.Left + 14 + pinRadius, py };
		}
		for (size_t i = 0; i < node.Outputs.size(); ++i) {
			HXGInt py = bodyTop + static_cast<HXGInt>(i) * pinSpacing + pinSpacing / 2;
			node.Outputs[i].ScreenCenter = { sr.Right - 14 - pinRadius, py };
		}

		if (sr.Right < canvasRect.Left || sr.Left > canvasRect.Right || sr.Bottom < canvasRect.Top ||
			sr.Top > canvasRect.Bottom) {
			return;
		}

		HXGInt	radius = std::max(static_cast<HXGInt>(theme.RadiusLg * zoom), 6);
		HXColor catColor = GetCategoryColor(node.Category, themePtr, profile);
		HXColor bodyBg = nodeBodyBottomColor;
		HXColor titleBg = nodeBodyTopColor;
		HXColor borderColor = nodeBodyBorderColor;

		// 1) Selection glow (drawn first, behind node)
		if (selected) {
			for (int i = 4; i >= 1; --i) {
				int			  expand = i;
				unsigned char alpha = static_cast<unsigned char>(nodeSelectionBorderColor.A * 0.10f);
				HXColor glow = { nodeSelectionBorderColor.R, nodeSelectionBorderColor.G, nodeSelectionBorderColor.B, alpha };
				HXRect	gr{ sr.Left - expand, sr.Top - expand, sr.Right + expand, sr.Bottom + expand };
				painter->DrawFilledRoundedRectangle(gr, glow, glow, radius + expand);
			}
			HXRect selR{ sr.Left - 2, sr.Top - 2, sr.Right + 2, sr.Bottom + 2 };
			painter->DrawFilledRoundedRectangle(selR, nodeSelectionBorderColor, nodeSelectionBorderColor, radius + 2);
			painter->DrawFilledRoundedRectangle(sr, bodyBg, bodyBg, radius);
		}
		else if (hovered) {
			HXRect hr{ sr.Left - 1, sr.Top - 1, sr.Right + 1, sr.Bottom + 1 };
			painter->DrawFilledRoundedRectangle(hr, nodeHoverBorderColor, nodeHoverBorderColor, radius + 1);
			painter->DrawFilledRoundedRectangle(sr, bodyBg, bodyBg, radius);
		}

		// 2) Shadow (simulate with translucent layers)
		for (int i = 3; i >= 1; --i) {
			int			  off = 2 + i;
			int			  expand = i * 2;
			unsigned char alpha = static_cast<unsigned char>(nodeShadowColor.A * (0.12f * i));
			HXColor		  sc = { nodeShadowColor.R, nodeShadowColor.G, nodeShadowColor.B, alpha };
			HXRect		  sh{ sr.Left + off, sr.Top + off, sr.Right + off + expand, sr.Bottom + off + expand };
			painter->DrawFilledRoundedRectangle(sh, sc, sc, radius);
		}

		// 3) Body background
		painter->DrawFilledRoundedRectangle(sr, bodyBg, bodyBg, radius);

		// 4) Title bar (rounded top only)
		HXRect titleRounded{ sr.Left, sr.Top, sr.Right, sr.Top + titleH + radius };
		painter->DrawFilledRoundedRectangle(titleRounded, titleBg, titleBg, radius);
		painter->DrawFilledRectangle({ sr.Left, sr.Top + titleH, sr.Right, sr.Top + titleH + radius + 1 }, titleBg, titleBg);

		// Separator between title and body
		painter->DrawLine({ sr.Left, sr.Top + titleH }, { sr.Right, sr.Top + titleH }, borderColor);

		// 5) Category strip (4px wide, left side)
		{
			HXGInt stripW = std::max(static_cast<HXGInt>(4 * zoom), 3);
			HXRect stripRect{ sr.Left, sr.Top, sr.Left + stripW, sr.Bottom };
			painter->DrawFilledRoundedRectangle(stripRect, catColor, catColor, std::min(radius, std::max(stripW / 2, 1)));
		}

		// Title text with icon
		HXGInt titleFont = std::max(static_cast<HXGInt>(theme.FontSizeHeading * zoom), 13);
		HXGInt textY = sr.Top + (titleH - titleFont) / 2;
		HXGInt textX = sr.Left + std::max(static_cast<HXGInt>(12 * zoom), 8);
		if (!node.Icon.empty()) {
			painter->DrawText(node.Icon, HXFont{}, { textX, textY }, nodeTitleTextColor, titleFont);
			textX += titleFont + std::max(static_cast<HXGInt>(6 * zoom), 4);
		}
		painter->DrawText(node.Title, HXFont{}, { textX, textY }, nodeTitleTextColor, titleFont);

		// 6) Pins (auto-layout)
		HXGInt pinLabelFont = std::max(static_cast<HXGInt>(theme.FontSizeCaption * zoom), 11);
		HXGInt labelOffset = std::max(static_cast<HXGInt>(8 * zoom), 6);

		// Input pins
		for (size_t i = 0; i < node.Inputs.size(); ++i) {
			auto& pin = node.Inputs[i];
			HXGInt	py = bodyTop + static_cast<HXGInt>(i) * pinSpacing + pinSpacing / 2;
			HXPoint pc{ sr.Left + 14 + pinRadius, py };
			DrawPin(painter, pin, pc, zoom, true, themePtr, profile);
			// Label: left-aligned, to the right of pin
			if (!pin.Label.empty()) {
				HXGInt lx = pc.X + pinRadius + labelOffset;
				painter->DrawText(pin.Label, HXFont{}, { lx, py - pinLabelFont / 2 }, theme.TextSecondary, pinLabelFont);
			}
			// Default value display disabled to avoid overlap with output labels
		}

		// Output pins
		for (size_t i = 0; i < node.Outputs.size(); ++i) {
			auto& pin = node.Outputs[i];
			HXGInt	py = bodyTop + static_cast<HXGInt>(i) * pinSpacing + pinSpacing / 2;
			HXPoint pc{ sr.Right - 14 - pinRadius, py };
			DrawPin(painter, pin, pc, zoom, false, themePtr, profile);
			// Right-aligned label
			if (!pin.Label.empty()) {
				int	   labelW = painter->MeasureText(pin.Label, HXFont{}, pinLabelFont).CalWidth();
				HXGInt lx = pc.X - pinRadius - labelOffset - labelW;
				painter->DrawText(pin.Label, HXFont{}, { lx, py - pinLabelFont / 2 }, theme.TextSecondary, pinLabelFont);
			}
		}

		// 7) Node Values
		if (!node.Values.empty()) {
			HXGInt valueY =
				bodyTop + static_cast<HXGInt>(std::max(node.Inputs.size(), node.Outputs.size())) * pinSpacing + 10;
			HXGInt valueFont = std::max(static_cast<HXGInt>(theme.FontSizeBody * zoom), 12);
			for (int i = 0; i < static_cast<int>(node.Values.size()); ++i) {
				bool beingEdited = profile && profile->_internal.Editing && profile->_internal.EditingNodeId == node.Id &&
					!profile->_internal.EditingIsPin && profile->_internal.EditingValueIdx == i;
				if (beingEdited) {
					valueY += valueFont + 4;
					continue;
				}
				// Texture preview
				if (node.Values[i].Type == HXPinType::Texture) {
					IMAGE* img = GetCachedTextureImage(node.Values[i].Value);
					if (img) {
						int	  imgW = img->getwidth();
						int	  imgH = img->getheight();
						int	  maxThumbW = std::max(static_cast<int>(sr.CalWidth() - 28), 20);
						int	  maxThumbH = 64;
						float scale = std::min(static_cast<float>(maxThumbW) / imgW, static_cast<float>(maxThumbH) / imgH);
						scale = std::min(scale, 1.0f);
						int drawW = static_cast<int>(imgW * scale);
						int drawH = static_cast<int>(imgH * scale);
						painter->DrawImage(img, { sr.Left + 14, valueY }, { drawW, drawH });
						valueY += drawH + 4;
					}
					else {
						HXRect textureFallback{ sr.Left + 14, valueY, sr.Left + 54, valueY + 40 };
						painter->DrawFilledRoundedRectangle(textureFallback, profile->TextureFallbackBackground,
							profile->TextureFallbackBackground, 6);
						painter->DrawRectangle(textureFallback, profile->TextureFallbackBorder);
						painter->DrawText(HXStr("?"), HXFont{}, { sr.Left + 30, valueY + 12 }, profile->TextureFallbackText,
							16);
						valueY += 44;
					}
					HXString pathLine = node.Values[i].Value;
					if (pathLine.length() > 20)
						pathLine = pathLine.substr(0, 17) + HXStr("...");
					painter->DrawText(pathLine, HXFont{}, { sr.Left + 14, valueY }, profile->TextureFallbackText, valueFont);
					valueY += valueFont + 4;
				}
				else {
					HXString line = node.Values[i].Name + HXStr(": ") + node.Values[i].Value;
					painter->DrawText(line, HXFont{}, { sr.Left + 14, valueY }, theme.TextSecondary, valueFont);
					// Color swatch for Color-type values (16x16, radius 4)
					if (node.Values[i].Type == HXPinType::Color) {
						HXColor swatchColor = ParseColorString(node.Values[i].Value);
						HXGInt	swatchSize = std::max(static_cast<HXGInt>(16 * zoom), 12);
						int		textW = painter->MeasureText(line, HXFont{}, valueFont).CalWidth();
						HXRect	swatchRect{ sr.Left + 18 + textW, valueY, sr.Left + 18 + textW + swatchSize,
										   valueY + swatchSize };
						painter->DrawFilledRoundedRectangle(swatchRect, swatchColor, swatchColor,
							std::max(static_cast<HXGInt>(4 * zoom), 3));
						painter->DrawRectangle(swatchRect, profile->ColorSwatchBorder);
					}
					valueY += valueFont + 4;
				}
			}
		}

		// 8) Selection / hover border (drawn LAST)
		if (selected) {
			HXRect hl{ sr.Left - 2, sr.Top - 2, sr.Right + 2, sr.Bottom + 2 };
			painter->DrawRectangle(hl, nodeSelectionBorderColor);
		}
		else if (hovered) {
			HXRect hl{ sr.Left - 1, sr.Top - 1, sr.Right + 1, sr.Bottom + 1 };
			painter->DrawRectangle(hl, nodeHoverBorderColor);
		}
	}

	static HXNode* HitTestNode(HXNodeGraph* graph, HXPoint worldMouse) {
		for (auto it = graph->Nodes.rbegin(); it != graph->Nodes.rend(); ++it) {
			if (it->Category == HXNodeCategory::Comment) {
				HXRect rect{ it->Position.X, it->Position.Y, it->Position.X + it->Size.X, it->Position.Y + it->Size.Y };
				if (rect.PointInside(worldMouse))
					return &*it;
			}
			else {
				HXPoint sz = CalculateNodeSize(*it, 1.0f);
				HXRect	rect{ it->Position.X, it->Position.Y, it->Position.X + sz.X, it->Position.Y + sz.Y };
				if (rect.PointInside(worldMouse))
					return &*it;
			}
		}
		return nullptr;
	}

	struct PinHitResult {
		HXNode* Node = nullptr;
		HXNodePin* Pin = nullptr;
		int		   PinIndex = -1;
		bool	   IsInput = false;
	};

	static PinHitResult HitTestPin(HXNodeGraph* graph, HXPoint worldMouse, float zoom) {
		float pinRadiusWorld = 12.0f / std::max(zoom, 0.001f);
		float r2 = pinRadiusWorld * pinRadiusWorld;
		for (auto it = graph->Nodes.rbegin(); it != graph->Nodes.rend(); ++it) {
			if (it->Category == HXNodeCategory::Comment)
				continue;
			HXPoint sz = CalculateNodeSize(*it, 1.0f);
			HXGInt	titleH = 32;
			HXGInt	pinSpacing = 26;
			HXGInt	pinRadius = 5;
			HXGInt	bodyTopY = it->Position.Y + titleH + 10;
			for (int i = 0; i < static_cast<int>(it->Inputs.size()); ++i) {
				auto& pin = it->Inputs[i];
				float px = it->Position.X + 14 + pinRadius;
				float py = bodyTopY + i * pinSpacing + pinSpacing / 2;
				float dx = worldMouse.X - px;
				float dy = worldMouse.Y - py;
				if (dx * dx + dy * dy <= r2)
					return { &*it, &pin, i, true };
			}
			for (int i = 0; i < static_cast<int>(it->Outputs.size()); ++i) {
				auto& pin = it->Outputs[i];
				float px = it->Position.X + sz.X - 14 - pinRadius;
				float py = bodyTopY + i * pinSpacing + pinSpacing / 2;
				float dx = worldMouse.X - px;
				float dy = worldMouse.Y - py;
				if (dx * dx + dy * dy <= r2)
					return { &*it, &pin, i, false };
			}
		}
		return {};
	}

	static bool CanConnect(const HXNodePin* a, const HXNodePin* b) {
		if (!a || !b)
			return false;
		if (a->Direction == b->Direction)
			return false;
		if (a->Type == HXPinType::Flow && b->Type != HXPinType::Flow)
			return false;
		if (a->Type != HXPinType::Flow && b->Type == HXPinType::Flow)
			return false;
		if (a->Type != b->Type && a->Type != HXPinType::Custom && b->Type != HXPinType::Custom)
			return false;
		return true;
	}

	static bool IsNodeSelected(const BlueprintProfile& profile, const HXString& id) {
		for (const auto& s : profile._internal.SelectedNodes)
			if (s == id)
				return true;
		return false;
	}

	static bool IsPointerValid(HXNodeGraph* graph, HXNode* node) {
		if (!node)
			return false;
		for (auto& n : graph->Nodes)
			if (&n == node)
				return true;
		return false;
	}

	////////////////////////////////////////////////////////////
	/// Clipboard

	static std::vector<HXNode>	   G_BlueprintClipboard;
	static std::vector<HXNodeLink> G_BlueprintClipboardLinks;
	static int					   G_BlueprintPasteCounter = 0;

	HXString					   MakeBlueprintNodeId(const HXString& base) {
		return base + HXStr("_") + std::to_wstring(++G_BlueprintPasteCounter);
	}

	static HXString MakeUniqueNodeId(const HXString& base) {
		return MakeBlueprintNodeId(base);
	}

	static void CopyNodesToClipboard(const std::vector<HXString>& ids, HXNodeGraph* graph) {
		G_BlueprintClipboard.clear();
		G_BlueprintClipboardLinks.clear();
		for (const auto& id : ids) {
			auto* n = graph->FindNode(id);
			if (n)
				G_BlueprintClipboard.push_back(*n);
		}
		// Collect pin IDs belonging to selected nodes
		std::unordered_set<HXString> selectedPinIds;
		for (const auto& node : G_BlueprintClipboard) {
			for (const auto& pin : node.Inputs)
				selectedPinIds.insert(pin.Id);
			for (const auto& pin : node.Outputs)
				selectedPinIds.insert(pin.Id);
		}
		// Copy internal links (both ends inside selection)
		for (const auto& link : graph->Links) {
			if (selectedPinIds.count(link.StartPinId) && selectedPinIds.count(link.EndPinId)) {
				G_BlueprintClipboardLinks.push_back(link);
			}
		}
	}

	static void PasteNodesFromClipboard(HXNodeGraph* graph, const HXPoint& worldPos,
		std::vector<HXString>* outNewNodeIds = nullptr) {
		if (G_BlueprintClipboard.empty())
			return;
		HXPoint minP{ INT_MAX, INT_MAX }, maxP{ INT_MIN, INT_MIN };
		for (const auto& n : G_BlueprintClipboard) {
			minP.X = std::min(minP.X, n.Position.X);
			minP.Y = std::min(minP.Y, n.Position.Y);
			maxP.X = std::max(maxP.X, n.Position.X + n.Size.X);
			maxP.Y = std::max(maxP.Y, n.Position.Y + n.Size.Y);
		}
		HXPoint								   center{ (minP.X + maxP.X) / 2, (minP.Y + maxP.Y) / 2 };
		HXPoint								   delta{ worldPos.X - center.X, worldPos.Y - center.Y };

		std::unordered_map<HXString, HXString> pinIdMap;
		for (auto& src : G_BlueprintClipboard) {
			HXNode node = src;
			node.Id = MakeUniqueNodeId(src.Id);
			node.Position.X += delta.X;
			node.Position.Y += delta.Y;
			for (auto& pin : node.Inputs) {
				HXString oldId = pin.Id;
				pin.Id = MakeUniqueNodeId(pin.Id);
				pinIdMap[oldId] = pin.Id;
			}
			for (auto& pin : node.Outputs) {
				HXString oldId = pin.Id;
				pin.Id = MakeUniqueNodeId(pin.Id);
				pinIdMap[oldId] = pin.Id;
			}
			graph->Nodes.push_back(node);
			if (outNewNodeIds)
				outNewNodeIds->push_back(node.Id);
		}

		// Restore internal links
		for (const auto& link : G_BlueprintClipboardLinks) {
			auto itS = pinIdMap.find(link.StartPinId);
			auto itE = pinIdMap.find(link.EndPinId);
			if (itS != pinIdMap.end() && itE != pinIdMap.end()) {
				graph->Links.push_back(HXNodeLink{ itS->second, itE->second });
			}
		}
	}

	////////////////////////////////////////////////////////////
	/// Context menu data

	static std::vector<std::pair<HXString, std::vector<HXString>>> G_BPMenuCategories = {
		{ HXStr("Math"),
		 { HXStr("Add"), HXStr("Subtract"), HXStr("Multiply"), HXStr("Divide"), HXStr("Sine"), HXStr("Cosine"),
		  HXStr("Random") } },
		{ HXStr("Compare"),{ HXStr("Equals"), HXStr("Greater Than"), HXStr("Less Than") } },
		{ HXStr("Logic"),{ HXStr("AND"), HXStr("OR"), HXStr("NOT") } },
		{ HXStr("Constant"),{ HXStr("Float"), HXStr("Int"), HXStr("Bool"), HXStr("String"), HXStr("Color") } },
		{ HXStr("Flow"),{ HXStr("Start"), HXStr("Branch"), HXStr("Sequence"), HXStr("ForLoop") } },
		{ HXStr("Variable"),{ HXStr("Variable"), HXStr("Get Variable"), HXStr("Set Variable") } },
		{ HXStr("Function"),{ HXStr("Print"), HXStr("Time") } },
		{ HXStr("Conversion"),{ HXStr("Float to Int"), HXStr("Int to Float"), HXStr("Float to String"), HXStr("Int to String"), HXStr("Bool to String"), HXStr("Color to String"), HXStr("Vector to String"), HXStr("String to Float"), HXStr("String to Int") } },
		{ HXStr("String"),{ HXStr("String Append") } },
		{ HXStr("Texture"),{ HXStr("Texture"), HXStr("Texture Sample"), HXStr("Sample Texture 2D") } },
		{ HXStr("UV"),{ HXStr("Tiling And Offset"), HXStr("Rotate UV"), HXStr("Flip UV") } },
		{ HXStr("Color"),
		 { HXStr("Color Multiply"), HXStr("Color Add"), HXStr("Color Lerp"), HXStr("One Minus"), HXStr("Saturate"),
		  HXStr("Component Mask"), HXStr("Blend"), HXStr("Grayscale"), HXStr("Normal Map") } },
		{ HXStr("Misc"),{ HXStr("Comment") } },
	};

	////////////////////////////////////////////////////////////
	/// Custom node registry

	struct BlueprintNodeRegistry {
		std::unordered_map<HXString, HXBlueprintDataHandler> DataHandlers;
		std::unordered_map<HXString, HXBlueprintFlowHandler> FlowHandlers;
		std::vector<HXBlueprintNodeTemplate>				 Templates;
	};

	static BlueprintNodeRegistry& GetBlueprintRegistry() {
		static BlueprintNodeRegistry reg;
		return reg;
	}

	static void RefreshBlueprintMenu() {
		// Remove previously appended custom categories (everything after built-in)
		static constexpr size_t BUILTIN_COUNT = 13;
		if (G_BPMenuCategories.size() > BUILTIN_COUNT)
			G_BPMenuCategories.resize(BUILTIN_COUNT);

		const auto& templates = GetBlueprintRegistry().Templates;
		if (templates.empty())
			return;

		std::map<HXString, std::vector<HXString>> customCats;
		for (const auto& tmpl : templates) {
			customCats[tmpl.Category].push_back(tmpl.Title);
		}
		for (auto& [cat, items] : customCats) {
			G_BPMenuCategories.push_back({ cat, items });
		}
	}

	void RegisterBlueprintDataNode(const HXString& typeName, HXBlueprintDataHandler handler) {
		GetBlueprintRegistry().DataHandlers[typeName] = std::move(handler);
	}

	void RegisterBlueprintFlowNode(const HXString& typeName, HXBlueprintFlowHandler handler) {
		GetBlueprintRegistry().FlowHandlers[typeName] = std::move(handler);
	}

	void RegisterBlueprintNodeTemplate(const HXBlueprintNodeTemplate& tmpl) {
		GetBlueprintRegistry().Templates.push_back(tmpl);
		RefreshBlueprintMenu();
	}

	const std::vector<HXBlueprintNodeTemplate>& GetBlueprintNodeTemplates() {
		return GetBlueprintRegistry().Templates;
	}

	////////////////////////////////////////////////////////////
	/// Inline edit rendering

	static void DrawInlineEdit(HXBufferPainter* painter, BlueprintProfile& profile, const HXRect& canvasRect) {
		if (!profile._internal.Editing)
			return;
		auto* node = profile.Graph->FindNode(profile._internal.EditingNodeId);
		if (!node)
			return;

		// Calculate screen rect of the edited field
		HXRect sr = WorldToScreenRect(node->Position, node->Size, profile.Zoom, profile.Offset);
		sr.Left += canvasRect.Left;
		sr.Right += canvasRect.Left;
		sr.Top += canvasRect.Top;
		sr.Bottom += canvasRect.Top;

		HXGInt titleH = std::max(static_cast<HXGInt>(32 * profile.Zoom), 24);
		HXGInt pinSpacing = std::max(static_cast<HXGInt>(26 * profile.Zoom), 20);
		HXRect editRect{ 0, 0, 0, 0 };

		HXFont fnt{};
		HXGInt fontSize = std::max(static_cast<HXGInt>(15 * profile.Zoom), 12);

		if (profile._internal.EditingCommentTitle) {
			HXGInt commentFont = std::max(static_cast<HXGInt>(14 * profile.Zoom), 10);
			int	   textW = painter->MeasureText(profile._internal.EditState.Text, fnt, commentFont).CalWidth();
			HXGInt rectW = std::max(static_cast<HXGInt>(textW + 16), static_cast<HXGInt>(60));
			editRect = { sr.Left + 6, sr.Top + 2, std::min(sr.Left + 6 + rectW, sr.Right - 4), sr.Top + 2 + commentFont + 6 };
			fontSize = commentFont;
		}
		else if (profile._internal.EditingIsPin) {
			int idx = profile._internal.EditingPinIdx;
			if (idx < 0 || idx >= static_cast<int>(node->Inputs.size()))
				return;
			HXGInt py = sr.Top + titleH + 10 + idx * pinSpacing + pinSpacing / 2;
			HXGInt pinR = std::max(static_cast<HXGInt>(5 * profile.Zoom), 4);
			int	   textW = painter->MeasureText(profile._internal.EditState.Text, fnt, fontSize).CalWidth();
			HXGInt rectW = std::max(static_cast<HXGInt>(textW + 12), static_cast<HXGInt>(40));
			editRect = { sr.Left + pinR + 14 + 70, py - 8, std::min(sr.Left + pinR + 2 + 70 + rectW, sr.Right - 10),
							py + 10 };
		}
		else {
			int idx = profile._internal.EditingValueIdx;
			if (idx < 0 || idx >= static_cast<int>(node->Values.size()))
				return;
			HXGInt valueFont = std::max(static_cast<HXGInt>(14 * profile.Zoom), 10);
			HXGInt valueY = sr.Top + titleH + 10 +
				static_cast<HXGInt>(std::max(node->Inputs.size(), node->Outputs.size())) * pinSpacing + 10;
			valueY += idx * (valueFont + 4);
			int	   textW = painter->MeasureText(profile._internal.EditState.Text, fnt, fontSize).CalWidth();
			HXGInt rectW = std::max(static_cast<HXGInt>(textW + 12), static_cast<HXGInt>(40));
			editRect = { sr.Left + 14, valueY - 2, std::min(sr.Left + 14 + rectW, sr.Right - 14), valueY + valueFont + 4 };
		}

		// Inline color picker for HXPinType::Color values
		if (!profile._internal.EditingIsPin && profile._internal.EditingColor && profile._internal.EditingValueIdx >= 0 &&
			profile._internal.EditingValueIdx < static_cast<int>(node->Values.size()) &&
			node->Values[profile._internal.EditingValueIdx].Type == HXPinType::Color) {
			constexpr HXGInt sqSize = 140;
			constexpr HXGInt hueH = 14;
			constexpr HXGInt grid = 28;
			HXGInt			 pickerW = sqSize + 8;
			HXGInt			 pickerH = sqSize + 4 + hueH + 4 + 18;

			HXRect pickerRect{ editRect.Left, editRect.Bottom + 2, editRect.Left + pickerW, editRect.Bottom + 2 + pickerH };
			if (pickerRect.Right > canvasRect.Right)
				pickerRect.Left -= (pickerRect.Right - canvasRect.Right);
			if (pickerRect.Bottom > canvasRect.Bottom)
				pickerRect.Top -= (pickerRect.Bottom - canvasRect.Bottom);
			if (pickerRect.Left < canvasRect.Left)
				pickerRect.Left = canvasRect.Left;
			if (pickerRect.Top < canvasRect.Top)
				pickerRect.Top = canvasRect.Top;
			pickerRect.Right = pickerRect.Left + pickerW;
			pickerRect.Bottom = pickerRect.Top + pickerH;

			// Cache rects for input handling
			profile._internal._pickerRect = pickerRect;
			profile._internal._sqRect = { pickerRect.Left + 4, pickerRect.Top + 4, pickerRect.Left + 4 + sqSize,
											 pickerRect.Top + 4 + sqSize };
			profile._internal._hueRect = { pickerRect.Left + 4, profile._internal._sqRect.Bottom + 4,
											 pickerRect.Left + 4 + sqSize, profile._internal._sqRect.Bottom + 4 + hueH };

			painter->DrawFilledRectangle(pickerRect, profile.ColorPickerBackgroundColor,
				profile.ColorPickerBackgroundColor);
			painter->DrawRectangle(pickerRect, profile.ColorPickerBorderColor);

			// Sat/Val square
			HXRect sqRect = profile._internal._sqRect;
			HXGInt cellSize = sqSize / grid;
			for (int gy = 0; gy < grid; ++gy) {
				for (int gx = 0; gx < grid; ++gx) {
					float	cs = (gx + 0.5f) / grid;
					float	cv = 1.0f - (gy + 0.5f) / grid;
					HXColor c = HSVToRGB(profile._internal.ColorH, cs, cv);
					HXRect	cr{ sqRect.Left + gx * cellSize, sqRect.Top + gy * cellSize, sqRect.Left + (gx + 1) * cellSize,
							   sqRect.Top + (gy + 1) * cellSize };
					painter->DrawFilledRectangle(cr, c, c);
				}
			}
			HXGInt selX = sqRect.Left + static_cast<HXGInt>(profile._internal.ColorS * sqSize);
			HXGInt selY = sqRect.Top + static_cast<HXGInt>((1.0f - profile._internal.ColorV) * sqSize);
			painter->DrawRectangle({ selX - 3, selY - 3, selX + 3, selY + 3 }, profile.ColorPickerCursorColor);

			// Hue bar — smooth gradient, 1px per hue step
			HXRect hueRect = profile._internal._hueRect;
			for (int hx = 0; hx < sqSize; ++hx) {
				float	hh = static_cast<float>(hx) / sqSize * 360.0f;
				HXColor c = HSVToRGB(hh, 1.0f, 1.0f);
				HXRect	cr{ hueRect.Left + hx, hueRect.Top, hueRect.Left + hx + 1, hueRect.Bottom };
				painter->DrawFilledRectangle(cr, c, c);
			}
			HXGInt hueX = hueRect.Left + static_cast<HXGInt>((profile._internal.ColorH / 360.0f) * sqSize);
			painter->DrawLine({ hueX, hueRect.Top - 1 }, { hueX, hueRect.Bottom + 1 }, profile.ColorPickerCursorColor);

			// Preview + RGB text
			HXColor curColor = HSVToRGB(profile._internal.ColorH, profile._internal.ColorS, profile._internal.ColorV);
			HXRect	previewRect{ pickerRect.Left + 4, hueRect.Bottom + 4, pickerRect.Left + 4 + 32, hueRect.Bottom + 4 + 16 };
			painter->DrawFilledRectangle(previewRect, curColor, curColor);
			painter->DrawRectangle(previewRect, profile.ColorPickerPreviewBorder);
			HXString rgbText = std::to_wstring(curColor.R) + HXStr(",") + std::to_wstring(curColor.G) + HXStr(",") +
				std::to_wstring(curColor.B);
			painter->DrawText(rgbText, HXFont{}, { previewRect.Right + 6, previewRect.Top + 2 },
				profile.ColorPickerPreviewText, 11);
			return;
		}

		// Render via reusable single-line editor
		HXSingleLineEditor editor;
		editor.State = &profile._internal.EditState;
		editor.Bounds = editRect;
		editor.Active = true;
		editor.Style.FontSize = static_cast<int>(fontSize);
		editor.Style.TextColor = profile.InlineEditorTextColor;
		editor.Style.SelectionBg = profile.InlineEditorSelectionBg;
		editor.Style.SelectionTextColor = profile.InlineEditorTextColor;
		editor.Style.CursorColor = profile.InlineEditorTextColor;
		editor.Style.BackgroundColor = profile.InlineEditorBackgroundColor;
		editor.Style.BorderColor = profile.InlineEditorBorderColor;
		editor.Style.BorderFocusColor = profile.InlineEditorBorderColor;
		editor.Style.LeftPadding = 4;
		SingleLineEditorRender(editor, painter);
	}

	////////////////////////////////////////////////////////////
	/// Context menu rendering

	static void DrawContextMenu(HXBufferPainter* painter, BlueprintProfile& profile, const HXRect& canvasRect) {
		if (!profile._internal.ContextMenuOpen)
			return;
		HXPoint pos = profile._internal.ContextMenuScreenPos;
		// Clamp to canvas
		if (pos.X < canvasRect.Left)
			pos.X = canvasRect.Left;
		if (pos.Y < canvasRect.Top)
			pos.Y = canvasRect.Top;

		constexpr HXGInt catWidth = 120;
		constexpr HXGInt itemWidth = 140;
		constexpr HXGInt rowH = 26;
		HXGInt			 totalH = 0;
		for (const auto& cat : G_BPMenuCategories) {
			totalH = std::max(totalH, static_cast<HXGInt>(cat.second.size() * rowH + 8));
		}
		totalH = std::max(totalH, static_cast<HXGInt>(G_BPMenuCategories.size() * rowH + 8));

		HXRect menuRect{ pos.X, pos.Y, pos.X + catWidth + itemWidth, pos.Y + totalH };
		if (menuRect.Right > canvasRect.Right)
			menuRect.Left -= (menuRect.Right - canvasRect.Right);
		if (menuRect.Bottom > canvasRect.Bottom)
			menuRect.Top -= (menuRect.Bottom - canvasRect.Bottom);
		if (menuRect.Left < canvasRect.Left)
			menuRect.Left = canvasRect.Left;
		if (menuRect.Top < canvasRect.Top)
			menuRect.Top = canvasRect.Top;
		menuRect.Right = menuRect.Left + catWidth + itemWidth;
		menuRect.Bottom = menuRect.Top + totalH;

		// Background (rounded 12px)
		painter->DrawFilledRoundedRectangle(menuRect, profile.MenuBackgroundTopColor, profile.MenuBackgroundBottomColor,
			12);
		painter->DrawRectangle(menuRect, profile.MenuBorderColor);

		// Category list (left column)
		HXGInt cy = menuRect.Top + 4;
		for (int c = 0; c < static_cast<int>(G_BPMenuCategories.size()); ++c) {
			HXRect catRect{ menuRect.Left, cy, menuRect.Left + catWidth, cy + rowH };
			if (c == profile._internal.ContextMenuHoverCategory) {
				painter->DrawFilledRectangle(catRect, profile.MenuCategoryHoverColor, profile.MenuCategoryHoverColor);
			}
			painter->DrawText(G_BPMenuCategories[c].first, HXFont{}, { catRect.Left + 10, catRect.Top + 4 },
				profile.MenuCategoryTextColor, 13);
			cy += rowH;
		}

		// Items for hovered category (right column)
		int hc = profile._internal.ContextMenuHoverCategory;
		if (hc >= 0 && hc < static_cast<int>(G_BPMenuCategories.size())) {
			HXGInt iy = menuRect.Top + 4;
			for (int i = 0; i < static_cast<int>(G_BPMenuCategories[hc].second.size()); ++i) {
				HXRect itemRect{ menuRect.Left + catWidth, iy, menuRect.Right, iy + rowH };
				if (i == profile._internal.ContextMenuHoverItem) {
					painter->DrawFilledRectangle(itemRect, profile.MenuItemHoverColor, profile.MenuItemHoverColor);
				}
				painter->DrawText(G_BPMenuCategories[hc].second[i], HXFont{}, { itemRect.Left + 10, itemRect.Top + 4 },
					profile.MenuItemTextColor, 13);
				iy += rowH;
			}
		}

		// Separator line
		painter->DrawLine({ menuRect.Left + catWidth, menuRect.Top + 4 }, { menuRect.Left + catWidth, menuRect.Bottom - 4 },
			profile.MenuSeparatorColor);
	}

	////////////////////////////////////////////////////////////
	/// BeginBlueprint / EndBlueprint

	HXNode MakeBlueprintNode(const HXString& typeName, HXPoint pos, BlueprintProfile* profile) {
		HX::HXNode newNode;
		if (profile && profile->CustomNodeFactory) {
			if (profile->CustomNodeFactory(typeName, pos, newNode)) {
				return newNode;
			}
		}
		if (typeName == HXStr("Texture Sample")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("tex")),
								 HXStr("Texture Sample"),
								 HXStr("\u25A0"),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 200, 100 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("uv")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr(""), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("rgb")), HXStr("RGB"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Texture")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("texnode")),
								 HXStr("Texture"),
								 HXStr("\u25A0"),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 160, 140 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("texout")), HXStr("Texture"), HX::HXPinType::Texture,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Texture"), HXStr("./diffuse.png"), HX::HXPinType::Texture } },
								 false };
		}
		else if (typeName == HXStr("Multiply")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("mult")),
								 HXStr("Multiply"),
								 HXStr("\u00D7"),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 180, 100 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0.5"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Constant")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("const")),
								 HXStr("Constant"),
								 HXStr("\u25CF"),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 160, 100 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("val")), HXStr("Value"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Value"), HXStr("1.0"), HX::HXPinType::Float } },
								 false };
		}
		else if (typeName == HXStr("Color")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("color")),
								 HXStr("Color"),
								 HXStr("\u25CF"),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 160, 100 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("rgb")), HXStr("RGB"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Color"), HXStr("1.0,0.5,0.2"), HX::HXPinType::Color } },
								 false };
		}
		else if (typeName == HXStr("Material Output")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("out")),
								 HXStr("Material Output"),
								 HXStr("\u25B6"),
								 HX::HXNodeCategory::Output,
								 pos,
								 { 200, 100 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("base")), HXStr("Base Color"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("rough")), HXStr("Roughness"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0.5"), false } },
								 {},
								 {},
								 false };
		}
		else if (typeName == HXStr("Start")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("start")),
								 HXStr("Start"),
								 HXStr(""),
								 HX::HXNodeCategory::Event,
								 pos,
								 { 160, 70 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Out"), HX::HXPinType::Flow,
													HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Branch")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("branch")),
								 HXStr("Branch"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 180, 100 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("In"), HX::HXPinType::Flow,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("cond")), HXStr("Condition"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr(""), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("true")), HXStr("True"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("false")), HXStr("False"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Add")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("add")),
								 HXStr("Add"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 160, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Subtract")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("sub")),
								 HXStr("Subtract"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 160, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Divide")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("div")),
								 HXStr("Divide"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 160, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("1"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("1"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Sequence")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("seq")),
								 HXStr("Sequence"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 160, 100 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("In"), HX::HXPinType::Flow,
												HX::HXPinDirection::Input, HXStr(""), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out1")), HXStr("Then 1"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("out2")), HXStr("Then 2"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Variable")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("var")),
								 HXStr("Variable"),
								 HXStr(""),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 160, 70 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("ref")), HXStr("Ref"), HX::HXPinType::String,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Name"), HXStr("MyVariable"), HX::HXPinType::String } },
								 false };
		}
		else if (typeName == HXStr("Get Variable")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("getvar")),
								 HXStr("Get Variable"),
								 HXStr(""),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 180, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("target")), HXStr("Target"), HX::HXPinType::String,
												HX::HXPinDirection::Input, HXStr(""), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("val")), HXStr("Value"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Name"), HXStr("MyVariable"), HX::HXPinType::String } },
								 false };
		}
		else if (typeName == HXStr("Set Variable")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("setvar")),
								 HXStr("Set Variable"),
								 HXStr(""),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 200, 110 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("In"), HX::HXPinType::Flow,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("target")), HXStr("Target"), HX::HXPinType::String,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("val")), HXStr("Value"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr(""), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Out"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Name"), HXStr("MyVariable"), HX::HXPinType::String } },
								 false };
		}
		else if (typeName == HXStr("Comment")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("comment")),
								 HXStr("Comment"),
								 HXStr(""),
								 HX::HXNodeCategory::Comment,
								 pos,
								 { 300, 150 },
								 {},
								 {},
								 {},
								 false };
		}
		else if (typeName == HXStr("Sine")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("sin")),
								 HXStr("Sine"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 160, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Cosine")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("cos")),
								 HXStr("Cosine"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 160, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Random")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("rand")),
								 HXStr("Random"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 160, 60 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Equals")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("eq")),
								 HXStr("Equals"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 160, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Bool,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Greater Than")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("gt")),
								 HXStr("Greater Than"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 180, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Bool,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Less Than")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("lt")),
								 HXStr("Less Than"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 160, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Bool,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("AND")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("and")),
								 HXStr("AND"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 160, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("True"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("True"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Bool,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("OR")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("or")),
								 HXStr("OR"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 160, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("False"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("False"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Bool,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("NOT")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("not")),
								 HXStr("NOT"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 160, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("False"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Bool,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Float")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("fconst")),
								 HXStr("Float"),
								 HXStr(""),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 140, 70 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("val")), HXStr("Value"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Value"), HXStr("0.0"), HX::HXPinType::Float } },
								 false };
		}
		else if (typeName == HXStr("Int")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("iconst")),
								 HXStr("Int"),
								 HXStr(""),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 140, 70 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("val")), HXStr("Value"), HX::HXPinType::Int,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Value"), HXStr("0"), HX::HXPinType::Int } },
								 false };
		}
		else if (typeName == HXStr("Bool")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("bconst")),
								 HXStr("Bool"),
								 HXStr(""),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 140, 70 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("val")), HXStr("Value"), HX::HXPinType::Bool,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Value"), HXStr("False"), HX::HXPinType::Bool } },
								 false };
		}
		else if (typeName == HXStr("String")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("sconst")),
								 HXStr("String"),
								 HXStr(""),
								 HX::HXNodeCategory::Variable,
								 pos,
								 { 160, 70 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("val")), HXStr("Value"), HX::HXPinType::String,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 { HX::HXNodeValue{ HXStr("Value"), HXStr("Hello"), HX::HXPinType::String } },
								 false };
		}
		else if (typeName == HXStr("ForLoop")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("for")),
								 HXStr("ForLoop"),
								 HXStr(""),
								 HX::HXNodeCategory::FlowControl,
								 pos,
								 { 200, 120 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("In"), HX::HXPinType::Flow,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("start")), HXStr("Start"), HX::HXPinType::Int,
												HX::HXPinDirection::Input, HXStr("0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("end")), HXStr("End"), HX::HXPinType::Int,
												HX::HXPinDirection::Input, HXStr("10"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("loop")), HXStr("Loop Body"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("idx")), HXStr("Index"), HX::HXPinType::Int,
												HX::HXPinDirection::Output, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("done")), HXStr("Completed"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Print")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("print")),
								 HXStr("Print"),
								 HXStr(""),
								 HX::HXNodeCategory::Function,
								 pos,
								 { 180, 90 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("In"), HX::HXPinType::Flow,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("msg")), HXStr("Message"), HX::HXPinType::String,
												HX::HXPinDirection::Input, HXStr("Hello"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Out"), HX::HXPinType::Flow,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Time")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("time")),
								 HXStr("Time"),
								 HXStr(""),
								 HX::HXNodeCategory::Function,
								 pos,
								 { 160, 60 },
								 {},
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Seconds"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Float to Int")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("f2i")),
								 HXStr("Float to Int"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 170, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Int,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Int to Float")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("i2f")),
								 HXStr("Int to Float"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 170, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::Int,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Float to String")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("f2s")),
								 HXStr("Float to String"),
								 HXStr(""),
								 HX::HXNodeCategory::Math,
								 pos,
								 { 180, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::String,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Int to String")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("i2s")),
									 HXStr("Int to String"),
									 HXStr(""),
									 HX::HXNodeCategory::Math,
									 pos,
									 { 180, 80 },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::Int,
																											HX::HXPinDirection::Input, HXStr("0"), false } },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::String,
																											HX::HXPinDirection::Output, HXStr(""), false } },
									 {},
									 false };
		}
		else if (typeName == HXStr("Bool to String")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("b2s")),
									 HXStr("Bool to String"),
									 HXStr(""),
									 HX::HXNodeCategory::Math,
									 pos,
									 { 180, 80 },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::Bool,
																											HX::HXPinDirection::Input, HXStr("False"), false } },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::String,
																											HX::HXPinDirection::Output, HXStr("False"), false } },
									 {},
									 false };
		}
		else if (typeName == HXStr("Color to String")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("c2s")),
									 HXStr("Color to String"),
									 HXStr(""),
									 HX::HXNodeCategory::Math,
									 pos,
									 { 180, 80 },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::Color,
																											HX::HXPinDirection::Input, HXStr("255,255,255"), false } },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::String,
																											HX::HXPinDirection::Output, HXStr("255,255,255"), false } },
									 {},
									 false };
		}
		else if (typeName == HXStr("Vector to String")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("v2s")),
									 HXStr("Vector to String"),
									 HXStr(""),
									 HX::HXNodeCategory::Math,
									 pos,
									 { 180, 80 },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::Vector,
																											HX::HXPinDirection::Input, HXStr("0,0"), false } },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::String,
																											HX::HXPinDirection::Output, HXStr("0,0"), false } },
									 {},
									 false };
		}
		else if (typeName == HXStr("String to Float")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("s2f")),
									 HXStr("String to Float"),
									 HXStr(""),
									 HX::HXNodeCategory::Math,
									 pos,
									 { 180, 80 },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::String,
																											HX::HXPinDirection::Input, HXStr("0.0"), false } },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Float,
																											HX::HXPinDirection::Output, HXStr(""), false } },
									 {},
									 false };
		}
		else if (typeName == HXStr("String to Int")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("s2i")),
									 HXStr("String to Int"),
									 HXStr(""),
									 HX::HXNodeCategory::Math,
									 pos,
									 { 180, 80 },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Value"), HX::HXPinType::String,
																											HX::HXPinDirection::Input, HXStr("0"), false } },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Int,
																											HX::HXPinDirection::Output, HXStr(""), false } },
									 {},
									 false };
		}
		else if (typeName == HXStr("String Append")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("sapp")),
									 HXStr("String Append"),
									 HXStr(""),
									 HX::HXNodeCategory::Function,
									 pos,
									 { 200, 90 },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::String,
																											HX::HXPinDirection::Input, HXStr(""), false },
									  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::String,
																											HX::HXPinDirection::Input, HXStr(""), false } },
									 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::String,
																											HX::HXPinDirection::Output, HXStr(""), false } },
									 {},
									 false };
		}
		else if (typeName == HXStr("Sample Texture 2D")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("sample")),
								 HXStr("Sample Texture 2D"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 200, 110 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("tex")), HXStr("Texture"), HX::HXPinType::Texture,
												HX::HXPinDirection::Input, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("uv")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("0.5,0.5"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("rgb")), HXStr("RGB"), HX::HXPinType::Color,
												HX::HXPinDirection::Output, HXStr(""), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Tiling And Offset")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("tiling")),
								 HXStr("Tiling And Offset"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 200, 120 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("uv")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("0.5,0.5"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("tiling")), HXStr("Tiling"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("1,1"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("offset")), HXStr("Offset"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("0,0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Rotate UV")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("rotuv")),
								 HXStr("Rotate UV"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 180, 110 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("uv")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("0.5,0.5"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("angle")), HXStr("Angle"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("center")), HXStr("Center"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("0.5,0.5"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Flip UV")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("flipuv")),
								 HXStr("Flip UV"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 180, 110 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("uv")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("0.5,0.5"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("flipx")), HXStr("Flip X"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("False"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("flipy")), HXStr("Flip Y"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("False"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("UV"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Color Multiply")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("cmult")),
								 HXStr("Color Multiply"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 180, 100 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("1,1,1"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("1,1,1"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Color,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Color Add")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("cadd")),
								 HXStr("Color Add"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 180, 100 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("0,0,0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("0,0,0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Color,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Color Lerp")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("clerp")),
								 HXStr("Color Lerp"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 180, 110 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("0,0,0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("1,1,1"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("t")), HXStr("T"), HX::HXPinType::Float,
												HX::HXPinDirection::Input, HXStr("0.5"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Color,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("One Minus")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("oneminus")),
								 HXStr("One Minus"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 160, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Input"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("1,1,1"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Color,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Saturate")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("sat")),
								 HXStr("Saturate"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 160, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Input"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("1,1,1"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Color,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Component Mask")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("mask")),
								 HXStr("Component Mask"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 180, 130 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Input"), HX::HXPinType::Vector,
												HX::HXPinDirection::Input, HXStr("0,0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("r")), HXStr("R"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("True"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("g")), HXStr("G"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("True"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("b")), HXStr("B"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("False"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("a")), HXStr("A"), HX::HXPinType::Bool,
												HX::HXPinDirection::Input, HXStr("False"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Blend")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("blend")),
								 HXStr("Blend"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 200, 110 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("base")), HXStr("Base"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("1,1,1"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("blend")), HXStr("Blend"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("0,0,0"), false },
								  HX::HXNodePin{ MakeUniqueNodeId(HXStr("mode")), HXStr("Mode"), HX::HXPinType::Int,
												HX::HXPinDirection::Input, HXStr("0"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Result"), HX::HXPinType::Color,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Grayscale")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("gray")),
								 HXStr("Grayscale"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 160, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Color"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("1,1,1"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Luminance"), HX::HXPinType::Float,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else if (typeName == HXStr("Normal Map")) {
			newNode = HX::HXNode{ MakeUniqueNodeId(HXStr("norm")),
								 HXStr("Normal Map"),
								 HXStr(""),
								 HX::HXNodeCategory::Material,
								 pos,
								 { 160, 80 },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("in")), HXStr("Color"), HX::HXPinType::Color,
												HX::HXPinDirection::Input, HXStr("0.5,0.5,1"), false } },
								 { HX::HXNodePin{ MakeUniqueNodeId(HXStr("out")), HXStr("Normal"), HX::HXPinType::Vector,
												HX::HXPinDirection::Output, HXStr(""), false } },
								 {},
								 false };
		}
		else {
			// Try custom templates
			for (const auto& tmpl : GetBlueprintRegistry().Templates) {
				if (tmpl.Title == typeName) {
					newNode.Id = MakeUniqueNodeId(tmpl.TypeName);
					newNode.Title = tmpl.Title;
					newNode.TypeName = tmpl.TypeName;
					newNode.Icon = tmpl.Icon;
					newNode.Category = tmpl.NodeCategory;
					newNode.Position = pos;
					newNode.Size = tmpl.DefaultSize;
					newNode.Inputs = tmpl.Inputs;
					newNode.Outputs = tmpl.Outputs;
					newNode.Values = tmpl.Values;
					// Regenerate pin IDs to avoid collisions
					for (auto& pin : newNode.Inputs)
						pin.Id = MakeUniqueNodeId(pin.Id);
					for (auto& pin : newNode.Outputs)
						pin.Id = MakeUniqueNodeId(pin.Id);
					break;
				}
			}
		}
		if (!newNode.Id.empty()) {
			if (typeName == HXStr("Greater Than"))
				newNode.TypeName = HXStr("Greater");
			else if (typeName == HXStr("Less Than"))
				newNode.TypeName = HXStr("Less");
			else if (typeName == HXStr("AND"))
				newNode.TypeName = HXStr("And");
			else if (typeName == HXStr("OR"))
				newNode.TypeName = HXStr("Or");
			else if (typeName == HXStr("NOT"))
				newNode.TypeName = HXStr("Not");
			else if (typeName == HXStr("Float"))
				newNode.TypeName = HXStr("ConstantFloat");
			else if (typeName == HXStr("Int"))
				newNode.TypeName = HXStr("ConstantInt");
			else if (typeName == HXStr("Bool"))
				newNode.TypeName = HXStr("ConstantBool");
			else if (typeName == HXStr("String"))
				newNode.TypeName = HXStr("ConstantString");
			else if (typeName == HXStr("Color"))
				newNode.TypeName = HXStr("ConstantColor");
			else if (typeName == HXStr("Float to Int"))
				newNode.TypeName = HXStr("FloatToInt");
			else if (typeName == HXStr("Int to Float"))
				newNode.TypeName = HXStr("IntToFloat");
			else if (typeName == HXStr("Float to String"))
				newNode.TypeName = HXStr("FloatToString");
			else if (typeName == HXStr("Int to String"))
				newNode.TypeName = HXStr("IntToString");
			else if (typeName == HXStr("Bool to String"))
				newNode.TypeName = HXStr("BoolToString");
			else if (typeName == HXStr("Color to String"))
				newNode.TypeName = HXStr("ColorToString");
			else if (typeName == HXStr("Vector to String"))
				newNode.TypeName = HXStr("VectorToString");
			else if (typeName == HXStr("String to Float"))
				newNode.TypeName = HXStr("StringToFloat");
			else if (typeName == HXStr("String to Int"))
				newNode.TypeName = HXStr("StringToInt");
			else if (typeName == HXStr("String Append"))
				newNode.TypeName = HXStr("StringAppend");
			else if (typeName == HXStr("Variable"))
				newNode.TypeName = HXStr("ConstantString");
			else if (typeName == HXStr("Get Variable"))
				newNode.TypeName = HXStr("GetVariable");
			else if (typeName == HXStr("Set Variable"))
				newNode.TypeName = HXStr("SetVariable");
			else
				newNode.TypeName = typeName;
		}
		return newNode;
	}

	bool BeginBlueprint(const HXString& id, BlueprintProfile& profile) {
		(void)id;
		if (RoughVisibility())
			return false;
		if (!profile.Graph)
			return false;

		auto& context = GetContext();
		auto& theme = GetTheme();
		auto	position = CurrentLayoutPosition();
		auto* layout = CurrentLayout();

		HXPoint size = profile.Size;
		if (size.X <= 0)
			size.X = layout->AvailableRegion.Right - position.X;
		if (size.Y <= 0)
			size.Y = layout->AvailableRegion.Bottom - position.Y;
		if (AccVisibility(size))
			return false;

		HXRect canvasRect{ position.X, position.Y, position.X + size.X, position.Y + size.Y };
		profile._internal.CanvasRect = canvasRect;
		auto painter = CurrentPainter();

		// Validate pointers
		if (!IsPointerValid(profile.Graph, profile._internal.HoverNode))
			profile._internal.HoverNode = nullptr;
		if (!IsPointerValid(profile.Graph, profile._internal.DraggingNode))
			profile._internal.DraggingNode = nullptr;
		profile._internal.HoverPin = nullptr;

		// ---- Reset per-frame pin hover ----
		for (auto& node : profile.Graph->Nodes) {
			for (auto& pin : node.Inputs)
				pin.Hovered = false;
			for (auto& pin : node.Outputs)
				pin.Hovered = false;
		}

		// ---- Inline editing: reusable single-line editor ----
		if (profile._internal.Editing) {
			auto* editNode = profile.Graph->FindNode(profile._internal.EditingNodeId);
			HXRect editRect{ 0, 0, 0, 0 };
			if (editNode) {
				HXRect sr = WorldToScreenRect(editNode->Position, editNode->Size, profile.Zoom, profile.Offset);
				sr.Left += canvasRect.Left;
				sr.Right += canvasRect.Left;
				sr.Top += canvasRect.Top;
				sr.Bottom += canvasRect.Top;
				HXGInt titleH = std::max(static_cast<HXGInt>(32 * profile.Zoom), 24);
				HXGInt pinSpacing = std::max(static_cast<HXGInt>(26 * profile.Zoom), 20);
				if (profile._internal.EditingCommentTitle) {
					HXGInt commentFont = std::max(static_cast<HXGInt>(14 * profile.Zoom), 10);
					editRect = { sr.Left + 6, sr.Top + 2, sr.Right - 4, sr.Top + 2 + commentFont + 6 };
				}
				else if (profile._internal.EditingIsPin) {
					int idx = profile._internal.EditingPinIdx;
					if (idx >= 0 && idx < static_cast<int>(editNode->Inputs.size())) {
						HXGInt py = sr.Top + titleH + 10 + idx * pinSpacing + pinSpacing / 2;
						HXGInt pinR = std::max(static_cast<HXGInt>(5 * profile.Zoom), 4);
						editRect = { sr.Left + pinR + 14 + 70, py - 8, sr.Right - 20, py + 10 };
					}
				}
				else {
					int idx = profile._internal.EditingValueIdx;
					if (idx >= 0 && idx < static_cast<int>(editNode->Values.size())) {
						HXGInt valueFont = std::max(static_cast<HXGInt>(14 * profile.Zoom), 10);
						HXGInt valueY =
							sr.Top + titleH + 10 +
							static_cast<HXGInt>(std::max(editNode->Inputs.size(), editNode->Outputs.size())) * pinSpacing +
							10;
						valueY += idx * (valueFont + 4);
						editRect = { sr.Left + 14, valueY - 2, sr.Right - 10, valueY + valueFont + 4 };
					}
				}
			}

			HXSingleLineEditor editor;
			editor.State = &profile._internal.EditState;
			editor.Bounds = editRect;
			editor.Active = true;
			editor.Style.FontSize = static_cast<int>(std::max(static_cast<HXGInt>(15 * profile.Zoom), 12));
			SingleLineEditorProcessMessages(editor, painter);
			SingleLineEditorUpdateIME(editor, painter);

			// ---- Numeric input filtering ----
			if (profile._internal.Editing && !profile._internal.EditState.InIMEComposition) {
				auto* editNode2 = profile.Graph->FindNode(profile._internal.EditingNodeId);
				if (editNode2) {
					HXPinType editType = HXPinType::String;
					if (profile._internal.EditingIsPin) {
						if (profile._internal.EditingPinIdx >= 0 &&
							profile._internal.EditingPinIdx < static_cast<int>(editNode2->Inputs.size()))
							editType = editNode2->Inputs[profile._internal.EditingPinIdx].Type;
					}
					else {
						if (profile._internal.EditingValueIdx >= 0 &&
							profile._internal.EditingValueIdx < static_cast<int>(editNode2->Values.size()))
							editType = editNode2->Values[profile._internal.EditingValueIdx].Type;
					}
					if (editType == HXPinType::Float || editType == HXPinType::Int) {
						auto& text = profile._internal.EditState.Text;
						HXString filtered;
						bool	 hasDot = false;
						for (size_t i = 0; i < text.size(); ++i) {
							wchar_t c = text[i];
							if (c == L'-' && i == 0) {
								filtered += c;
							}
							else if (c == L'.' && !hasDot && editType == HXPinType::Float) {
								filtered += c;
								hasDot = true;
							}
							else if (c >= L'0' && c <= L'9') {
								filtered += c;
							}
						}
						if (filtered != text) {
							int oldCursor = profile._internal.EditState.CursorPos;
							text = filtered;
							profile._internal.EditState.CursorPos = std::min(oldCursor, static_cast<int>(text.size()));
							profile._internal.EditState.SelectStart = profile._internal.EditState.CursorPos;
							profile._internal.EditState.SelectEnd = profile._internal.EditState.CursorPos;
							profile._internal.EditState.AnchorPos = profile._internal.EditState.CursorPos;
						}
					}
				}
			}

			// ---- Commit / cancel via Return / Escape ----
			for (auto& msg : context.MessageQuery) {
				if (msg.Processed)
					continue;
				if (!msg.KeyPressed)
					continue;
				if (msg.KeyCode == VK_RETURN) {
					msg.Processed = true;
					if (editNode) {
						if (profile._internal.EditingCommentTitle) {
							editNode->Title = profile._internal.EditState.Text;
						}
						else if (profile._internal.EditingIsPin && profile._internal.EditingPinIdx >= 0 &&
							profile._internal.EditingPinIdx < static_cast<int>(editNode->Inputs.size())) {
							editNode->Inputs[profile._internal.EditingPinIdx].DefaultValue =
								profile._internal.EditState.Text;
						}
						else if (!profile._internal.EditingIsPin && profile._internal.EditingValueIdx >= 0 &&
							profile._internal.EditingValueIdx < static_cast<int>(editNode->Values.size())) {
							if (profile._internal.EditingColor) {
								HXColor c =
									HSVToRGB(profile._internal.ColorH, profile._internal.ColorS, profile._internal.ColorV);
								editNode->Values[profile._internal.EditingValueIdx].Value = ColorToString(c);
							}
							else {
								editNode->Values[profile._internal.EditingValueIdx].Value =
									profile._internal.EditState.Text;
							}
						}
					}
					profile._internal.Editing = false;
					profile._internal.EditingCommentTitle = false;
					profile._internal.EditingColor = false;
				}
				else if (msg.KeyCode == VK_ESCAPE) {
					msg.Processed = true;
					profile._internal.Editing = false;
					profile._internal.EditingCommentTitle = false;
					profile._internal.EditingColor = false;
				}
			}

			// ---- Click outside to commit ----
			if (profile._internal.Editing) {
				for (auto& msg : context.MessageQuery) {
					if (msg.Processed)
						continue;
					if (!msg.MouseAction)
						continue;
					auto ml = ClipCoord({ msg.MouseX, msg.MouseY });
					if (!editRect.PointInside(ml) &&
						!(profile._internal.EditingColor && profile._internal._pickerRect.PointInside(ml))) {
						if (msg.MouseLeftPressed) {
							msg.Processed = true;
							if (editNode) {
								if (profile._internal.EditingIsPin && profile._internal.EditingPinIdx >= 0 &&
									profile._internal.EditingPinIdx < static_cast<int>(editNode->Inputs.size())) {
									editNode->Inputs[profile._internal.EditingPinIdx].DefaultValue =
										profile._internal.EditState.Text;
								}
								else if (!profile._internal.EditingIsPin && profile._internal.EditingValueIdx >= 0 &&
									profile._internal.EditingValueIdx < static_cast<int>(editNode->Values.size())) {
									if (profile._internal.EditingColor) {
										HXColor c = HSVToRGB(profile._internal.ColorH, profile._internal.ColorS,
											profile._internal.ColorV);
										editNode->Values[profile._internal.EditingValueIdx].Value = ColorToString(c);
									}
									else {
										editNode->Values[profile._internal.EditingValueIdx].Value =
											profile._internal.EditState.Text;
									}
								}
							}
							profile._internal.Editing = false;
							profile._internal.EditingCommentTitle = false;
							profile._internal.EditingColor = false;
						}
					}
				}
			}

			// ---- Color picker drag logic ----
			if (profile._internal.EditingColor) {
				for (auto& msg : context.MessageQuery) {
					if (msg.Processed)
						continue;
					if (!msg.MouseAction)
						continue;
					auto ml = ClipCoord({ msg.MouseX, msg.MouseY });
					if (profile._internal._pickerRect.PointInside(ml)) {
						msg.Processed = true;
						if (msg.MouseLeftPressed) {
							profile._internal.ColorPickerDragging = true;
						}
						if (msg.MouseLeftRelease) {
							profile._internal.ColorPickerDragging = false;
						}
						if (profile._internal.ColorPickerDragging || msg.MouseLeftPressed) {
							if (profile._internal._sqRect.PointInside(ml)) {
								profile._internal.ColorS =
									static_cast<float>(ml.X - profile._internal._sqRect.Left) / 140.0f;
								profile._internal.ColorV =
									1.0f - static_cast<float>(ml.Y - profile._internal._sqRect.Top) / 140.0f;
								profile._internal.ColorS = std::clamp(profile._internal.ColorS, 0.0f, 1.0f);
								profile._internal.ColorV = std::clamp(profile._internal.ColorV, 0.0f, 1.0f);
							}
							if (profile._internal._hueRect.PointInside(ml)) {
								profile._internal.ColorH =
									static_cast<float>(ml.X - profile._internal._hueRect.Left) / 140.0f * 360.0f;
								profile._internal.ColorH = std::clamp(profile._internal.ColorH, 0.0f, 360.0f);
							}
						}
					}
					else {
						if (msg.MouseLeftRelease) {
							profile._internal.ColorPickerDragging = false;
						}
					}
				}
			}
		}
		// ---- Keyboard: Delete / Copy / Paste ----
		if (!profile._internal.Editing) {
			for (auto& msg : context.MessageQuery) {
				if (msg.Processed)
					continue;
				if (msg.KeyPressed && msg.KeyCode == VK_DELETE && !profile._internal.SelectedNodes.empty()) {
					for (const auto& nodeId : profile._internal.SelectedNodes)
						profile.Graph->DeleteNode(nodeId);
					profile._internal.SelectedNodes.clear();
					profile._internal.DraggingNode = nullptr;
					profile._internal.HoverNode = nullptr;
					msg.Processed = true;
				}
				if (msg.KeyPressed && msg.KeyCode == 'C' && msg.InCtrl && !profile._internal.SelectedNodes.empty()) {
					CopyNodesToClipboard(profile._internal.SelectedNodes, profile.Graph);
					msg.Processed = true;
				}
				if (msg.KeyPressed && msg.KeyCode == 'V' && msg.InCtrl && !G_BlueprintClipboard.empty()) {
					HXPoint pasteWorld = ScreenToWorld(
						{ canvasRect.Left + canvasRect.CalWidth() / 2, canvasRect.Top + canvasRect.CalHeight() / 2 },
						profile.Zoom, profile.Offset);
					for (auto& m : context.MessageQuery) {
						if (m.MouseAction) {
							auto ml = ClipCoord({ m.MouseX, m.MouseY });
							if (canvasRect.PointInside(ml))
								pasteWorld = ScreenToWorld({ ml.X - canvasRect.Left, ml.Y - canvasRect.Top }, profile.Zoom,
									profile.Offset);
							break;
						}
					}
					std::vector<HXString> pastedIds;
					PasteNodesFromClipboard(profile.Graph, pasteWorld, &pastedIds);
					profile._internal.SelectedNodes = std::move(pastedIds);
					msg.Processed = true;
				}
			}
		}

		// ---- Input handling ----
		for (auto& msg : context.MessageQuery) {
			if (msg.Processed)
				continue;
			if (!msg.MouseAction && !msg.MouseWheel)
				continue;

			HXPoint mouseLocal = ClipCoord({ msg.MouseX, msg.MouseY });
			bool	inside = canvasRect.PointInside(mouseLocal);

			// Context menu interaction
			if (profile._internal.ContextMenuOpen) {
				if (msg.MouseRightPressed || msg.MouseRightRelease) {
					profile._internal.ContextMenuOpen = false;
					msg.Processed = true;
					continue;
				}
				if (msg.MouseLeftPressed || msg.MouseLeftRelease) {
					// Check if click is on a menu item
					bool clickedItem = false;
					int	 hc = profile._internal.ContextMenuHoverCategory;
					int	 hi = profile._internal.ContextMenuHoverItem;
					if (hc >= 0 && hc < static_cast<int>(G_BPMenuCategories.size()) && hi >= 0 &&
						hi < static_cast<int>(G_BPMenuCategories[hc].second.size())) {
						// Create node
						HXPoint	 worldPos = profile._internal.ContextMenuWorldPos;
						HXString typeName = G_BPMenuCategories[hc].second[hi];
						HXNode	 newNode = MakeBlueprintNode(typeName, worldPos, &profile);
						if (!newNode.Id.empty()) {
							profile.Graph->Nodes.push_back(newNode);
						}
						clickedItem = true;
					}
					profile._internal.ContextMenuOpen = false;
					msg.Processed = true;
					continue;
				}
				// Hover detection for menu
				if (msg.MouseAction) {
					HXPoint			 m = mouseLocal;
					constexpr HXGInt catW = 120, rowH = 26, itemW = 140;
					profile._internal.ContextMenuHoverItem = -1;
					HXPoint pos = profile._internal.ContextMenuScreenPos;

					// Calculate menu size (same as DrawContextMenu)
					HXGInt	totalH = 0;
					for (const auto& cat : G_BPMenuCategories) {
						totalH = std::max(totalH, static_cast<HXGInt>(cat.second.size() * rowH + 8));
					}
					totalH = std::max(totalH, static_cast<HXGInt>(G_BPMenuCategories.size() * rowH + 8));

					// Clamp menu rect to canvas (must match DrawContextMenu exactly)
					HXRect menuRect{ pos.X, pos.Y, pos.X + catW + itemW, pos.Y + totalH };
					if (menuRect.Right > canvasRect.Right)
						menuRect.Left -= (menuRect.Right - canvasRect.Right);
					if (menuRect.Bottom > canvasRect.Bottom)
						menuRect.Top -= (menuRect.Bottom - canvasRect.Bottom);
					if (menuRect.Left < canvasRect.Left)
						menuRect.Left = canvasRect.Left;
					if (menuRect.Top < canvasRect.Top)
						menuRect.Top = canvasRect.Top;
					pos.X = menuRect.Left;
					pos.Y = menuRect.Top;

					HXGInt my = m.Y - pos.Y - 4;
					// Category column
					if (m.X >= pos.X && m.X < pos.X + catW && my >= 0) {
						int catIdx = my / rowH;
						if (catIdx >= 0 && catIdx < static_cast<int>(G_BPMenuCategories.size()))
							profile._internal.ContextMenuHoverCategory = catIdx;
					}
					// Item column (keep current category; only update item)
					if (profile._internal.ContextMenuHoverCategory >= 0) {
						if (m.X >= pos.X + catW && m.X < pos.X + catW + itemW && my >= 0) {
							int itemIdx = my / rowH;
							if (itemIdx >= 0 &&
								itemIdx < static_cast<int>(
									G_BPMenuCategories[profile._internal.ContextMenuHoverCategory].second.size()))
								profile._internal.ContextMenuHoverItem = itemIdx;
						}
					}
					msg.Processed = true;
				}
				continue;
			}

			// Panning and zoom
			if (inside || profile._internal.IsPanning) {
				HXPoint mouseViewport{ mouseLocal.X - canvasRect.Left, mouseLocal.Y - canvasRect.Top };
				if (msg.MouseRightPressed) {
					profile._internal.IsPanning = true;
					profile._internal.PanStartMouse = mouseViewport;
					profile._internal.PanStartOffset = profile.Offset;
					// Abort any ongoing node drag / box-select so they don't persist
					profile._internal.DraggingNode = nullptr;
					profile._internal.DraggingPin = nullptr;
					profile._internal.SelectionBox = {};
					msg.Processed = true;
				}
				if (msg.MouseRightRelease) {
					float dx = static_cast<float>(mouseViewport.X - profile._internal.PanStartMouse.X);
					float dy = static_cast<float>(mouseViewport.Y - profile._internal.PanStartMouse.Y);
					if (dx * dx + dy * dy < 25.0f && inside) {
						// Right-click without drag -> check if clicked on a link
						bool		deletedLink = false;
						const float hitThreshold = 8.0f; // pixels
						for (auto it = profile.Graph->Links.begin(); it != profile.Graph->Links.end();) {
							auto* startPin = profile.Graph->FindPin(it->StartPinId);
							auto* endPin = profile.Graph->FindPin(it->EndPinId);
							auto* startNode = profile.Graph->FindNodeContainingPin(it->StartPinId);
							auto* endNode = profile.Graph->FindNodeContainingPin(it->EndPinId);
							if (!startPin || !endPin || !startNode || !endNode) {
								++it;
								continue;
							}
							// Compute screen positions for link endpoints
							HXRect nsrS =
								WorldToScreenRect(startNode->Position, startNode->Size, profile.Zoom, profile.Offset);
							HXRect nsrE = WorldToScreenRect(endNode->Position, endNode->Size, profile.Zoom, profile.Offset);
							nsrS.Left += canvasRect.Left;
							nsrS.Right += canvasRect.Left;
							nsrS.Top += canvasRect.Top;
							nsrS.Bottom += canvasRect.Top;
							nsrE.Left += canvasRect.Left;
							nsrE.Right += canvasRect.Left;
							nsrE.Top += canvasRect.Top;
							nsrE.Bottom += canvasRect.Top;
							HXGInt	titleHS = std::max(static_cast<HXGInt>(32 * profile.Zoom), 24);
							HXGInt	pinSpacingS = std::max(static_cast<HXGInt>(26 * profile.Zoom), 20);
							HXGInt	pinRadiusS = std::max(static_cast<HXGInt>(5 * profile.Zoom), 4);
							HXGInt	bodyTopS = nsrS.Top + titleHS + 10;
							HXPoint startScr{ 0, 0 }, endScr{ 0, 0 };
							for (int i = 0; i < static_cast<int>(startNode->Outputs.size()); ++i) {
								if (&startNode->Outputs[i] == startPin) {
									startScr = { nsrS.Right - 14 - pinRadiusS, bodyTopS + i * pinSpacingS + pinSpacingS / 2 };
									break;
								}
							}
							HXGInt titleHE = std::max(static_cast<HXGInt>(32 * profile.Zoom), 24);
							HXGInt pinSpacingE = std::max(static_cast<HXGInt>(26 * profile.Zoom), 20);
							HXGInt pinRadiusE = std::max(static_cast<HXGInt>(5 * profile.Zoom), 4);
							HXGInt bodyTopE = nsrE.Top + titleHE + 10;
							for (int i = 0; i < static_cast<int>(endNode->Inputs.size()); ++i) {
								if (&endNode->Inputs[i] == endPin) {
									endScr = { nsrE.Left + 14 + pinRadiusE, bodyTopE + i * pinSpacingE + pinSpacingE / 2 };
									break;
								}
							}
							float dist = DistancePointToBezier(mouseLocal, startScr, endScr, profile.Zoom);
							if (dist < hitThreshold) {
								it = profile.Graph->Links.erase(it);
								deletedLink = true;
								break;
							}
							else {
								++it;
							}
						}
						if (!deletedLink) {
							// Right-click on empty canvas -> open context menu
							profile._internal.ContextMenuOpen = true;
							profile._internal.ContextMenuScreenPos = { mouseLocal.X, mouseLocal.Y };
							profile._internal.ContextMenuWorldPos =
								ScreenToWorld(mouseViewport, profile.Zoom, profile.Offset);
							profile._internal.ContextMenuHoverCategory = 0;
							profile._internal.ContextMenuHoverItem = -1;
						}
					}
					profile._internal.IsPanning = false;
					profile._internal.DraggingNode = nullptr;
					profile._internal.DraggingPin = nullptr;
					msg.Processed = true;
				}
				if (msg.MouseAction && profile._internal.IsPanning) {
					msg.Processed = true;
					profile.Offset.X = profile._internal.PanStartOffset.X -
						(mouseViewport.X - profile._internal.PanStartMouse.X) / profile.Zoom;
					profile.Offset.Y = profile._internal.PanStartOffset.Y -
						(mouseViewport.Y - profile._internal.PanStartMouse.Y) / profile.Zoom;
				}
				if (msg.MouseWheel) {
					msg.Processed = true;
					float	factor = msg.MouseWheelUp ? 0.87f : 1.15f;
					float	newZoom = std::clamp(profile.Zoom * factor, 0.3f, 3.0f);
					HXPoint mouseWorldBefore = ScreenToWorld(mouseViewport, profile.Zoom, profile.Offset);
					profile.Zoom = newZoom;
					HXPoint mouseWorldAfter = ScreenToWorld(mouseViewport, profile.Zoom, profile.Offset);
					profile.Offset.X += (mouseWorldBefore.X - mouseWorldAfter.X);
					profile.Offset.Y += (mouseWorldBefore.Y - mouseWorldAfter.Y);
				}
			}

			if (!inside)
				continue;

			HXPoint mouseViewport{ mouseLocal.X - canvasRect.Left, mouseLocal.Y - canvasRect.Top };
			HXPoint mouseWorld = ScreenToWorld(mouseViewport, profile.Zoom, profile.Offset);

			// Hit-test hover (only when not actively dragging)
			if (!profile._internal.DraggingNode && !profile._internal.DraggingPin &&
				profile._internal.SelectionBox.Left == profile._internal.SelectionBox.Right) {
				auto pinHit = HitTestPin(profile.Graph, mouseWorld, profile.Zoom);
				if (pinHit.Pin) {
					profile._internal.HoverPin = pinHit.Pin;
					profile._internal.HoverNode = pinHit.Node;
					pinHit.Pin->Hovered = true;
				}
				else {
					profile._internal.HoverNode = HitTestNode(profile.Graph, mouseWorld);
				}
			}

			// Left press
			if (msg.MouseLeftPressed) {
				// Check inline edit click-outside
				if (profile._internal.Editing) {
					// Clicking inside color picker popup should not exit editing
					if (profile._internal.EditingColor && profile._internal._pickerRect.PointInside(mouseLocal)) {
						msg.Processed = true;
						continue;
					}
					auto* editNode = profile.Graph->FindNode(profile._internal.EditingNodeId);
					if (editNode) {
						bool saved = false;
						if (profile._internal.EditingIsPin && profile._internal.EditingPinIdx >= 0 &&
							profile._internal.EditingPinIdx < static_cast<int>(editNode->Inputs.size())) {
							editNode->Inputs[profile._internal.EditingPinIdx].DefaultValue =
								profile._internal.EditState.Text;
							saved = true;
						}
						else if (!profile._internal.EditingIsPin && profile._internal.EditingValueIdx >= 0 &&
							profile._internal.EditingValueIdx < static_cast<int>(editNode->Values.size())) {
							if (profile._internal.EditingColor) {
								HXColor c =
									HSVToRGB(profile._internal.ColorH, profile._internal.ColorS, profile._internal.ColorV);
								editNode->Values[profile._internal.EditingValueIdx].Value = ColorToString(c);
							}
							else {
								editNode->Values[profile._internal.EditingValueIdx].Value =
									profile._internal.EditState.Text;
							}
							saved = true;
						}
					}
					profile._internal.Editing = false;
					profile._internal.EditingColor = false;
				}

				auto pinHit = HitTestPin(profile.Graph, mouseWorld, profile.Zoom);
				if (pinHit.Pin) {
					profile._internal.DraggingPin = pinHit.Pin;
					profile._internal.DraggingNode = pinHit.Node;
					// If pin already has a link, remove ONLY that specific link
					// Output pin -> remove its outgoing link; Input pin -> remove its incoming link
					auto& links = profile.Graph->Links;
					for (auto it = links.begin(); it != links.end();) {
						bool isMyLink = false;
						if (pinHit.Pin->Direction == HXPinDirection::Output) {
							isMyLink = (it->StartPinId == pinHit.Pin->Id);
						}
						else {
							isMyLink = (it->EndPinId == pinHit.Pin->Id);
						}
						if (isMyLink) {
							it = links.erase(it);
							break; // Only delete ONE link, not all
						}
						else {
							++it;
						}
					}
					msg.Processed = true;
					continue;
				}

				auto* hitNode = HitTestNode(profile.Graph, mouseWorld);
				if (hitNode) {
					// Double-click Comment node to edit its title
					if (hitNode->Category == HXNodeCategory::Comment) {
						auto now = clock();
						bool isDoubleClick = (now - profile._internal.LastNodeClickTime < 500) &&
							(profile._internal.LastNodeClickId == hitNode->Id) &&
							(std::abs(mouseWorld.X - profile._internal.LastNodeClickPos.X) < 10) &&
							(std::abs(mouseWorld.Y - profile._internal.LastNodeClickPos.Y) < 10);
						profile._internal.LastNodeClickTime = now;
						profile._internal.LastNodeClickPos = mouseWorld;
						profile._internal.LastNodeClickId = hitNode->Id;
						if (isDoubleClick) {
							profile._internal.Editing = true;
							profile._internal.EditingCommentTitle = true;
							profile._internal.EditingNodeId = hitNode->Id;
							profile._internal.EditState.Text = hitNode->Title;
							profile._internal.EditState.CursorPos = static_cast<int>(hitNode->Title.size());
							profile._internal.EditState.SelectStart = profile._internal.EditState.CursorPos;
							profile._internal.EditState.SelectEnd = profile._internal.EditState.CursorPos;
							profile._internal.EditState.AnchorPos = profile._internal.EditState.CursorPos;
							profile._internal.EditState.BlinkClock = clock();
							msg.Processed = true;
							continue;
						}
					}

					// Only normal nodes support inline editing
					if (hitNode->Category != HXNodeCategory::Comment) {
						bool	startedEdit = false;
						HXPoint sz = CalculateNodeSize(*hitNode, 1.0f);
						// Check node values
						if (!hitNode->Values.empty()) {

							HXGInt titleH = 32;
							HXGInt pinSpacing = 26;
							HXGInt valueY = hitNode->Position.Y + titleH + 10 +
								static_cast<HXGInt>(std::max(hitNode->Inputs.size(), hitNode->Outputs.size())) *
								pinSpacing +
								10;
							for (int i = 0; i < static_cast<int>(hitNode->Values.size()); ++i) {
								HXRect vrect{ hitNode->Position.X + 14, valueY - 2, hitNode->Position.X + sz.X - 14,
											 valueY + 16 };
								if (vrect.PointInside(mouseWorld)) {
									profile._internal.Editing = true;
									profile._internal.EditingIsPin = false;
									profile._internal.EditingNodeId = hitNode->Id;
									profile._internal.EditingValueIdx = i;
									profile._internal.EditState.Text = hitNode->Values[i].Value;
									profile._internal.EditState.CursorPos =
										static_cast<int>(profile._internal.EditState.Text.size());
									profile._internal.EditState.SelectStart = profile._internal.EditState.CursorPos;
									profile._internal.EditState.SelectEnd = profile._internal.EditState.CursorPos;
									profile._internal.EditState.AnchorPos = profile._internal.EditState.CursorPos;
									profile._internal.EditState.BlinkClock = clock();
									if (hitNode->Values[i].Type == HXPinType::Color) {
										profile._internal.EditingColor = true;
										HXColor c = ParseColorString(profile._internal.EditState.Text);
										RGBToHSV(c, profile._internal.ColorH, profile._internal.ColorS,
											profile._internal.ColorV);
									}
									else {
										profile._internal.EditingColor = false;
									}
									startedEdit = true;
									msg.Processed = true;
									break;
								}
								valueY += 16;
							}
						}
						if (startedEdit)
							continue;
					}

					// Selection logic for ALL node types (including Comment)
					if (msg.InCtrl || msg.InShift) {
						auto it = std::find(profile._internal.SelectedNodes.begin(), profile._internal.SelectedNodes.end(),
							hitNode->Id);
						if (it != profile._internal.SelectedNodes.end())
							profile._internal.SelectedNodes.erase(it);
						else
							profile._internal.SelectedNodes.push_back(hitNode->Id);
					}
					else {
						if (!IsNodeSelected(profile, hitNode->Id)) {
							profile._internal.SelectedNodes.clear();
							profile._internal.SelectedNodes.push_back(hitNode->Id);
						}
					}
					profile._internal.DraggingNode = hitNode;
					profile._internal.DragStartMouseWorld = mouseWorld;
					profile._internal.DragStartNodePositions.clear();
					for (const auto& sid : profile._internal.SelectedNodes) {
						auto* n = profile.Graph->FindNode(sid);
						if (n)
							profile._internal.DragStartNodePositions.push_back(n->Position);
					}
					msg.Processed = true;
					continue;
				}

				// Empty canvas -> box select
				if (!msg.InCtrl && !msg.InShift)
					profile._internal.SelectedNodes.clear();
				profile._internal.SelectionBoxStart = mouseViewport;
				profile._internal.SelectionBox = { mouseViewport.X, mouseViewport.Y, mouseViewport.X, mouseViewport.Y };
				msg.Processed = true;
				continue;
			}

			// Node drag in progress
			if (msg.MouseAction && !msg.MouseLeftRelease && !msg.MouseRightRelease && profile._internal.DraggingNode &&
				!profile._internal.DraggingPin) {
				msg.Processed = true;
				HXPoint delta{ mouseWorld.X - profile._internal.DragStartMouseWorld.X,
							  mouseWorld.Y - profile._internal.DragStartMouseWorld.Y };
				for (size_t i = 0; i < profile._internal.SelectedNodes.size(); ++i) {
					auto* n = profile.Graph->FindNode(profile._internal.SelectedNodes[i]);
					if (n && i < profile._internal.DragStartNodePositions.size()) {
						n->Position.X = profile._internal.DragStartNodePositions[i].X + delta.X;
						n->Position.Y = profile._internal.DragStartNodePositions[i].Y + delta.Y;
					}
				}
				continue;
			}

			// Box select in progress
			if (msg.MouseAction && !msg.MouseLeftRelease && !msg.MouseRightRelease &&
				profile._internal.SelectionBox.Left != profile._internal.SelectionBox.Right) {
				msg.Processed = true;
				profile._internal.SelectionBox = { std::min(profile._internal.SelectionBoxStart.X, mouseViewport.X),
												  std::min(profile._internal.SelectionBoxStart.Y, mouseViewport.Y),
												  std::max(profile._internal.SelectionBoxStart.X, mouseViewport.X),
												  std::max(profile._internal.SelectionBoxStart.Y, mouseViewport.Y) };
				continue;
			}
		}

		// Finalize box select on release
		for (auto& msg : context.MessageQuery) {
			if (msg.Processed)
				continue;
			if (msg.MouseLeftRelease && profile._internal.SelectionBox.Left != profile._internal.SelectionBox.Right) {
				float  invZ = 1.0f / std::max(profile.Zoom, 0.001f);
				HXRect boxWorld{
					static_cast<HXGInt>(
						std::min(profile._internal.SelectionBox.Left, profile._internal.SelectionBox.Right) * invZ +
						profile.Offset.X),
					static_cast<HXGInt>(
						std::min(profile._internal.SelectionBox.Top, profile._internal.SelectionBox.Bottom) * invZ +
						profile.Offset.Y),
					static_cast<HXGInt>(
						std::max(profile._internal.SelectionBox.Left, profile._internal.SelectionBox.Right) * invZ +
						profile.Offset.X),
					static_cast<HXGInt>(
						std::max(profile._internal.SelectionBox.Top, profile._internal.SelectionBox.Bottom) * invZ +
						profile.Offset.Y) };
				for (auto& node : profile.Graph->Nodes) {
					HXPoint sz = CalculateNodeSize(node, 1.0f);
					if (node.Position.X >= boxWorld.Left && node.Position.X + sz.X <= boxWorld.Right &&
						node.Position.Y >= boxWorld.Top && node.Position.Y + sz.Y <= boxWorld.Bottom) {
						if (!IsNodeSelected(profile, node.Id))
							profile._internal.SelectedNodes.push_back(node.Id);
					}
				}
				profile._internal.SelectionBox = {};
				msg.Processed = true;
			}
			if (msg.MouseLeftRelease && profile._internal.DraggingPin) {
				HXPoint mouseLocal = ClipCoord({ msg.MouseX, msg.MouseY });
				HXPoint mouseViewport{ mouseLocal.X - canvasRect.Left, mouseLocal.Y - canvasRect.Top };
				HXPoint mouseWorld = ScreenToWorld(mouseViewport, profile.Zoom, profile.Offset);
				auto	hit = HitTestPin(profile.Graph, mouseWorld, profile.Zoom);
				if (hit.Pin && CanConnect(profile._internal.DraggingPin, hit.Pin)) {
					profile.Graph->Links.push_back({ profile._internal.DraggingPin->Id, hit.Pin->Id });
				}
				profile._internal.DraggingPin = nullptr;
				profile._internal.DraggingNode = nullptr;
				msg.Processed = true;
			}
			if (msg.MouseLeftRelease && profile._internal.DraggingNode) {
				profile._internal.DraggingNode = nullptr;
				msg.Processed = true;
			}
		}

		// ---- Drawing ----
		painter->Begin();
		painter->DrawFilledRectangle(canvasRect, profile.CanvasBackgroundColor, profile.CanvasBackgroundColor);
		DrawGrid(painter, canvasRect, profile.Zoom, profile.Offset, profile.CanvasGridColor);

		// Links (draw behind nodes)
		for (const auto& link : profile.Graph->Links) {
			auto* startPin = profile.Graph->FindPin(link.StartPinId);
			auto* endPin = profile.Graph->FindPin(link.EndPinId);
			if (!startPin || !endPin)
				continue;
			auto* startNode = profile.Graph->FindNodeContainingPin(link.StartPinId);
			auto* endNode = profile.Graph->FindNodeContainingPin(link.EndPinId);
			if (!startNode || !endNode)
				continue;
			// Use cached ScreenCenter from last frame; recalc if needed
			HXPoint startScr = startPin->ScreenCenter;
			HXPoint endScr = endPin->ScreenCenter;
			if (startScr.X == 0 && startScr.Y == 0) {
				// Fallback: recalc
				HXRect nsr = WorldToScreenRect(startNode->Position, startNode->Size, profile.Zoom, profile.Offset);
				nsr.Left += canvasRect.Left;
				nsr.Right += canvasRect.Left;
				nsr.Top += canvasRect.Top;
				nsr.Bottom += canvasRect.Top;
				HXGInt titleH = std::max(static_cast<HXGInt>(32 * profile.Zoom), 24);
				HXGInt pinSpacing = std::max(static_cast<HXGInt>(26 * profile.Zoom), 20);
				HXGInt pinRadius = std::max(static_cast<HXGInt>(5 * profile.Zoom), 4);
				HXGInt bodyTop = nsr.Top + titleH + 10;
				for (int i = 0; i < static_cast<int>(startNode->Outputs.size()); ++i) {
					if (&startNode->Outputs[i] == startPin) {
						startScr = { nsr.Right - 14 - pinRadius, bodyTop + i * pinSpacing + pinSpacing / 2 };
						break;
					}
				}
			}
			if (endScr.X == 0 && endScr.Y == 0) {
				HXRect nsr = WorldToScreenRect(endNode->Position, endNode->Size, profile.Zoom, profile.Offset);
				nsr.Left += canvasRect.Left;
				nsr.Right += canvasRect.Left;
				nsr.Top += canvasRect.Top;
				nsr.Bottom += canvasRect.Top;
				HXGInt titleH = std::max(static_cast<HXGInt>(32 * profile.Zoom), 24);
				HXGInt pinSpacing = std::max(static_cast<HXGInt>(26 * profile.Zoom), 20);
				HXGInt pinRadius = std::max(static_cast<HXGInt>(5 * profile.Zoom), 4);
				HXGInt bodyTop = nsr.Top + titleH + 10;
				for (int i = 0; i < static_cast<int>(endNode->Inputs.size()); ++i) {
					if (&endNode->Inputs[i] == endPin) {
						endScr = { nsr.Left + 14 + pinRadius, bodyTop + i * pinSpacing + pinSpacing / 2 };
						break;
					}
				}
			}
			bool	isFlow = (startPin->Type == HX::HXPinType::Flow);
			HXColor linkColor = GetPinColor(startPin->Type, &theme, &profile);
			if (isFlow) {
				linkColor = theme.TextSecondary;
				linkColor.A = static_cast<unsigned char>(linkColor.A * 0.4f);
				DrawBezierCubic(painter, startScr, endScr, linkColor, profile.Zoom, 2.0f, true);
			}
			else {
				linkColor.A = static_cast<unsigned char>(linkColor.A * 0.6f);
				DrawBezierCubic(painter, startScr, endScr, linkColor, profile.Zoom, 2.5f, false);
			}
		}

		// Active link drag
		if (profile._internal.DraggingPin && profile._internal.DraggingNode) {
			HXPoint startScr = profile._internal.DraggingPin->ScreenCenter;
			if (startScr.X == 0 && startScr.Y == 0) {
				HXRect nsr = WorldToScreenRect(profile._internal.DraggingNode->Position,
					profile._internal.DraggingNode->Size, profile.Zoom, profile.Offset);
				nsr.Left += canvasRect.Left;
				nsr.Right += canvasRect.Left;
				nsr.Top += canvasRect.Top;
				nsr.Bottom += canvasRect.Top;
				HXGInt titleH = std::max(static_cast<HXGInt>(32 * profile.Zoom), 24);
				HXGInt pinSpacing = std::max(static_cast<HXGInt>(26 * profile.Zoom), 20);
				HXGInt pinRadius = std::max(static_cast<HXGInt>(5 * profile.Zoom), 4);
				HXGInt bodyTop = nsr.Top + titleH + 10;
				for (int i = 0; i < static_cast<int>(profile._internal.DraggingNode->Outputs.size()); ++i) {
					if (&profile._internal.DraggingNode->Outputs[i] == profile._internal.DraggingPin) {
						startScr = { nsr.Right - 14 - pinRadius, bodyTop + i * pinSpacing + pinSpacing / 2 };
						break;
					}
				}
			}
			HXPoint mouseLocal = ClipCoord({ context.MessageQuery.empty() ? HXPoint{ 0, 0 }
																		 : HXPoint{ context.MessageQuery.back().MouseX,
																				   context.MessageQuery.back().MouseY } });
			if (!context.MessageQuery.empty()) {
				HXPoint endScr{ mouseLocal.X, mouseLocal.Y };
				HXColor dragColor = GetPinColor(profile._internal.DraggingPin->Type, &theme, &profile);
				if (profile._internal.DraggingPin->Type != HXPinType::Flow) {
					dragColor.A = static_cast<unsigned char>(dragColor.A * 0.8f);
				}
				DrawBezierCubic(painter, startScr, endScr, dragColor, profile.Zoom,
					profile._internal.DraggingPin->Type == HXPinType::Flow ? 2.0f : 2.5f,
					profile._internal.DraggingPin->Type == HXPinType::Flow);
			}
		}

		// Nodes
		for (auto& node : profile.Graph->Nodes) {
			bool sel = IsNodeSelected(profile, node.Id);
			bool hov = (profile._internal.HoverNode == &node);
			DrawNode(painter, node, profile.Zoom, profile.Offset, canvasRect, theme, sel, hov, &profile);
		}

		// Inline edit overlay
		DrawInlineEdit(painter, profile, canvasRect);

		// Context menu
		DrawContextMenu(painter, profile, canvasRect);

		// Selection box
		if (profile._internal.SelectionBox.Left != profile._internal.SelectionBox.Right) {
			HXRect box = profile._internal.SelectionBox;
			box.Left += canvasRect.Left;
			box.Right += canvasRect.Left;
			box.Top += canvasRect.Top;
			box.Bottom += canvasRect.Top;
			painter->DrawFilledRectangle(box, profile.SelectionBoxFillColor, profile.SelectionBoxInnerColor);
			painter->DrawRectangle(box, profile.SelectionBoxBorderColor);
		}

		painter->End();
		LayoutNextControl(size);
		return true;
	}

	void EndBlueprint() {
	}

	////////////////////////////////////////////////////////////
	/// Execution engine
	////////////////////////////////////////////////////////////

	bool HXBlueprintValue::IsTruthy() const {
		switch (Type) {
		case HXPinType::Bool:
			return AsBool;
		case HXPinType::Int:
			return AsInt != 0;
		case HXPinType::Float:
			return AsFloat != 0.0f;
		case HXPinType::String:
			return !AsString.empty();
		case HXPinType::Texture:
			return !AsTexture.empty();
		default:
			return false;
		}
	}

	HXString HXBlueprintValue::ToString() const {
		wchar_t buf[256];
		switch (Type) {
		case HXPinType::Bool:
			return AsBool ? HXStr("true") : HXStr("false");
		case HXPinType::Int:
			swprintf(buf, 256, L"%d", AsInt);
			return HXString(buf);
		case HXPinType::Float:
			swprintf(buf, 256, L"%.4f", AsFloat);
			return HXString(buf);
		case HXPinType::String:
			return AsString;
		case HXPinType::Vector:
			swprintf(buf, 256, L"(%d,%d)", AsVector.X, AsVector.Y);
			return HXString(buf);
		case HXPinType::Color:
			swprintf(buf, 256, L"(%d,%d,%d)", AsColor.R, AsColor.G, AsColor.B);
			return HXString(buf);
		case HXPinType::Texture:
			return AsTexture;
		default:
			return HXStr("0");
		}
	}

	HXBlueprintValue HXBlueprintValue::FromString(const HXString& s, HXPinType type) {
		HXBlueprintValue v;
		v.Type = type;
		switch (type) {
		case HXPinType::Bool:
			v.AsBool = (s == L"true" || s == L"1" || s == L"True");
			break;
		case HXPinType::Int:
			try {
				v.AsInt = std::stoi(s);
			}
			catch (...) {
				v.AsInt = 0;
			}
			break;
		case HXPinType::Float:
			try {
				v.AsFloat = std::stof(s);
			}
			catch (...) {
				v.AsFloat = 0.0f;
			}
			break;
		case HXPinType::String:
			v.AsString = s;
			break;
		case HXPinType::Texture:
			v.AsTexture = s;
			break;
		default:
			try {
				v.AsFloat = std::stof(s);
			}
			catch (...) {
				v.AsFloat = 0.0f;
			}
			break;
		}
		return v;
	}

	HXBlueprintValue HXBlueprintValue::FromDefault(const HXString& s, HXPinType type) {
		return FromString(s, type);
	}

	// Forward declarations
	static HXBlueprintValue		 ExecuteNodeOutput(HXNodeGraph* graph, HXNode& node, const HXString& outputPinId,
		HXBlueprintRuntime& runtime);

	static std::vector<HXNode*> FindNextFlowNodes(HXNodeGraph* graph, const HXString& outputPinId) {
		std::vector<HXNode*> result;
		for (auto& link : graph->Links) {
			if (link.StartPinId == outputPinId) {
				auto* node = graph->FindNodeContainingPin(link.EndPinId);
				if (node)
					result.push_back(node);
			}
		}
		return result;
	}

	HXBlueprintValue EvaluateBlueprintPin(HXNodeGraph* graph, const HXString& pinId, HXBlueprintRuntime& runtime) {
		auto it = runtime.PinValues.find(pinId);
		if (it != runtime.PinValues.end())
			return it->second;

		auto* pin = graph->FindPin(pinId);
		if (!pin) {
			runtime.PinValues[pinId] = HXBlueprintValue();
			return runtime.PinValues[pinId];
		}

		if (pin->Direction == HXPinDirection::Input) {
			// Follow link backwards
			HXString sourcePinId;
			for (auto& link : graph->Links) {
				if (link.EndPinId == pinId) {
					sourcePinId = link.StartPinId;
					break;
				}
			}
			if (!sourcePinId.empty()) {
				auto val = EvaluateBlueprintPin(graph, sourcePinId, runtime);
				runtime.PinValues[pinId] = val;
				return val;
			}
			// No link - use default
			auto val = HXBlueprintValue::FromDefault(pin->DefaultValue, pin->Type);
			runtime.PinValues[pinId] = val;
			return val;
		}

		// Output pin: evaluate owning node
		auto* node = graph->FindNodeContainingPin(pinId);
		if (!node) {
			runtime.PinValues[pinId] = HXBlueprintValue();
			return runtime.PinValues[pinId];
		}
		auto val = ExecuteNodeOutput(graph, *node, pinId, runtime);
		runtime.PinValues[pinId] = val;
		return val;
	}

	static HXBlueprintValue ExecuteNodeOutput(HXNodeGraph* graph, HXNode& node, const HXString& outputPinId,
		HXBlueprintRuntime& runtime) {
		auto it = runtime.PinValues.find(outputPinId);
		if (it != runtime.PinValues.end())
			return it->second;

		HXBlueprintValue result;
		const HXString& type = node.TypeName;

		// ForLoop Index output is stored in runtime variables during flow execution
		if (type == HXStr("ForLoop")) {
			for (auto& out : node.Outputs) {
				if (out.Id == outputPinId && (out.Label == HXStr("Index") || out.Label == HXStr("Loop Index"))) {
					auto it = runtime.Variables.find(HXStr("__loop_") + node.Id);
					if (it != runtime.Variables.end()) {
						result = it->second;
					}
					else {
						result.Type = HXPinType::Int;
						result.AsInt = 0;
					}
					runtime.PinValues[outputPinId] = result;
					return result;
				}
			}
		}

		auto getInput = [&](size_t idx) -> HXBlueprintValue {
			if (idx < node.Inputs.size()) {
				return EvaluateBlueprintPin(graph, node.Inputs[idx].Id, runtime);
			}
			return HXBlueprintValue();
			};

		// Custom data registry takes priority over built-in dispatch
		{
			auto& reg = GetBlueprintRegistry();
			auto  it = reg.DataHandlers.find(type);
			if (it != reg.DataHandlers.end()) {
				result = it->second(graph, node, outputPinId, runtime);
				runtime.PinValues[outputPinId] = result;
				return result;
			}
		}

		if (type == HXStr("ConstantFloat")) {
			result.Type = HXPinType::Float;
			if (!node.Values.empty())
				try {
					result.AsFloat = std::stof(node.Values[0].Value);
				}
			catch (...) {
			}
			result.AsInt = static_cast<int>(result.AsFloat);
		}
		else if (type == HXStr("ConstantInt")) {
			result.Type = HXPinType::Int;
			if (!node.Values.empty())
				try {
					result.AsInt = std::stoi(node.Values[0].Value);
				}
			catch (...) {
			}
			result.AsFloat = static_cast<float>(result.AsInt);
		}
		else if (type == HXStr("ConstantBool")) {
			result.Type = HXPinType::Bool;
			if (!node.Values.empty()) {
				HXString v = node.Values[0].Value;
				result.AsBool = (v == L"true" || v == L"1" || v == L"True");
			}
			result.AsFloat = result.AsBool ? 1.0f : 0.0f;
			result.AsInt = result.AsBool ? 1 : 0;
		}
		else if (type == HXStr("ConstantString")) {
			result.Type = HXPinType::String;
			if (!node.Values.empty())
				result.AsString = node.Values[0].Value;
		}
		else if (type == HXStr("ConstantColor")) {
			result.Type = HXPinType::Color;
			if (!node.Values.empty())
				result.AsColor = ParseColorString(node.Values[0].Value);
		}
		else if (type == HXStr("GetVariable")) {
			HXString   varName;
			HXNodePin* targetPin = nullptr;
			for (auto& in : node.Inputs) {
				if (in.Label == HXStr("Target") || in.Label == HXStr("Variable"))
					targetPin = &in;
			}
			if (targetPin) {
				HXBlueprintValue targetVal = EvaluateBlueprintPin(graph, targetPin->Id, runtime);
				varName = targetVal.AsString;
			}
			if (varName.empty() && !node.Values.empty())
				varName = node.Values[0].Value;
			auto vit = runtime.Variables.find(varName);
			if (vit != runtime.Variables.end())
				result = vit->second;
			else
				result.AsFloat = 0.0f;
		}
		else if (type == HXStr("Time")) {
			result.Type = HXPinType::Float;
			result.AsFloat = static_cast<float>(clock()) / CLOCKS_PER_SEC;
		}
		else if (type == HXStr("Add")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = a.Type;
			if (a.Type == HXPinType::Int)
				result.AsInt = a.AsInt + b.AsInt;
			else
				result.AsFloat = a.AsFloat + b.AsFloat;
		}
		else if (type == HXStr("Subtract")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = a.Type;
			if (a.Type == HXPinType::Int)
				result.AsInt = a.AsInt - b.AsInt;
			else
				result.AsFloat = a.AsFloat - b.AsFloat;
		}
		else if (type == HXStr("Multiply")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = a.Type;
			if (a.Type == HXPinType::Int)
				result.AsInt = a.AsInt * b.AsInt;
			else
				result.AsFloat = a.AsFloat * b.AsFloat;
		}
		else if (type == HXStr("Divide")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = a.Type;
			if (a.Type == HXPinType::Int)
				result.AsInt = b.AsInt != 0 ? a.AsInt / b.AsInt : 0;
			else
				result.AsFloat = b.AsFloat != 0.0f ? a.AsFloat / b.AsFloat : 0.0f;
		}
		else if (type == HXStr("Sine")) {
			auto a = getInput(0);
			result.Type = HXPinType::Float;
			result.AsFloat = std::sin(a.AsFloat);
		}
		else if (type == HXStr("Cosine")) {
			auto a = getInput(0);
			result.Type = HXPinType::Float;
			result.AsFloat = std::cos(a.AsFloat);
		}
		else if (type == HXStr("Random")) {
			auto mn = getInput(0), mx = getInput(1);
			result.Type = HXPinType::Float;
			float minv = std::min(mn.AsFloat, mx.AsFloat);
			float maxv = std::max(mn.AsFloat, mx.AsFloat);
			result.AsFloat = minv + static_cast<float>(rand()) / RAND_MAX * (maxv - minv);
		}
		else if (type == HXStr("Equals")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = HXPinType::Bool;
			if (a.Type == HXPinType::Int)
				result.AsBool = a.AsInt == b.AsInt;
			else if (a.Type == HXPinType::String)
				result.AsBool = a.AsString == b.AsString;
			else
				result.AsBool = std::abs(a.AsFloat - b.AsFloat) < 0.0001f;
		}
		else if (type == HXStr("Greater")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = HXPinType::Bool;
			result.AsBool = a.AsFloat > b.AsFloat;
		}
		else if (type == HXStr("Less")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = HXPinType::Bool;
			result.AsBool = a.AsFloat < b.AsFloat;
		}
		else if (type == HXStr("And")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = HXPinType::Bool;
			result.AsBool = a.IsTruthy() && b.IsTruthy();
		}
		else if (type == HXStr("Or")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = HXPinType::Bool;
			result.AsBool = a.IsTruthy() || b.IsTruthy();
		}
		else if (type == HXStr("Not")) {
			auto a = getInput(0);
			result.Type = HXPinType::Bool;
			result.AsBool = !a.IsTruthy();
		}
		else if (type == HXStr("FloatToInt")) {
			auto a = getInput(0);
			result.Type = HXPinType::Int;
			result.AsInt = static_cast<int>(a.AsFloat);
		}
		else if (type == HXStr("IntToFloat")) {
			auto a = getInput(0);
			result.Type = HXPinType::Float;
			result.AsFloat = static_cast<float>(a.AsInt);
		}
		else if (type == HXStr("FloatToString")) {
			auto a = getInput(0);
			result.Type = HXPinType::String;
			wchar_t buf[64];
			swprintf(buf, 64, L"%.4f", a.AsFloat);
			result.AsString = HXString(buf);
		}
		else if (type == HXStr("IntToString")) {
			auto a = getInput(0);
			result.Type = HXPinType::String;
			wchar_t buf[64];
			swprintf(buf, 64, L"%d", a.AsInt);
			result.AsString = HXString(buf);
		}
		else if (type == HXStr("BoolToString")) {
			auto a = getInput(0);
			result.Type = HXPinType::String;
			result.AsString = a.AsBool ? HXStr("true") : HXStr("false");
		}
		else if (type == HXStr("ColorToString")) {
			auto a = getInput(0);
			result.Type = HXPinType::String;
			wchar_t buf[64];
			swprintf(buf, 64, L"(%d,%d,%d)", a.AsColor.R, a.AsColor.G, a.AsColor.B);
			result.AsString = HXString(buf);
		}
		else if (type == HXStr("VectorToString")) {
			auto a = getInput(0);
			result.Type = HXPinType::String;
			wchar_t buf[64];
			swprintf(buf, 64, L"(%d,%d)", a.AsVector.X, a.AsVector.Y);
			result.AsString = HXString(buf);
		}
		else if (type == HXStr("StringToFloat")) {
			auto a = getInput(0);
			result.Type = HXPinType::Float;
			try { result.AsFloat = std::stof(a.AsString); }
			catch (...) { result.AsFloat = 0.0f; }
		}
		else if (type == HXStr("StringToInt")) {
			auto a = getInput(0);
			result.Type = HXPinType::Int;
			try { result.AsInt = std::stoi(a.AsString); }
			catch (...) { result.AsInt = 0; }
		}
		else if (type == HXStr("StringAppend")) {
			auto a = getInput(0), b = getInput(1);
			result.Type = HXPinType::String;
			result.AsString = a.AsString + b.AsString;
		}
		else if (type == HXStr("Texture")) {
			result.Type = HXPinType::Texture;
			if (!node.Values.empty())
				result.AsTexture = node.Values[0].Value;
		}
		else if (type == HXStr("Texture Sample")) {
			auto  uv = getInput(0);
			float u = uv.AsVector.X / 255.0f;
			float v = uv.AsVector.Y / 255.0f;
			result.Type = HXPinType::Color;
			result.AsColor = { static_cast<unsigned char>(u * 255), static_cast<unsigned char>(v * 255), 0, 255 };
		}
		else if (type == HXStr("Sample Texture 2D")) {
			auto	 texVal = getInput(0);
			auto	 uvVal = getInput(1);
			HXString path = texVal.AsTexture.empty() ? texVal.AsString : texVal.AsTexture;
			float	 u = uvVal.AsVector.X / 255.0f;
			float	 v = uvVal.AsVector.Y / 255.0f;
			if (path.empty()) {
				result.Type = HXPinType::Color;
				result.AsColor = { 0, 0, 0, 255 };
			}
			else {
				IMAGE* img = GetCachedTextureImage(path);
				if (img && img->getwidth() > 0 && img->getheight() > 0) {
					int		 px = static_cast<int>(std::clamp(u, 0.0f, 1.0f) * (img->getwidth() - 1));
					int		 py = static_cast<int>(std::clamp(v, 0.0f, 1.0f) * (img->getheight() - 1));
					DWORD* buf = GetImageBuffer(img);
					COLORREF c = buf[py * img->getwidth() + px];
					result.Type = HXPinType::Color;
					result.AsColor = { GetRValue(c), GetGValue(c), GetBValue(c), 255 };
				}
				else {
					result.Type = HXPinType::Color;
					result.AsColor = { 0, 0, 0, 255 };
				}
			}
		}
		else if (type == HXStr("Tiling And Offset")) {
			auto uv = getInput(0);
			auto tiling = getInput(1);
			auto offset = getInput(2);
			result.Type = HXPinType::Vector;
			result.AsVector = {
				static_cast<int>((uv.AsVector.X / 255.0f * tiling.AsVector.X / 255.0f + offset.AsVector.X / 255.0f) * 255),
				static_cast<int>((uv.AsVector.Y / 255.0f * tiling.AsVector.Y / 255.0f + offset.AsVector.Y / 255.0f) * 255) };
		}
		else if (type == HXStr("Rotate UV")) {
			auto  uv = getInput(0);
			auto  angle = getInput(1);
			auto  center = getInput(2);
			float cx = center.AsVector.X / 255.0f;
			float cy = center.AsVector.Y / 255.0f;
			float x = uv.AsVector.X / 255.0f - cx;
			float y = uv.AsVector.Y / 255.0f - cy;
			float rad = angle.AsFloat * 3.14159265f / 180.0f;
			float cosA = std::cos(rad), sinA = std::sin(rad);
			result.Type = HXPinType::Vector;
			result.AsVector = { static_cast<int>((x * cosA - y * sinA + cx) * 255),
							   static_cast<int>((x * sinA + y * cosA + cy) * 255) };
		}
		else if (type == HXStr("Flip UV")) {
			auto uv = getInput(0);
			auto flipX = getInput(1);
			auto flipY = getInput(2);
			result.Type = HXPinType::Vector;
			result.AsVector = uv.AsVector;
			if (flipX.AsBool)
				result.AsVector.X = 255 - result.AsVector.X;
			if (flipY.AsBool)
				result.AsVector.Y = 255 - result.AsVector.Y;
		}
		else if (type == HXStr("Color Multiply")) {
			auto a = getInput(0);
			auto b = getInput(1);
			result.Type = HXPinType::Color;
			result.AsColor = {
				static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.R * b.AsColor.R / 255), 0, 255)),
				static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.G * b.AsColor.G / 255), 0, 255)),
				static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.B * b.AsColor.B / 255), 0, 255)), 255 };
		}
		else if (type == HXStr("Color Add")) {
			auto a = getInput(0);
			auto b = getInput(1);
			result.Type = HXPinType::Color;
			result.AsColor = { static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.R + b.AsColor.R), 0, 255)),
							  static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.G + b.AsColor.G), 0, 255)),
							  static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.B + b.AsColor.B), 0, 255)),
							  255 };
		}
		else if (type == HXStr("Color Lerp")) {
			auto  a = getInput(0);
			auto  b = getInput(1);
			auto  t = getInput(2);
			float tt = std::clamp(t.AsFloat, 0.0f, 1.0f);
			result.Type = HXPinType::Color;
			result.AsColor = { static_cast<unsigned char>(a.AsColor.R + (b.AsColor.R - a.AsColor.R) * tt),
							  static_cast<unsigned char>(a.AsColor.G + (b.AsColor.G - a.AsColor.G) * tt),
							  static_cast<unsigned char>(a.AsColor.B + (b.AsColor.B - a.AsColor.B) * tt), 255 };
		}
		else if (type == HXStr("One Minus")) {
			auto a = getInput(0);
			result.Type = HXPinType::Color;
			result.AsColor = { static_cast<unsigned char>(255 - a.AsColor.R), static_cast<unsigned char>(255 - a.AsColor.G),
							  static_cast<unsigned char>(255 - a.AsColor.B), 255 };
		}
		else if (type == HXStr("Saturate")) {
			auto a = getInput(0);
			result.Type = HXPinType::Color;
			result.AsColor = { static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.R), 0, 255)),
							  static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.G), 0, 255)),
							  static_cast<unsigned char>(std::clamp(static_cast<int>(a.AsColor.B), 0, 255)), 255 };
		}
		else if (type == HXStr("Component Mask")) {
			auto input = getInput(0);
			auto r = getInput(1);
			auto g = getInput(2);
			auto b = getInput(3);
			auto aa = getInput(4);
			result.Type = HXPinType::Vector;
			result.AsVector = { 0, 0 };
			if (r.AsBool)
				result.AsVector.X = input.AsVector.X;
			if (g.AsBool)
				result.AsVector.Y = input.AsVector.Y;
		}
		else if (type == HXStr("Blend")) {
			auto base = getInput(0);
			auto blend = getInput(1);
			auto mode = getInput(2);
			result.Type = HXPinType::Color;
			switch (mode.AsInt) {
			case 1: // Multiply
				result.AsColor = { static_cast<unsigned char>(base.AsColor.R * blend.AsColor.R / 255),
								  static_cast<unsigned char>(base.AsColor.G * blend.AsColor.G / 255),
								  static_cast<unsigned char>(base.AsColor.B * blend.AsColor.B / 255), 255 };
				break;
			case 2: // Screen
				result.AsColor = { static_cast<unsigned char>(255 - (255 - base.AsColor.R) * (255 - blend.AsColor.R) / 255),
								  static_cast<unsigned char>(255 - (255 - base.AsColor.G) * (255 - blend.AsColor.G) / 255),
								  static_cast<unsigned char>(255 - (255 - base.AsColor.B) * (255 - blend.AsColor.B) / 255),
								  255 };
				break;
			case 3: // Add
				result.AsColor = { static_cast<unsigned char>(std::clamp(base.AsColor.R + blend.AsColor.R, 0, 255)),
								  static_cast<unsigned char>(std::clamp(base.AsColor.G + blend.AsColor.G, 0, 255)),
								  static_cast<unsigned char>(std::clamp(base.AsColor.B + blend.AsColor.B, 0, 255)), 255 };
				break;
			case 4: // Subtract
				result.AsColor = { static_cast<unsigned char>(std::clamp(base.AsColor.R - blend.AsColor.R, 0, 255)),
								  static_cast<unsigned char>(std::clamp(base.AsColor.G - blend.AsColor.G, 0, 255)),
								  static_cast<unsigned char>(std::clamp(base.AsColor.B - blend.AsColor.B, 0, 255)), 255 };
				break;
			default: // Normal
				result.AsColor = blend.AsColor;
			}
		}
		else if (type == HXStr("Grayscale")) {
			auto a = getInput(0);
			result.Type = HXPinType::Float;
			result.AsFloat = (a.AsColor.R * 0.299f + a.AsColor.G * 0.587f + a.AsColor.B * 0.114f) / 255.0f;
		}
		else if (type == HXStr("Normal Map")) {
			auto a = getInput(0);
			result.Type = HXPinType::Vector;
			result.AsVector = { static_cast<int>((a.AsColor.R / 255.0f) * 255),
							   static_cast<int>((a.AsColor.G / 255.0f) * 255) };
		}
		else if (!node.Inputs.empty()) {
			// Generic pass-through
			result = getInput(0);
		}

		runtime.PinValues[outputPinId] = result;
		return result;
	}

	static void ExecuteNodeFlow(HXNodeGraph* graph, HXNode& node, HXBlueprintRuntime& runtime,
		std::unordered_set<HXString>& visited);

	static void FollowFlowPin(HXNodeGraph* graph, HXNodePin* pin, HXBlueprintRuntime& runtime,
		std::unordered_set<HXString>& visited) {
		if (!pin)
			return;
		auto nextNodes = FindNextFlowNodes(graph, pin->Id);
		for (auto* next : nextNodes) {
			if (next)
				ExecuteNodeFlow(graph, *next, runtime, visited);
		}
	}

	static void ExecuteNodeFlow(HXNodeGraph* graph, HXNode& node, HXBlueprintRuntime& runtime,
		std::unordered_set<HXString>& visited) {
		if (visited.count(node.Id))
			return;
		visited.insert(node.Id);

		const HXString& type = node.TypeName;

		// Collect flow vs data pins
		HXNodePin* flowIn = nullptr;
		std::vector<HXNodePin*> flowOuts;
		std::vector<HXNodePin*> dataIns;
		for (auto& in : node.Inputs) {
			if (in.Type == HXPinType::Flow)
				flowIn = &in;
			else
				dataIns.push_back(&in);
		}
		for (auto& out : node.Outputs) {
			if (out.Type == HXPinType::Flow)
				flowOuts.push_back(&out);
		}

		// Custom flow registry takes priority over built-in dispatch
		{
			auto& reg = GetBlueprintRegistry();
			auto  it = reg.FlowHandlers.find(type);
			if (it != reg.FlowHandlers.end()) {
				it->second(graph, node, runtime, visited);
				return;
			}
		}

		if (type == HXStr("Print")) {
			HXBlueprintValue val;
			if (!dataIns.empty())
				val = EvaluateBlueprintPin(graph, dataIns[0]->Id, runtime);
			runtime.Log += HXStr("[Print] ") + val.ToString() + HXStr("\n");
			if (!flowOuts.empty())
				FollowFlowPin(graph, flowOuts[0], runtime, visited);
		}
		else if (type == HXStr("SetVariable")) {
			HXString   varName;
			HXNodePin* targetPin = nullptr;
			HXNodePin* valuePin = nullptr;
			for (auto* p : dataIns) {
				if (p->Label == HXStr("Target") || p->Label == HXStr("Variable"))
					targetPin = p;
				else if (!valuePin)
					valuePin = p;
			}
			if (targetPin) {
				HXBlueprintValue targetVal = EvaluateBlueprintPin(graph, targetPin->Id, runtime);
				varName = targetVal.AsString;
			}
			if (varName.empty() && !node.Values.empty())
				varName = node.Values[0].Value;
			HXBlueprintValue val;
			if (valuePin)
				val = EvaluateBlueprintPin(graph, valuePin->Id, runtime);
			runtime.Variables[varName] = val;
			runtime.Log += HXStr("[Set Variable] ") + varName + HXStr(" = ") + val.ToString() + HXStr("\n");
			if (!flowOuts.empty())
				FollowFlowPin(graph, flowOuts[0], runtime, visited);
		}
		else if (type == HXStr("Branch")) {
			bool	   cond = false;
			HXNodePin* condPin = nullptr;
			HXNodePin* truePin = nullptr;
			HXNodePin* falsePin = nullptr;
			for (auto* p : dataIns) {
				if (p->Label == HXStr("Condition") || !condPin)
					condPin = p;
			}
			for (auto* p : flowOuts) {
				if (p->Label == HXStr("True") || p->Label == HXStr("true"))
					truePin = p;
				else if (p->Label == HXStr("False") || p->Label == HXStr("false"))
					falsePin = p;
			}
			if (condPin) {
				HXBlueprintValue v = EvaluateBlueprintPin(graph, condPin->Id, runtime);
				cond = v.IsTruthy();
			}
			{
				HXString branchLog = HXStr("[Branch] Condition = ");
				branchLog += cond ? HXStr("True") : HXStr("False");
				branchLog += HXStr("\n");
				runtime.Log += branchLog;
			}
			FollowFlowPin(graph, cond ? truePin : falsePin, runtime, visited);
		}
		else if (type == HXStr("Sequence")) {
			runtime.Log += HXStr("[Sequence]\n");
			for (auto* p : flowOuts) {
				FollowFlowPin(graph, p, runtime, visited);
			}
		}
		else if (type == HXStr("ForLoop")) {
			HXNodePin* startPin = nullptr, * endPin = nullptr;
			HXNodePin* bodyPin = nullptr, * donePin = nullptr;
			for (auto* p : dataIns) {
				if (p->Label == HXStr("Start") || p->Label == HXStr("First Index"))
					startPin = p;
				else if (p->Label == HXStr("End") || p->Label == HXStr("Last Index"))
					endPin = p;
			}
			for (auto* p : flowOuts) {
				if (p->Label == HXStr("Loop Body") || p->Label == HXStr("Body"))
					bodyPin = p;
				else if (p->Label == HXStr("Completed") || p->Label == HXStr("Done"))
					donePin = p;
			}
			int startIdx = startPin ? static_cast<int>(EvaluateBlueprintPin(graph, startPin->Id, runtime).AsFloat) : 0;
			int endIdx = endPin ? static_cast<int>(EvaluateBlueprintPin(graph, endPin->Id, runtime).AsFloat) : 0;
			runtime.Log +=
				HXStr("[ForLoop] ") + std::to_wstring(startIdx) + HXStr(" to ") + std::to_wstring(endIdx) + HXStr("\n");
			for (int i = startIdx; i < endIdx; ++i) {
				HXBlueprintValue idxVal;
				idxVal.Type = HXPinType::Int;
				idxVal.AsInt = i;
				runtime.Variables[HXStr("__loop_") + node.Id] = idxVal;
				// Clear visited and pin-value cache for body-connected nodes so they re-execute each iteration
				auto bodyTargets = FindNextFlowNodes(graph, bodyPin->Id);
				for (auto* t : bodyTargets) {
					if (t)
						visited.erase(t->Id);
				}
				runtime.PinValues.clear();
				FollowFlowPin(graph, bodyPin, runtime, visited);
			}
			FollowFlowPin(graph, donePin, runtime, visited);
		}
		else if (!flowOuts.empty()) {
			// Data nodes with flow passthrough (e.g. GetVariable connected to flow chain)
			FollowFlowPin(graph, flowOuts[0], runtime, visited);
		}
	}

	void ExecuteBlueprint(HXNodeGraph* graph, HXBlueprintRuntime& runtime) {
		runtime.PinValues.clear();
		runtime.Log.clear();
		runtime.LastError.clear();
		runtime.Log += HXStr("=== Blueprint Execution ===\n");

		std::unordered_set<HXString> visited;
		bool						 foundEvent = false;
		for (auto& node : graph->Nodes) {
			if (node.Category == HX::HXNodeCategory::Event || node.TypeName == HXStr("Start")) {
				foundEvent = true;
				runtime.Log += HXStr("--> Starting from '") + node.Title + HXStr("'\n");
				ExecuteNodeFlow(graph, node, runtime, visited);
			}
		}
		if (!foundEvent) {
			runtime.Log += HXStr("No Start node found.\n");
		}
		runtime.Log += HXStr("=== Done ===\n");
	}

	// ------------------------------------------------------------------
	// Serialization helpers
	// ------------------------------------------------------------------

	static std::vector<HXString> SplitBpLine(const HXString& line, wchar_t delim = L'|') {
		std::vector<HXString> parts;
		HXString			  current;
		for (wchar_t c : line) {
			if (c == delim) {
				parts.push_back(current);
				current.clear();
			}
			else {
				current += c;
			}
		}
		parts.push_back(current);
		return parts;
	}

	static HXString PinTypeToString(HXPinType t) {
		switch (t) {
		case HXPinType::Flow:
			return L"Flow";
		case HXPinType::Bool:
			return L"Bool";
		case HXPinType::Int:
			return L"Int";
		case HXPinType::Float:
			return L"Float";
		case HXPinType::String:
			return L"String";
		case HXPinType::Vector:
			return L"Vector";
		case HXPinType::Color:
			return L"Color";
		case HXPinType::Texture:
			return L"Texture";
		case HXPinType::Object:
			return L"Object";
		default:
			return L"Custom";
		}
	}

	static HXPinType ParsePinType(const HXString& s) {
		if (s == L"Flow")
			return HXPinType::Flow;
		if (s == L"Bool")
			return HXPinType::Bool;
		if (s == L"Int")
			return HXPinType::Int;
		if (s == L"Float")
			return HXPinType::Float;
		if (s == L"String")
			return HXPinType::String;
		if (s == L"Vector")
			return HXPinType::Vector;
		if (s == L"Color")
			return HXPinType::Color;
		if (s == L"Texture")
			return HXPinType::Texture;
		if (s == L"Object")
			return HXPinType::Object;
		return HXPinType::Custom;
	}

	static HXString CategoryToString(HXNodeCategory c) {
		switch (c) {
		case HXNodeCategory::Event:
			return L"Event";
		case HXNodeCategory::Function:
			return L"Function";
		case HXNodeCategory::Variable:
			return L"Variable";
		case HXNodeCategory::Math:
			return L"Math";
		case HXNodeCategory::FlowControl:
			return L"FlowControl";
		case HXNodeCategory::Material:
			return L"Material";
		case HXNodeCategory::Output:
			return L"Output";
		case HXNodeCategory::Comment:
			return L"Comment";
		default:
			return L"Custom";
		}
	}

	static HXNodeCategory ParseCategory(const HXString& s) {
		if (s == L"Event")
			return HXNodeCategory::Event;
		if (s == L"Function")
			return HXNodeCategory::Function;
		if (s == L"Variable")
			return HXNodeCategory::Variable;
		if (s == L"Math")
			return HXNodeCategory::Math;
		if (s == L"FlowControl")
			return HXNodeCategory::FlowControl;
		if (s == L"Material")
			return HXNodeCategory::Material;
		if (s == L"Output")
			return HXNodeCategory::Output;
		if (s == L"Comment")
			return HXNodeCategory::Comment;
		return HXNodeCategory::Custom;
	}

	void SaveBlueprint(const HXNodeGraph& graph, float zoom, const HXPoint& offset, const HXString& path) {
		std::wofstream file(path.c_str());
		if (!file.is_open())
			return;

		file << L"HXBP|1\n";
		file << std::setprecision(6) << std::fixed;
		file << L"ZOOM|" << zoom << L"\n";
		file << L"OFFSET|" << offset.X << L"|" << offset.Y << L"\n";

		for (const auto& node : graph.Nodes) {
			file << L"NODE|" << node.Id << L"\n";
			file << L"TITLE|" << node.Title << L"\n";
			file << L"TYPENAME|" << node.TypeName << L"\n";
			file << L"ICON|" << node.Icon << L"\n";
			file << L"CATEGORY|" << CategoryToString(node.Category) << L"\n";
			file << L"POSITION|" << node.Position.X << L"|" << node.Position.Y << L"\n";
			file << L"SIZE|" << node.Size.X << L"|" << node.Size.Y << L"\n";
			file << L"SELECTED|" << (node.Selected ? 1 : 0) << L"\n";
			file << L"COLLAPSED|" << (node.Collapsed ? 1 : 0) << L"\n";

			for (const auto& pin : node.Inputs) {
				file << L"PIN_IN|" << pin.Id << L"|" << pin.Label << L"|" << PinTypeToString(pin.Type) << L"|"
					<< (pin.Direction == HXPinDirection::Input ? L"Input" : L"Output") << L"|" << pin.DefaultValue
					<< L"\n";
			}

			for (const auto& pin : node.Outputs) {
				file << L"PIN_OUT|" << pin.Id << L"|" << pin.Label << L"|" << PinTypeToString(pin.Type) << L"|"
					<< (pin.Direction == HXPinDirection::Input ? L"Input" : L"Output") << L"|" << pin.DefaultValue
					<< L"\n";
			}

			for (const auto& val : node.Values) {
				file << L"VALUE|" << val.Name << L"|" << val.Value << L"|" << PinTypeToString(val.Type) << L"\n";
			}

			file << L"END_NODE\n";
		}

		for (const auto& link : graph.Links) {
			file << L"LINK|" << link.StartPinId << L"|" << link.EndPinId << L"\n";
		}

		file << L"END\n";
		file.close();
	}

	bool LoadBlueprint(HXNodeGraph& graph, float& zoom, HXPoint& offset, const HXString& path) {
		std::wifstream file(path.c_str());
		if (!file.is_open())
			return false;

		graph.Nodes.clear();
		graph.Links.clear();

		HXString line;
		bool	 firstLine = true;
		HXNode* currentNode = nullptr;

		while (std::getline(file, line)) {
			if (!line.empty() && line.back() == L'\r')
				line.pop_back();
			if (line.empty())
				continue;

			auto parts = SplitBpLine(line, L'|');
			if (parts.empty())
				continue;

			const HXString& cmd = parts[0];

			if (firstLine) {
				if (cmd != L"HXBP" || parts.size() < 2 || parts[1] != L"1") {
					return false;
				}
				firstLine = false;
				continue;
			}

			if (cmd == L"ZOOM" && parts.size() > 1) {
				zoom = std::stof(parts[1]);
			}
			else if (cmd == L"OFFSET" && parts.size() > 2) {
				offset.X = std::stof(parts[1]);
				offset.Y = std::stof(parts[2]);
			}
			else if (cmd == L"NODE" && parts.size() > 1) {
				graph.Nodes.emplace_back();
				currentNode = &graph.Nodes.back();
				currentNode->Id = parts[1];
			}
			else if (cmd == L"TITLE" && parts.size() > 1 && currentNode) {
				currentNode->Title = parts[1];
			}
			else if (cmd == L"TYPENAME" && parts.size() > 1 && currentNode) {
				currentNode->TypeName = parts[1];
			}
			else if (cmd == L"ICON" && parts.size() > 1 && currentNode) {
				currentNode->Icon = parts[1];
			}
			else if (cmd == L"CATEGORY" && parts.size() > 1 && currentNode) {
				currentNode->Category = ParseCategory(parts[1]);
			}
			else if (cmd == L"POSITION" && parts.size() > 2 && currentNode) {
				currentNode->Position.X = std::stof(parts[1]);
				currentNode->Position.Y = std::stof(parts[2]);
			}
			else if (cmd == L"SIZE" && parts.size() > 2 && currentNode) {
				currentNode->Size.X = std::stof(parts[1]);
				currentNode->Size.Y = std::stof(parts[2]);
			}
			else if (cmd == L"SELECTED" && parts.size() > 1 && currentNode) {
				currentNode->Selected = std::stoi(parts[1]) != 0;
			}
			else if (cmd == L"COLLAPSED" && parts.size() > 1 && currentNode) {
				currentNode->Collapsed = std::stoi(parts[1]) != 0;
			}
			else if (cmd == L"PIN_IN" && parts.size() > 4 && currentNode) {
				HXNodePin pin;
				pin.Id = parts[1];
				pin.Label = parts[2];
				pin.Type = ParsePinType(parts[3]);
				pin.Direction = (parts[4] == L"Input" ? HXPinDirection::Input : HXPinDirection::Output);
				if (parts.size() > 5)
					pin.DefaultValue = parts[5];
				currentNode->Inputs.push_back(pin);
			}
			else if (cmd == L"PIN_OUT" && parts.size() > 4 && currentNode) {
				HXNodePin pin;
				pin.Id = parts[1];
				pin.Label = parts[2];
				pin.Type = ParsePinType(parts[3]);
				pin.Direction = (parts[4] == L"Input" ? HXPinDirection::Input : HXPinDirection::Output);
				if (parts.size() > 5)
					pin.DefaultValue = parts[5];
				currentNode->Outputs.push_back(pin);
			}
			else if (cmd == L"VALUE" && parts.size() > 3 && currentNode) {
				HXNodeValue val;
				val.Name = parts[1];
				val.Value = parts[2];
				val.Type = ParsePinType(parts[3]);
				currentNode->Values.push_back(val);
			}
			else if (cmd == L"END_NODE") {
				currentNode = nullptr;
			}
			else if (cmd == L"LINK" && parts.size() > 2) {
				HXNodeLink link;
				link.StartPinId = parts[1];
				link.EndPinId = parts[2];
				graph.Links.push_back(link);
			}
			else if (cmd == L"END") {
				break;
			}
		}

		return true;
	}

} // namespace HX
