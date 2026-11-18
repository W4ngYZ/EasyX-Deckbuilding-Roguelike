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
  * \file hex_window.cpp
  * \brief The window for the HiEasyX
  */
#include <cmath>
#include <include/hex.h>
#include <include/hex_window.h>
#include <include/hex_pool.h>
#include <stdio.h>

namespace HX {
	extern std::vector<HXWindow*> G_WindowPool;
	extern void ResetHXWindow(HXWindow* Window);

	void Window(const HXString& Title, WindowProfile& Profile) {
		auto& context = GetContext();
		auto& theme = GetTheme();

		// Flush the previous window's draw list before starting a new one
		if (!context.Windows.empty()) {
			auto* prev = context.Windows.back();
			if (prev->DrawList && !prev->DrawList->Commands.empty()) {
				FlushDrawList(prev->RealPainter, *prev->DrawList);
				prev->DrawList->Clear();
			}
			for (auto* p : prev->PendingSubPainters) {
				GetSubPainterPool().Release(p);
			}
			prev->PendingSubPainters.clear();
		}

		HXWindow* window;
		if (!G_WindowPool.empty()) {
			window = G_WindowPool.back();
			G_WindowPool.pop_back();
			ResetHXWindow(window);
		}
		else {
			window = new HXWindow{};
		}
		window->Title = Title;
		window->Size = Profile.Size;
		window->Where = HXPoint{ 0, 0 };
		window->Folded = Profile.Folded;

		// Apply saved layout if available
		if (HX::HasSavedWindowPosition(Title)) {
			Profile.Position = HX::GetSavedWindowPosition(Title);
			Profile.Size = HX::GetSavedWindowSize(Title);
			window->Size = Profile.Size;
		}
		context.Windows.emplace_back(window);
		context.CurrentWindow = context.Windows.back();

		context.CurrentWindow->CurrentLayout = &context.CurrentWindow->Layout;
		context.CurrentWindow->Where = Profile.Position;
		context.CurrentWindow->Layout.AvailableRegion = { 0, 0, Profile.Size.X, Profile.Size.Y };
		context.LayoutStack.push_back(&context.CurrentWindow->Layout);

		if (Profile.Folded) {
			context.CurrentWindow->RealPainter = GetSubPainterPool().Acquire(
				context.CurrentWindow->Size.X, Profile.TitleBarHeight);
		}
		else {
			context.CurrentWindow->RealPainter = GetSubPainterPool().Acquire(
				context.CurrentWindow->Size.X, context.CurrentWindow->Size.Y);
		}
		context.CurrentWindow->Painter = new HXDrawListRecorder(
			context.CurrentWindow->RealPainter, context.CurrentWindow->DrawList);

		auto windowBarRectangle = HXRect{ Profile.Position.X, Profile.Position.Y, Profile.Position.X + Profile.Size.X,
										 Profile.Position.Y + Profile.TitleBarHeight };
		auto windowFoldBar = HXRect{ 4 + Profile.Position.X, 15 + Profile.Position.Y,
									16 + Profile.Position.X,
									static_cast<HXGInt>(ceil(6 * sqrt(3) + 15)) + Profile.Position.Y };
		auto windowRectangle = HXRect{
			Profile.Position.X, Profile.Position.Y, Profile.Position.X + Profile.Size.X, Profile.Position.Y + Profile.Size.Y
		};

		// Judge the window drag operation
		for (auto& Message : context.MessageQuery) {
			if (Message.Processed) {
				continue;
			}

			if (Message.MouseLeftPressed && windowBarRectangle.PointInside({ Message.MouseX, Message.MouseY })) {
				if (windowFoldBar.PointInside({ Message.MouseX, Message.MouseY })) {
					Profile.Folded = !Profile.Folded;
				}
				else if (Profile.Movable) {
					Profile.DeltaX = Message.MouseX - windowBarRectangle.Left;
					Profile.DeltaY = Message.MouseY - windowBarRectangle.Top;

					Profile.InDrag = true;
				}

				Message.Processed = true;
			}
			else if (Message.MouseLeftRelease) {
				Profile.InDrag = false;
			}

			if (Message.MouseAction && Profile.InDrag) {
				Message.Processed = true;

				context.CurrentWindow->Where = { Message.MouseX - Profile.DeltaX, Message.MouseY - Profile.DeltaY };
			}

			if (Profile.Folded || !Profile.Sizable) {
				continue;
			}

			bool needCursorStyle = false;

			if (Message.MouseLeftRelease) {
				Profile.InWidthSize = false;
				Profile.InAllSize = false;
				Profile.InHeightSize = false;
				Profile.InRAllSize = false;
				Profile.InRWidthSize = false;

				Profile.InCursorStyling = false;

				continue;
			}

			// Process All Size (Reserved)
			if (Message.MouseAction && Message.MouseX >= windowRectangle.Left && Message.MouseY >=
				windowRectangle.Bottom - 10 &&
				Message.MouseX <= windowRectangle.Left + 10 && Message.MouseY <= windowRectangle.Bottom) {
				Message.Processed = true;

				if (Message.MouseLeftPressed) {
					Profile.InRAllSize = true;
					Profile.OriginX = Message.MouseX;
					Profile.OriginY = Message.MouseY;
					Profile.LastSize = Profile.Size;
				}

				Profile.InCursorStyling = true;
				context.FrameCursor = HXCursorStyle::ResizeES;

				needCursorStyle = true;
			}

			// Process All Size
			if (Message.MouseAction && Message.MouseX >= windowRectangle.Right - 10 && Message.MouseY >=
				windowRectangle.Bottom - 10 &&
				Message.MouseX <= windowRectangle.Right && Message.MouseY <= windowRectangle.Bottom) {
				Message.Processed = true;

				if (Message.MouseLeftPressed) {
					Profile.InAllSize = true;
					Profile.OriginX = Message.MouseX;
					Profile.OriginY = Message.MouseY;
					Profile.LastSize = Profile.Size;
				}

				Profile.InCursorStyling = true;
				context.FrameCursor = HXCursorStyle::ResizeNW;

				needCursorStyle = true;
			}

			// Process Width Size (Reserved)
			if (Message.MouseAction && Message.MouseX >= windowRectangle.Left && Message.MouseY >=
				windowRectangle.Top + Profile.TitleBarHeight &&
				Message.MouseX <= windowRectangle.Left + 10 && Message.MouseY <= windowRectangle.Bottom && !(Profile.
					InAllSize || Profile.InRAllSize)) {
				Message.Processed = true;

				if (Message.MouseLeftPressed) {
					Profile.InRWidthSize = true;
					Profile.OriginX = Message.MouseX;
					Profile.OriginY = Message.MouseY;
					Profile.LastSize = Profile.Size;
				}

				Profile.InCursorStyling = true;
				if (!needCursorStyle) {
					context.FrameCursor = HXCursorStyle::ResizeE;
				}

				needCursorStyle = true;
			}

			// Process Width Size
			if (Message.MouseAction && Message.MouseX >= windowRectangle.Right - 10 && Message.MouseY >=
				windowRectangle.Top &&
				Message.MouseX <= windowRectangle.Right && Message.MouseY <= windowRectangle.Bottom && !(Profile.
					InAllSize || Profile.InRAllSize)) {
				Message.Processed = true;

				if (Message.MouseLeftPressed) {
					Profile.InWidthSize = true;
					Profile.OriginX = Message.MouseX;
					Profile.OriginY = Message.MouseY;
					Profile.LastSize = Profile.Size;
				}

				Profile.InCursorStyling = true;
				if (!needCursorStyle) {
					context.FrameCursor = HXCursorStyle::ResizeE;
				}

				needCursorStyle = true;
			}

			// Process Height Size
			if (Message.MouseAction && Message.MouseX >= windowRectangle.Left && Message.MouseY >=
				windowRectangle.Bottom - 10 &&
				Message.MouseX <= windowRectangle.Right && Message.MouseY <= windowRectangle.Bottom && !(Profile.
					InAllSize || Profile.InRAllSize)) {
				Message.Processed = true;

				if (Message.MouseLeftPressed) {
					Profile.InHeightSize = true;
					Profile.OriginX = Message.MouseX;
					Profile.OriginY = Message.MouseY;
					Profile.LastSize = Profile.Size;
				}

				Profile.InCursorStyling = true;
				if (!needCursorStyle) {
					context.FrameCursor = HXCursorStyle::ResizeN;
				}

				needCursorStyle = true;
			}

			if (!needCursorStyle && !(Profile.InAllSize || Profile.InWidthSize || Profile.InHeightSize || Profile.
				InRAllSize) && Profile.InCursorStyling) {
				Profile.InCursorStyling = false;
			}

			if (Message.MouseAction && Profile.InAllSize) {
				Message.Processed = true;

				context.CurrentWindow->Size = { Profile.LastSize.X - (Profile.OriginX - Message.MouseX),
											   Profile.LastSize.Y - (Profile.OriginY - Message.MouseY) };
			}
			if (Message.MouseAction && Profile.InWidthSize) {
				Message.Processed = true;

				context.CurrentWindow->Size = { Profile.LastSize.X - (Profile.OriginX - Message.MouseX),
											   Profile.LastSize.Y };
			}
			if (Message.MouseAction && Profile.InHeightSize) {
				Message.Processed = true;

				context.CurrentWindow->Size = { Profile.LastSize.X,
											   Profile.LastSize.Y - (Profile.OriginY - Message.MouseY) };
			}
			if (Message.MouseAction && Profile.InRAllSize) {
				Message.Processed = true;

				context.CurrentWindow->Where.X = Message.MouseX;
				context.CurrentWindow->Size = { Profile.LastSize.X + (Profile.OriginX - Message.MouseX),
											   Profile.LastSize.Y - (Profile.OriginY - Message.MouseY) };
			}
			if (Message.MouseAction && Profile.InRWidthSize) {
				Message.Processed = true;

				context.CurrentWindow->Where.X = Message.MouseX;
				context.CurrentWindow->Size = { Profile.LastSize.X + (Profile.OriginX - Message.MouseX),
											   Profile.LastSize.Y };
			}
		}

		Profile.Size = context.CurrentWindow->Size;

		bool vertUseMax = Profile.MaxSize.X > 0;
		bool horUseMax = Profile.MaxSize.Y > 0;
		bool vertUseMin = Profile.MinSize.X > 0;
		bool horUseMin = Profile.MinSize.Y > 0;

		if (!vertUseMin) {
			vertUseMin = true;
			Profile.MinSize.X = 50;
		}
		if (!horUseMin) {
			horUseMin = true;
			Profile.MinSize.Y = 50;
		}

		if (Profile.Size.X > Profile.MaxSize.X && vertUseMax) {
			Profile.Size.X = Profile.MaxSize.X;
		}
		if (Profile.Size.Y > Profile.MaxSize.Y && horUseMax) {
			Profile.Size.Y = Profile.MaxSize.Y;
		}
		if (Profile.Size.X < Profile.MinSize.X) {
			Profile.Size.X = Profile.MinSize.X;
		}
		if (Profile.Size.Y < Profile.MinSize.Y) {
			Profile.Size.Y = Profile.MinSize.Y;
		}

		windowBarRectangle = HXRect{ 0, 0, Profile.Size.X, Profile.TitleBarHeight };

		// Draw Title Bar
		context.CurrentWindow->Painter->Begin();
		context.CurrentWindow->Painter->Clear(theme.WindowBackground);
		context.CurrentWindow->Painter->DrawFilledRectangle(windowBarRectangle, theme.WindowTitleBackground,
			theme.WindowTitleBackground);
		context.CurrentWindow->Painter->DrawLine(
			{ 0, Profile.TitleBarHeight - 1 },
			{ Profile.Size.X, Profile.TitleBarHeight - 1 },
			theme.BorderSubtle);
		if (Profile.Folded) {
			DrawDropdownIcon(context.CurrentWindow->Painter, { 4, 15 }, 12, false, theme.TextSecondary);
		}
		else {
			DrawDropdownIcon(context.CurrentWindow->Painter, { 4, 15 }, 12, true, theme.TextSecondary);
		}
		context.CurrentWindow->Painter->DrawText(Title, HXFont{},
			{ 20, (Profile.TitleBarHeight - theme.FontSizeTitle) / 2 },
			theme.WindowTitleText, theme.FontSizeTitle);
		context.CurrentWindow->Painter->End();

		Profile.Position = context.CurrentWindow->Where;

		// Advance baseline past the title bar so child controls don't overlap it
		context.CurrentWindow->Layout.BaseLine = Profile.TitleBarHeight;
	}

	void DrawDropdownIcon(HXBufferPainter* Painter, HXPoint Where, HXGInt Size, bool Direction, HXColor Color) {
		const auto           rectangleHeight = static_cast<HXGInt>(ceil(6 * sqrt(3)));
		std::vector<HXPoint> rectangleVertexesDown = {
			{ 0, 0 },
			{ 12, 0 },
			{ 6, rectangleHeight },
		};
		std::vector<HXPoint> rectangleVertexesUp = {
			{ 0, rectangleHeight },
			{ 12, rectangleHeight },
			{ 6, 0 },
		};

		const auto ratio = Size / 12.0f;

		for (auto& vertex : rectangleVertexesUp) {
			vertex.X = vertex.X * ratio + Where.X;
			vertex.Y = vertex.Y * ratio + Where.Y;
		}
		for (auto& vertex : rectangleVertexesDown) {
			vertex.X = vertex.X * ratio + Where.X;
			vertex.Y = vertex.Y * ratio + Where.Y;
		}

		if (Direction) {
			Painter->DrawFilledPolygon(rectangleVertexesDown, Color, Color);
		}
		else {
			Painter->DrawFilledPolygon(rectangleVertexesUp, Color, Color);
		}
	}

}
