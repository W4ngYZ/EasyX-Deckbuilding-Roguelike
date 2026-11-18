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
 * \file hex_scroller.cpp
 * \brief The scroller in for the HiEasyX
 */

#include <include/hex.h>
#include <include/hex_pool.h>
#include <include/hex_scroller.h>

#undef max
#undef min

namespace HX {
enum class ButtonType {
	Vertical, Horizontal
};

float ProcessButtonMessage(ScrollerButtonProfile &Profile, HXRect Button, ButtonType Type, HXRect Scroller) {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	const auto width  = Button.CalWidth();
	const auto height = Button.CalHeight();

	for (auto &Message : context.MessageQuery) {
		auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
		if (Message.Processed) {
			continue;
		}
		if (Message.MouseAction) {
			if (Profile.InDrag && !Message.MouseLeftRelease) {
				if (Type == ButtonType::Horizontal) {
					Button.Left  = mouse.X - Profile.DeltaX;
					Button.Right = Button.Left + width;
				}
				if (Type == ButtonType::Vertical) {
					Button.Top    = mouse.Y - Profile.DeltaY;
					Button.Bottom = Button.Top + height;
				}
			}
			if (Button.PointInside(mouse)) {
				Profile.OnHover = true;
				if (Message.MouseLeftPressed) {
					Message.Processed = true;

					Profile.InDrag = true;
					Profile.DeltaX = mouse.X - Button.Left;
					Profile.DeltaY = mouse.Y - Button.Top;
				}
				if (Message.MouseLeftRelease) {
					Profile.InDrag = false;
				}
			} else {
				if (Message.MouseLeftRelease) {
					Profile.InDrag = false;
				}

				Profile.OnHover = false;
			}
		}
	}

	if (Type == ButtonType::Vertical) {
		if (Button.Top < Scroller.Top) {
			Button.Top = Scroller.Top;
		}
		if (Button.Bottom > Scroller.Bottom) {
			Button.Top = Scroller.Bottom - height;
		}

		Button.Bottom = Button.Top + height;

		return static_cast<float>(Button.Top - Scroller.Top) / static_cast<float>(Scroller.CalHeight() - height);
	}
	if (Type == ButtonType::Horizontal) {
		if (Button.Left < Scroller.Left) {
			Button.Left = Scroller.Left;
		}
		if (Button.Right > Scroller.Right) {
			Button.Left = Scroller.Right - width;
		}

		Button.Right = Button.Left + width;

		return static_cast<float>(Button.Left - Scroller.Left) / static_cast<float>(Scroller.CalWidth() - width);
	}

	return 0.0f;
}

HXRect CalHorScrollButton(ScrollerProfile &Profile) {
	auto horizontalViewButtonWidth = static_cast<HXGInt>(
		(static_cast<float>(Profile.Size.X) / static_cast<float>(Profile.ViewSpace.X)) *
		static_cast<float>(Profile.Size.X));
	auto horizontalViewButtonRight = static_cast<HXGInt>(
		Profile.HorizontalVPer * static_cast<float>(Profile.Size.X - horizontalViewButtonWidth));
	auto horizontalViewButtonRect = HXRect{horizontalViewButtonRight,
	                                       Profile.Size.Y - Profile.ScrollbarThickness, horizontalViewButtonRight + horizontalViewButtonWidth,
	                                       Profile.Size.Y};
	horizontalViewButtonRect.Left += Profile.Where.X;
	horizontalViewButtonRect.Top += Profile.Where.Y;
	horizontalViewButtonRect.Right += Profile.Where.X;
	horizontalViewButtonRect.Bottom += Profile.Where.Y;

	if (Profile.Size.X == Profile.ViewSpace.X) {
		horizontalViewButtonRect = {Profile.Where.X, Profile.Where.Y, Profile.Where.X, Profile.ScrollbarThickness};
	}

	return horizontalViewButtonRect;
}

HXRect CalVertScrollButton(ScrollerProfile &Profile) {
	auto verticalViewButtonHeight = static_cast<HXGInt>(
		(static_cast<float>(Profile.Size.Y) / static_cast<float>(Profile.ViewSpace.Y)) *
		static_cast<float>(Profile.Size.Y));
	auto verticalViewButtonTop = static_cast<HXGInt>(
		Profile.VerticalVPer * static_cast<float>(Profile.Size.Y - verticalViewButtonHeight));
	auto verticalViewButtonRect = HXRect{Profile.Size.X - Profile.ScrollbarThickness,
	                                     verticalViewButtonTop, Profile.Size.X,
	                                     verticalViewButtonTop + verticalViewButtonHeight};
	verticalViewButtonRect.Left += Profile.Where.X;
	verticalViewButtonRect.Top += Profile.Where.Y;
	verticalViewButtonRect.Right += Profile.Where.X;
	verticalViewButtonRect.Bottom += Profile.Where.Y;

	if (Profile.Size.Y == Profile.ViewSpace.Y) {
		verticalViewButtonRect = {Profile.Where.X, Profile.Where.Y, Profile.Where.X + Profile.ScrollbarThickness, Profile.Where.Y};
	}

	return verticalViewButtonRect;
}

void RenderScrollerButton(ScrollerButtonProfile &Profile, HXRect Rect, HXBufferPainter *Painter) {
	auto &theme = GetTheme();
	if (Profile.InDrag) {
		Painter->DrawFilledRectangle(Rect, theme.ButtonPressedBorder, theme.ButtonPressedBackground);
	} else if (Profile.OnHover) {
		Painter->DrawFilledRectangle(Rect, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
	} else {
		Painter->DrawFilledRectangle(Rect, theme.ButtonBorder, theme.ButtonBackground);
	}
}

void BeginScroller(ScrollerProfile &Profile) {
	auto &context = GetContext();
	if (RoughVisibility()) {
		Profile.Visible   = false;
		auto scroller     = new HXScroller{};
		scroller->Visible = false;
		scroller->Last    = context.CurrentWindow->Scroller;

		context.LayoutStack.push_back(&scroller->Layout);
		context.CurrentWindow->Scroller      = scroller;

		return;
	}

	auto &theme = GetTheme();

	if (!CurrentVisibility(Profile.Size)) {
		if (NeedLayoutCal()) {
			LayoutNextControl(Profile.Size);
		}

		Profile.Visible                 = false;
		auto scroller                   = new HXScroller{};
		scroller->Visible               = false;
		scroller->Last                  = context.CurrentWindow->Scroller;

		context.LayoutStack.push_back(&scroller->Layout);
		context.CurrentWindow->Scroller = scroller;

		return;
	}

	Profile.Visible = true;

	auto position = CurrentLayoutPosition();
	Profile.Where = position;

	auto verticalViewButtonRect   = CalVertScrollButton(Profile);
	auto horizontalViewButtonRect = CalHorScrollButton(Profile);

	auto scrollerRect = HXRect{Profile.Where.X, Profile.Where.Y, Profile.Where.X + Profile.Size.X,
	                           Profile.Where.Y + Profile.Size.Y};

	// Track when the mouse is over the scroller for grace-period wheel scrolling
	for (auto &Message : context.MessageQuery) {
		if (Message.MouseWheel && !Message.Processed) {
			auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
			if (scrollerRect.PointInside(mouse)) {
				Profile.LastWheelTime = clock();
			}
		}
	}

	Profile.VerticalVPer = ProcessButtonMessage(Profile.VerticalButton, verticalViewButtonRect, ButtonType::Vertical,
	                                            scrollerRect);
	Profile.HorizontalVPer = ProcessButtonMessage(Profile.HorizontalButton, horizontalViewButtonRect,
	                                              ButtonType::Horizontal, scrollerRect);

	auto viewportLocate = HXPoint{static_cast<HXGInt>(Profile.HorizontalVPer * (Profile.ViewSpace.X - Profile.Size.X)),
	                             static_cast<HXGInt>(Profile.VerticalVPer * (Profile.ViewSpace.Y - Profile.Size.Y))};

	auto scroller                     = new HXScroller{};
	scroller->Layout.BaseLine         = 5 - viewportLocate.Y;
	scroller->Layout.LeftPadding      = -viewportLocate.X;
	scroller->Layout.LeftPaddingValue = -viewportLocate.X;

	context.LayoutStack.push_back(&scroller->Layout);

	scroller->Layout.AvailableRegion = {0, 0, Profile.Size.X, Profile.Size.Y};

	scroller->Painter = GetSubPainterPool().Acquire(Profile.Size.X, Profile.Size.Y);

	if (scroller->Painter != nullptr) {
		scroller->Painter->Begin();
		scroller->Painter->Clear(theme.ScrollerBackground);
		scroller->Painter->End();
	}

	// Setup scroller to the context
	scroller->Last = context.CurrentWindow->Scroller;

	context.CurrentWindow->ScrollerX += position.X;
	context.CurrentWindow->ScrollerY += position.Y;

	context.CurrentWindow->Scroller = scroller;
}

void EndScroller(ScrollerProfile &Profile) {
	auto &context      = GetContext();
	auto  scroller     = context.CurrentWindow->Scroller;
	auto  lastScroller = context.CurrentWindow->Scroller->Last;

	if (!Profile.Visible) {
		delete scroller;
		context.CurrentWindow->Scroller = lastScroller;
		if (!context.LayoutStack.empty()) {
			context.LayoutStack.pop_back();
		}

		return;
	}

	context.CurrentWindow->Scroller = lastScroller;
	context.CurrentWindow->ScrollerX -= Profile.Where.X;
	context.CurrentWindow->ScrollerY -= Profile.Where.Y;

	auto scrollerRect = HXRect{Profile.Where.X, Profile.Where.Y, Profile.Where.X + Profile.Size.X,
	                           Profile.Where.Y + Profile.Size.Y};

	// Process wheel events in EndScroller so inner scrollers get priority over outer ones.
	if (Profile.Visible) {
		bool canScrollVert = Profile.ViewSpace.Y > Profile.Size.Y;
		bool canScrollHorz = Profile.ViewSpace.X > Profile.Size.X;
		if (canScrollVert || canScrollHorz) {
			for (auto &Message : context.MessageQuery) {
				if (Message.Processed) {
					continue;
				}
				if (!Message.MouseWheel) {
					continue;
				}
				bool shouldScroll = false;
				auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
				if (scrollerRect.PointInside(mouse)) {
					shouldScroll = true;
					Profile.LastWheelTime = clock();
				} else if (clock() - Profile.LastWheelTime < Profile.WheelGracePeriodMs) {
					shouldScroll = true;
				}
				if (shouldScroll) {
					if (Message.InShift && canScrollHorz) {
						float scrollStep = 90.0f / static_cast<float>(Profile.ViewSpace.X - Profile.Size.X);
						if (Message.MouseWheelUp) {
							Profile.HorizontalVPer = std::min(1.0f, Profile.HorizontalVPer + scrollStep);
						} else {
							Profile.HorizontalVPer = std::max(0.0f, Profile.HorizontalVPer - scrollStep);
						}
					} else if (canScrollVert) {
						float scrollStep = 90.0f / static_cast<float>(Profile.ViewSpace.Y - Profile.Size.Y);
						if (Message.MouseWheelUp) {
							Profile.VerticalVPer = std::min(1.0f, Profile.VerticalVPer + scrollStep);
						} else {
							Profile.VerticalVPer = std::max(0.0f, Profile.VerticalVPer - scrollStep);
						}
					}
					Message.Processed = true;
				}
			}
		}
	}

	auto verticalViewButtonRect   = CalVertScrollButton(Profile);
	auto horizontalViewButtonRect = CalHorScrollButton(Profile);

	auto painter = CurrentPainter();
	painter->Begin();

	if (scroller->Painter != nullptr) {
		painter->DrawPainter(scroller->Painter, Profile.Where);
	}

	if (horizontalViewButtonRect.CalWidth() > 0) {
		RenderScrollerButton(Profile.HorizontalButton, horizontalViewButtonRect, painter);
	}
	if (verticalViewButtonRect.CalHeight() > 0) {
		RenderScrollerButton(Profile.VerticalButton, verticalViewButtonRect, painter);
	}

	painter->End();
	if (scroller->Painter != nullptr) {
		context.CurrentWindow->PendingSubPainters.push_back(scroller->Painter);
	}
	delete scroller;

	if (!context.LayoutStack.empty()) {
		context.LayoutStack.pop_back();
	}

	LayoutNextControl(Profile.Size);
}
}
