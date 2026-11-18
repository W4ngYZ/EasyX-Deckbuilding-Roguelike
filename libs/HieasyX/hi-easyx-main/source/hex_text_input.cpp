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
 * \file hex_text_input.cpp
 * \brief The text input for the HiEasyX
 */

#include <include/hex_text_input.h>
#include <include/hex_pool.h>
#include <algorithm>

#undef max

namespace HX {
bool HXIMEPendingStartComposition();
////////////////////////////////////////////////////////////
///        The following:
///				Rendering related API

inline void RenderTextInputRectangle(HXPoint Where, HXBufferPainter *Painter, TextInputProfile &Profile) {
	auto &theme = GetTheme();

	Painter->Begin();
	Painter->DrawFilledRoundedRectangle({Where.X, Where.Y, Where.X + Profile.Size.X, Where.Y + Profile.Size.Y},
	                             theme.TextInputBackground, theme.TextInputBackground, theme.RadiusMd);
	Painter->End();
}

inline void RenderTextInputFrame(HXPoint Where, HXBufferPainter *Painter, TextInputProfile &Profile) {
	auto &theme = GetTheme();

	Painter->Begin();
	auto borderColor = Profile.OnFocus ? theme.SelectedBorder : theme.TextInputBorder;
	int borderW = Profile.OnFocus ? 2 : 1;
	Painter->DrawFilledRoundedRectangle({Where.X, Where.Y, Where.X + Profile.Size.X, Where.Y + Profile.Size.Y},
	                       borderColor, {0,0,0,0}, theme.RadiusMd);
	if (Profile.OnFocus && borderW > 1) {
		Painter->DrawFilledRoundedRectangle({Where.X + 1, Where.Y + 1, Where.X + Profile.Size.X - 1, Where.Y + Profile.Size.Y - 1},
						   borderColor, {0,0,0,0}, std::max(0, theme.RadiusMd - 1));
	}
	Painter->End();
}

inline void RenderTextSelectionRectangle(HXPoint Offset, HXBufferPainter *Painter, TextInputProfile &Profile) {
	auto leftPadding = Profile.PaddingX;

	// If there dose not exists any selection, just stop
	if (Profile.Cursors.size() <= 1) {
		return;
	}
	if (Profile.Cursors[0].Position == Profile.Cursors[1].Position) {
		return;
	}

	auto &theme = GetTheme();

	HXFont defaultFont{};

	// Simply, we just assume it that there only one selection block
	// And the position of the cursor was sorted

	// Step 1) Search the selected para and where the para begin
	// and building a height map for each para

	// std::tuple<HXGInt, HXGInt, HXGInt> for : [Para Count] [Index] [Content String] [Offset]
	std::vector<std::tuple<HXGInt, HXGInt, HXString, HXGInt> > selectionBlock;
	HXGInt                                                     lastPara  = 0;
	HXGInt                                                     paraIndex = 0;
	// Count for the per line characters, for judging the start
	HXGInt   paraLineCount = 0;
	HXString paraString;
	for (auto index = 0; index < Profile.Text.size(); ++index) {
		paraString += Profile.Text[index];
		// What we can say is that, when it comes to here code, there shouldn't be any situation beyond this
		if (index + 1 == Profile.Cursors[1].Position || index + 1 == Profile.Text.size()) {
			// If Pos[0] and Pos[1] in the same para
			if (Profile.Cursors[0].Position > lastPara) {
				// It just only the part of the para
				auto offsetString = paraString.substr(0, Profile.Cursors[0].Position - lastPara);
				auto lengthString = paraString.substr(Profile.Cursors[0].Position - lastPara,
				                                      abs(Profile.Cursors[1].Position - Profile.Cursors[0].Position));
				auto offset = Painter->MeasureText(offsetString, defaultFont, Profile.FontSize).CalWidth();
				selectionBlock.insert(selectionBlock.begin(), {paraIndex, lastPara, lengthString, offset});
			} else {
				auto lengthString = paraString;
				selectionBlock.insert(selectionBlock.begin(),
				                      {paraIndex, Profile.Cursors[1].Position - 1, lengthString, 0});
			}

			break;
		}
		if (Profile.Text[index] == '\n') {
			if (index > Profile.Cursors[0].Position) {
				// It is the whole para
				if (index - paraLineCount > Profile.Cursors[0].Position) {
					selectionBlock.insert(selectionBlock.begin(), {paraIndex, lastPara, paraString, 0});
				} else {
					// It just only the part of the para
					auto offsetString = paraString.substr(0, Profile.Cursors[0].Position - lastPara);
					auto lengthString = Profile.Text.substr(Profile.Cursors[0].Position,
					                                        index - Profile.Cursors[0].Position);
					auto offset = Painter->MeasureText(offsetString, defaultFont, Profile.FontSize).CalWidth();
					selectionBlock.insert(selectionBlock.begin(), {paraIndex, lastPara, lengthString, offset});
				}
			}

			lastPara      = index + 1;
			paraLineCount = 0;

			++paraIndex;
			paraString.clear();

			continue;
		}

		++paraLineCount;
	}

	// Step 2) Drawing the block sets according to the calculated value
	for
	(
		auto &para : selectionBlock
	) {
		// Every coord calculation should include the offset to use the scroll

		// The top calculation formula is:
		//    paraIndex * fontHeight + (paraIndex - 1) * lineGap
		auto top = std::get<0>(para) * Profile.FontSize + (std::get<0>(para) - 1) * Profile.LineGap + Offset.Y;
		// The left calculation formula is:
		//	  leftPadding + virtualCanvasOffset + paraInlineOffset
		auto left = leftPadding + Offset.X + std::get<3>(para);
		// The right calculation formula is:
		//	  left + paraTextWidth - paraInlineOffset
		auto right = left + Painter->MeasureText(std::get<2>(para), defaultFont, Profile.FontSize).CalWidth();
		// The bottom calculation formula is:
		//	  top + fontHeight
		auto bottom = top + Profile.FontSize;

		// Draw the selection rectangle
		Painter->DrawFilledRectangle({left, top, right, bottom}, theme.TextInputSelectedBackground,
		                             theme.TextInputSelectedBackground);
	}
}

/**
 * Measure the text block(The actual text)'s size
 */
HXPoint MeasureTextBlock(HXBufferPainter *Painter, TextInputProfile &Profile) {
	HXPoint  size{0, 0};
	HXGInt   paraIndex = 0;
	HXGInt   lastPara  = 0;
	HXFont   defaultFont{};
	HXString paraString;

	HXGInt cursorPos = 0;
	if (!Profile.Cursors.empty()) {
		cursorPos = Profile.Cursors.back().Position;
	}

	for (auto index = 0; index < Profile.Text.size(); ++index) {
		paraString += Profile.Text[index];
		if (Profile.Text[index] == '\n' || index + 1 == Profile.Text.size()) {
			auto paraWidth = Painter->MeasureText(paraString, defaultFont, Profile.FontSize).CalWidth();

			// If cursor is inside this paragraph and we have an active IME composition,
			// the visual width includes the composition string inserted at cursor.
			if (Profile.InIMEComposition && !Profile.IMECompositionString.empty()
			    && cursorPos >= lastPara && cursorPos <= index + 1) {
				HXString beforeCursor = paraString.substr(0, cursorPos - lastPara);
				HXString afterCursor  = paraString.substr(cursorPos - lastPara);
				auto beforeWidth = Painter->MeasureText(beforeCursor, defaultFont, Profile.FontSize).CalWidth();
				auto compWidth   = Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
				auto afterWidth  = Painter->MeasureText(afterCursor, defaultFont, Profile.FontSize).CalWidth();
				paraWidth = beforeWidth + compWidth + afterWidth;
			}

			size.X = std::max(paraWidth, size.X);
			++paraIndex;
			lastPara = index + 1;
			paraString.clear();
			continue;
		}
	}

	// Empty text with IME composition
	if (Profile.Text.empty() && Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
		size.X = Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
	}

	// Height = paraCount*fontHeight + (paraCount - 1) * lineGap
	size.Y = paraIndex * Profile.FontSize + (paraIndex - 1) * Profile.LineGap;
	return size;
}

inline void RenderTextAndCursor(HXPoint Offset, HXBufferPainter *Painter, TextInputProfile &Profile) {
	auto leftPadding = Profile.PaddingX;
	auto &theme = GetTheme();
	HXFont defaultFont{};

	// Determine cursor position
	HXGInt cursorPosition = 0;
	if (!Profile.Cursors.empty() && Profile.OnFocus) {
		cursorPosition = Profile.Cursors.back().Position;
		if (Profile.Cursors[0].Position != Profile.Cursors[1].Position) {
			cursorPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
			                     ? Profile.Cursors[1].Position
			                     : Profile.Cursors[0].Position;
		}
	}

	// Cursor blink
	if (clock() - Profile.CursorClock > Profile.CursorBlinkMs) {
		Profile.DisplayCursor = !Profile.DisplayCursor;
		Profile.CursorClock   = clock();
	}

	HXGInt baseLine      = Offset.Y;
	HXGInt lastParaStart = 0;

	for (auto index = 0; index <= Profile.Text.size(); ++index) {
		bool isNewLine = (index < Profile.Text.size() && Profile.Text[index] == '\n');
		bool isEnd     = (index == Profile.Text.size());

		if (isNewLine || isEnd) {
			// This paragraph spans [lastParaStart, index)
			HXString para = Profile.Text.substr(lastParaStart, index - lastParaStart);
			bool cursorInPara = (cursorPosition >= lastParaStart && cursorPosition <= index);

			if (baseLine + Profile.FontSize >= 0 && baseLine <= Profile.Size.Y) {
				if (cursorInPara && Profile.OnFocus && Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
					// Split paragraph at cursor: before | comp | after
					HXString beforeCursor = para.substr(0, cursorPosition - lastParaStart);
					HXString afterCursor  = para.substr(cursorPosition - lastParaStart);

					HXGInt drawX = leftPadding + Offset.X;

					// Draw text before cursor
					if (!beforeCursor.empty()) {
						Painter->DrawText(beforeCursor, defaultFont, {drawX, baseLine}, theme.TextInputText, Profile.FontSize);
						drawX += Painter->MeasureText(beforeCursor, defaultFont, Profile.FontSize).CalWidth();
					}

					// Draw IME composition string with underline
					Painter->DrawText(Profile.IMECompositionString, defaultFont, {drawX, baseLine}, theme.TextInputText, Profile.FontSize);
					auto compWidth = Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
					Painter->DrawLine({drawX, baseLine + Profile.FontSize}, {drawX + compWidth, baseLine + Profile.FontSize}, theme.TextInputText);

					// Draw text after cursor (pushed to the right by comp)
					drawX += compWidth;
					if (!afterCursor.empty()) {
						Painter->DrawText(afterCursor, defaultFont, {drawX, baseLine}, theme.TextInputText, Profile.FontSize);
					}

					// Cursor at end of composition string
					if (Profile.DisplayCursor && !Profile.ReadOnly) {
						Painter->DrawLine({drawX, baseLine}, {drawX, baseLine + Profile.FontSize}, theme.TextInputCursorColor);
					}
				} else {
					// Normal draw
					if (!para.empty()) {
						Painter->DrawText(para, defaultFont, {leftPadding + Offset.X, baseLine}, theme.TextInputText, Profile.FontSize);
					}

					// Draw cursor if inside this paragraph (beginning, middle, or end)
					if (Profile.OnFocus && Profile.DisplayCursor && !Profile.ReadOnly && cursorInPara) {
						if (cursorPosition == index) {
							// End of paragraph
							auto paraWidth = Painter->MeasureText(para, defaultFont, Profile.FontSize).CalWidth();
							auto cursorX   = leftPadding + Offset.X + paraWidth;
							Painter->DrawLine({cursorX, baseLine}, {cursorX, baseLine + Profile.FontSize}, theme.TextInputCursorColor);
						} else if (cursorPosition == lastParaStart) {
							// Beginning of paragraph
							Painter->DrawLine({leftPadding + Offset.X, baseLine}, {leftPadding + Offset.X, baseLine + Profile.FontSize}, theme.TextInputCursorColor);
						} else {
							// Middle of paragraph
							HXString beforeCursor = para.substr(0, cursorPosition - lastParaStart);
							auto beforeWidth = Painter->MeasureText(beforeCursor, defaultFont, Profile.FontSize).CalWidth();
							auto cursorX = leftPadding + Offset.X + beforeWidth;
							Painter->DrawLine({cursorX, baseLine}, {cursorX, baseLine + Profile.FontSize}, theme.TextInputCursorColor);
						}
					}
				}
			}

			if (baseLine > Profile.Size.Y) {
				break;
			}

			if (isNewLine) {
				baseLine      += Profile.FontSize + Profile.LineGap;
				lastParaStart  = index + 1;
			}
		}
	}

	// Special case: completely empty text
	if (Profile.Text.empty() && Profile.OnFocus) {
		if (Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
			auto drawX = leftPadding + Offset.X;
			Painter->DrawText(Profile.IMECompositionString, defaultFont, {drawX, baseLine}, theme.TextInputText, Profile.FontSize);
			auto compWidth = Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
			Painter->DrawLine({drawX, baseLine + Profile.FontSize}, {drawX + compWidth, baseLine + Profile.FontSize}, theme.TextInputText);
			if (Profile.DisplayCursor && !Profile.ReadOnly) {
				Painter->DrawLine({drawX + compWidth, baseLine}, {drawX + compWidth, baseLine + Profile.FontSize}, theme.TextInputCursorColor);
			}
		} else if (Profile.DisplayCursor && !Profile.ReadOnly) {
			Painter->DrawLine({leftPadding + Offset.X, baseLine}, {leftPadding + Offset.X, baseLine + Profile.FontSize}, theme.TextInputCursorColor);
		}
	}
}

/**
 * Simply, we just created a canvas which just contained the text content
 */
void RenderVirtualCanvasText(HXPoint           TextBlockSize, HXPoint Where, HXBufferPainter *Painter,
                             TextInputProfile &Profile) {
	auto  subPainter = GetSubPainterPool().Acquire(Profile.Size.X, Profile.Size.Y);
	if (!subPainter) {
		return;
	}
	auto &theme      = GetTheme();

	subPainter->Begin();
	subPainter->Clear(theme.TextInputBackground);

	HXPoint offset{
		static_cast<HXGInt>(-(TextBlockSize.X - Profile.Size.X / 2) * Profile.HorizontalVPer),
		static_cast<HXGInt>(-(TextBlockSize.Y - Profile.Size.Y / 2) * Profile.VerticalVPer)};
	RenderTextSelectionRectangle(offset, subPainter, Profile);
	RenderTextAndCursor(offset, subPainter, Profile);

	subPainter->End();

	Painter->Begin();
	Painter->DrawPainter(subPainter, Where);
	Painter->End();

	// Defer release until after the parent window's draw list has been flushed
	GetContext().CurrentWindow->PendingSubPainters.push_back(subPainter);
}

HXRect CalHorScrollButton(TextInputProfile &Profile, HXPoint ViewSpace) {
	if (ViewSpace.X < Profile.Size.X) {
		return {0, 0, 0, 0};
	}
	auto horizontalViewButtonWidth = static_cast<HXGInt>(
		(static_cast<float>(Profile.Size.X) / static_cast<float>(ViewSpace.X)) *
		static_cast<float>(Profile.Size.X));
	auto horizontalViewButtonRight = static_cast<HXGInt>(
		Profile.HorizontalVPer * static_cast<float>(Profile.Size.X - horizontalViewButtonWidth));
	auto horizontalViewButtonRect = HXRect{horizontalViewButtonRight,
	                                       Profile.Size.Y - 10, horizontalViewButtonRight + horizontalViewButtonWidth,
	                                       Profile.Size.Y};
	horizontalViewButtonRect.Left += Profile.Where.X;
	horizontalViewButtonRect.Top += Profile.Where.Y;
	horizontalViewButtonRect.Right += Profile.Where.X;
	horizontalViewButtonRect.Bottom += Profile.Where.Y;

	if (Profile.Size.X == ViewSpace.X) {
		horizontalViewButtonRect = {Profile.Where.X, Profile.Where.Y, Profile.Where.X, 10};
	}

	return horizontalViewButtonRect;
}

HXRect CalVertScrollButton(TextInputProfile &Profile, HXPoint ViewSpace) {
	if (ViewSpace.Y < Profile.Size.Y) {
		return {0, 0, 0, 0};
	}

	auto verticalViewButtonHeight = static_cast<HXGInt>(
		(static_cast<float>(Profile.Size.Y) / static_cast<float>(ViewSpace.Y)) *
		static_cast<float>(Profile.Size.Y));
	auto verticalViewButtonTop = static_cast<HXGInt>(
		Profile.VerticalVPer * static_cast<float>(Profile.Size.Y - verticalViewButtonHeight));
	auto verticalViewButtonRect = HXRect{Profile.Size.X - 10,
	                                     verticalViewButtonTop, Profile.Size.X,
	                                     verticalViewButtonTop + verticalViewButtonHeight};
	verticalViewButtonRect.Left += Profile.Where.X;
	verticalViewButtonRect.Top += Profile.Where.Y;
	verticalViewButtonRect.Right += Profile.Where.X;
	verticalViewButtonRect.Bottom += Profile.Where.Y;

	if (Profile.Size.Y == ViewSpace.Y) {
		verticalViewButtonRect = {Profile.Where.X, Profile.Where.Y, Profile.Where.X + 10, Profile.Where.Y};
	}

	return verticalViewButtonRect;
}

void RenderScrollerButton(TextInputScrollButtonProfile &Profile, HXRect Rect, HXBufferPainter *Painter) {
	auto &theme = GetTheme();

	Painter->Begin();
	if (Profile.InDrag) {
		Painter->DrawFilledRoundedRectangle(Rect, theme.ButtonPressedBorder, theme.ButtonPressedBackground, theme.RadiusSm);
	} else if (Profile.OnHover) {
		Painter->DrawFilledRoundedRectangle(Rect, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground, theme.RadiusSm);
	} else {
		Painter->DrawFilledRoundedRectangle(Rect, theme.ButtonBorder, theme.ButtonBackground, theme.RadiusSm);
	}
	Painter->End();
}

void RenderScroller(TextInputProfile &Profile, HXBufferPainter *Painter, HXPoint TextBlock) {
	auto verticalRectangle = CalVertScrollButton(Profile, TextBlock);
	RenderScrollerButton(Profile.VerticalButton, verticalRectangle, Painter);
	auto horizontalRectangle = CalHorScrollButton(Profile, TextBlock);
	RenderScrollerButton(Profile.HorizontalButton, horizontalRectangle, Painter);
}

void RenderAndLayoutTextInput(HXBufferPainter *Painter, TextInputProfile &Profile, HXPoint Block) {
	auto &theme   = GetTheme();
	auto &context = GetContext();

	if (RoughVisibility()) {
		return;
	}
	if (AccVisibility(Profile.Size)) {
		return;
	}

	auto position = CurrentLayoutPosition();

	Profile.Where = position;

	RenderTextInputRectangle(position, Painter, Profile);
	RenderVirtualCanvasText(Block, position, Painter, Profile);
	RenderScroller(Profile, Painter, Block);
	RenderTextInputFrame(position, Painter, Profile);

	LayoutNextControl(Profile.Size);
}

////////////////////////////////////////////////////////////
///        The following:
///				Logic related API

enum class ButtonType {
	Vertical, Horizontal
};

float ProcessButtonMessage(TextInputScrollButtonProfile &Profile, HXRect Button, ButtonType Type, HXRect Scroller) {
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

void ProcessScrollButtonMessage(TextInputProfile &Profile, HXPoint TextBlock) {
	auto verticalRectangle   = CalVertScrollButton(Profile, TextBlock);
	auto horizontalRectangle = CalHorScrollButton(Profile, TextBlock);
	auto scrollerRect        = HXRect{Profile.Where.X, Profile.Where.Y, Profile.Where.X + Profile.Size.X,
	                           Profile.Where.Y + Profile.Size.Y};

	Profile.VerticalVPer = ProcessButtonMessage(Profile.VerticalButton, verticalRectangle, ButtonType::Vertical,
	                                            scrollerRect);
	Profile.HorizontalVPer = ProcessButtonMessage(Profile.HorizontalButton, horizontalRectangle,
	                                              ButtonType::Horizontal, scrollerRect);
}

/**
 * Converting the mouse coord to the position, using at the mouse selection
 */
HXGInt MouseCoordToTextPosition(TextInputProfile &Profile, HXPoint Offset, HXPoint Mouse, HXBufferPainter *Painter) {
	auto leftPadding = Profile.PaddingX;

	Mouse.X -= Profile.Where.X;
	Mouse.Y -= Profile.Where.Y;

	HXFont defaultFont{};

	// We assume that every mouse coord are naturally clicked by user, so we will just skip the unused paragraph

	HXString paraString;
	HXGInt   baseLine = Offset.Y;
	HXGInt   lastPara = 0;
	for (auto index = 0; index < Profile.Text.size(); ++index) {
		paraString += Profile.Text[index];
		if (Profile.Text[index] == '\n' || index + 1 == Profile.Text.size()) {
			// Here is the clicked one para
			if (Mouse.Y <= baseLine + Profile.FontSize + Profile.LineGap && Mouse.Y >= baseLine) {
				// Test in the para
				HXGInt widthSum = leftPadding + Offset.X;
				for (auto count = lastPara; count < index; ++count) {
					auto width = Painter->MeasureText(Profile.Text[count], defaultFont, Profile.FontSize).CalWidth();
					widthSum += width;

					// We just pick the former one
					if (widthSum >= Mouse.X) {
						if (widthSum - Mouse.X < width - (widthSum - Mouse.X)) {
							return count + 1;
						} else {
							return count;
						}
					}
				}

				// If the cursor reached the end of the para, return the para end
				if (index + 1 == Profile.Text.size()) {
					return index + 1;
				} else {
					return index;
				}
			}
			if (baseLine > Profile.Size.Y) {
				break;
			}

			paraString.clear();

			// TextHeight + LineGap
			baseLine += Profile.FontSize + Profile.LineGap;
			lastPara = index + 1;

			continue;
		}
	}

	return Profile.Text.size();
}

void ProcessMouseMessage(TextInputProfile &Profile, HXPoint Offset, HXPoint TextBlock, HXBufferPainter *Painter) {
	auto &context       = GetContext();
	auto  position      = CurrentLayoutPosition();
	auto  editRectangle = HXRect{position.X, position.Y, position.X + Profile.Size.X, position.Y + Profile.Size.Y};

	for (auto &Message : context.MessageQuery) {
		auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
		if (Message.Processed) {
			continue;
		}

		if (!Message.MouseAction) {
			continue;
		}

		if (Message.MouseWheel && Profile.OnFocus && editRectangle.PointInside(mouse) && TextBlock.Y > Profile.Size.Y) {
			Profile.VerticalVPer += (Message.MouseWheelUp ? 1 : -1) * 0.06;
			if (Profile.VerticalVPer > 1.0f) {
				Profile.VerticalVPer = 1.0f;
			}
			if (Profile.VerticalVPer < 0.0f) {
				Profile.VerticalVPer = 0.0f;
			}
		}
		if (Profile.InDrag && !Message.MouseLeftRelease) {
			Profile.Cursors[0] = {Profile.SelectingPoint};
			Profile.Cursors[1] = {MouseCoordToTextPosition(Profile, Offset, mouse, Painter)};

			std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
				return a.Position < b.Position;
			});
		}
		if (editRectangle.PointInside(mouse)) {
			Profile.MouseStyle = true;
			context.FrameCursor = HXCursorStyle::Editing;
			if (Message.MouseLeftPressed) {
				int clickPos = MouseCoordToTextPosition(Profile, Offset, mouse, Painter);
				clock_t now = clock();
				bool isDoubleClick = (now - Profile.LastClickTime < Profile.CursorBlinkMs) &&
				                     (std::abs(clickPos - Profile.LastClickPos) <= 2);

				if (isDoubleClick) {
					// Double-click: select word
					auto isWordChar = [](wchar_t c) {
						return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
						       (c >= L'0' && c <= L'9') || c == L'_';
					};
					int start = clickPos;
					int end = clickPos;
					if (clickPos < static_cast<int>(Profile.Text.size()) && !isWordChar(Profile.Text[clickPos])) {
						start = clickPos;
						end = clickPos + 1;
					} else {
						while (start > 0 && isWordChar(Profile.Text[start - 1])) --start;
						while (end < static_cast<int>(Profile.Text.size()) && isWordChar(Profile.Text[end])) ++end;
					}
					Profile.Cursors.clear();
					Profile.Cursors.push_back({start});
					Profile.Cursors.push_back({end});
					Profile.SelectingPoint = start;
					Profile.OnFocus = true;
					context.FocusedProfile = &Profile;
					Profile.DisplayCursor = true;
					Profile.CursorClock = clock();
					Profile.IMELastRect = {0, 0, 0, 0};
					Profile.InDrag = false;
					Message.Processed = true;
					Profile.LastClickTime = now;
					Profile.LastClickPos = clickPos;
					continue;
				}

				Profile.LastClickTime = now;
				Profile.LastClickPos = clickPos;

				// Mouse selecting
				if (Message.InShift && !Profile.Cursors.empty()) {
					Profile.Cursors[0] = {Profile.SelectingPoint};
					Profile.Cursors[1] = {clickPos};

					std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
						return a.Position < b.Position;
					});
				} else {
					Profile.Cursors.clear();

					Message.Processed = true;

					Profile.OnFocus        = true;
					context.FocusedProfile = &Profile;
					Profile.InDrag         = true;
					Profile.DisplayCursor  = true;
					Profile.CursorClock    = clock();
					Profile.SelectingPoint = clickPos;
					Profile.IMELastRect    = {0, 0, 0, 0};

					Profile.Cursors.push_back({Profile.SelectingPoint});
					Profile.Cursors.push_back({Profile.SelectingPoint});
				}
			}
			if (Message.MouseLeftRelease) {
				Profile.InDrag = false;
			}
		} else {
			if (Profile.MouseStyle) {
				Profile.MouseStyle = false;
			}
			if (Message.MouseLeftPressed) {
				Profile.Cursors.clear();
				if (Profile.OnFocus && Profile.InIMEComposition) {
					context.OSAPI->CancelIMEComposition();
					Profile.InIMEComposition = false;
					Profile.IMECompositionString.clear();
				}
				Profile.OnFocus = false;
				Profile.IMELastRect = {0, 0, 0, 0};
			} else if (Message.MouseLeftRelease) {
				Profile.InDrag = false;
			}
		}
	}
}

inline HXGInt MoveToHome(TextInputProfile &Profile, HXGInt CursorPosition) {
	HXGInt lastPara = 0;
	for (auto index = 0; index < Profile.Text.size(); ++index) {
		if (index + 1 == CursorPosition) {
			if (Profile.Text[index] == '\n') {
				return CursorPosition;
			}

			if (lastPara != 0) {
				return lastPara + 1;
			} else {
				return 0;
			}
		}
		if (Profile.Text[index] == '\n' || index + 1 == Profile.Text.size()) {
			lastPara = index;
		}
	}

	return CursorPosition;
}

inline HXGInt MoveToEnd(TextInputProfile &Profile, HXGInt CursorPosition) {
	for (auto index = 0; index < Profile.Text.size(); ++index) {
		if (Profile.Text[index] == '\n' || index + 1 == Profile.Text.size()) {
			if (index + 1 == Profile.Text.size()) {
				return index + 1;
			}
			if (index >= CursorPosition) {
				return index;
			}
		}
	}

	return Profile.Text.size();
}

inline HXGInt MoveUpDown(TextInputProfile &Profile, HXGInt CursorPosition, bool Up) {
	HXGInt cursorParaLength = 0;
	HXGInt cursorPara       = 0;
	HXGInt lastPara         = 0;
	HXGInt paraCount        = 0;
	HXGInt paraLengthCount  = 1;
	for (auto index = 0; index < Profile.Text.size(); ++index) {
		if (index + 1 == CursorPosition) {
			if (Profile.Text[index] == '\n') {
				cursorParaLength = 0;
				cursorPara       = paraCount + 1;
			}
			else {
				cursorParaLength = paraLengthCount;
				cursorPara       = paraCount;
			}

			break;
		}
		if (Profile.Text[index] == '\n' || index + 1 == Profile.Text.size()) {
			++paraCount;

			paraLengthCount = 1;

			continue;
		}

		++paraLengthCount;
	}

	lastPara              = 0;
	paraCount             = 0;
	HXGInt lastParaLength = 0;
	for (auto index = 0; index < Profile.Text.size(); ++index) {
		if (Profile.Text[index] == '\n' || index + 1 == Profile.Text.size()) {
			// Fill the missing character
			if (index + 1 == Profile.Text.size()) {
				++lastParaLength;
			}
			if ((Up && paraCount == cursorPara - 1) || (!Up && paraCount == cursorPara + 1)) {
				return lastPara + (cursorParaLength > lastParaLength ? lastParaLength : cursorParaLength);
			}

			++paraCount;
			lastPara       = index + 1;
			lastParaLength = 0;

			continue;
		}

		++lastParaLength;
	}

	return CursorPosition;
}

void ProcessKeyMessage(TextInputProfile &Profile) {
	auto &context  = GetContext();
	auto  position = CurrentLayoutPosition();

	if (!Profile.OnFocus) {
		return;
	}

	for (auto &Message : context.MessageQuery) {
		auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
		if (Message.Processed) {
			continue;
		}

		if (Message.KeyPressed) {
			Message.Processed = true;

			// During IME composition, let the IME handle all keyboard input.
			// The IME already received the raw WM_KEYDOWN via DefWindowProc.
			if (Profile.InIMEComposition) {
				continue;
			}

			// Back Delete content
			if (Message.KeyCode == VK_BACK) {
				if (Profile.ReadOnly) continue;
				if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
					Profile.Text.erase(Profile.Text.begin() + Profile.Cursors[0].Position,
					                   Profile.Text.begin() + Profile.Cursors[1].Position);
					Profile.Cursors[1] = Profile.Cursors[0];
				} else if (Profile.Text.begin() + Profile.Cursors[0].Position != Profile.Text.begin()) {
					Profile.Text.erase(Profile.Text.begin() + Profile.Cursors[0].Position - 1,
					                   Profile.Text.begin() + Profile.Cursors[0].Position);
					--Profile.Cursors[0].Position;
					--Profile.Cursors[1].Position;
				}
			}
			// Delete content
			if (Message.KeyCode == VK_DELETE) {
				if (Profile.ReadOnly) continue;
				if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
					Profile.Text.erase(Profile.Text.begin() + Profile.Cursors[0].Position,
					                   Profile.Text.begin() + Profile.Cursors[1].Position);
					Profile.Cursors[1] = Profile.Cursors[0];
				} else if (Profile.Cursors[0].Position < Profile.Text.size()) {
					Profile.Text.erase(Profile.Text.begin() + Profile.Cursors[0].Position,
					                   Profile.Text.begin() + Profile.Cursors[0].Position + 1);
				}
			}
			// Left select or move
			if (Message.KeyCode == VK_LEFT) {
				Profile.CursorClock   = clock();
				Profile.DisplayCursor = true;
				if (Message.InShift) {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPoint = Profile.Cursors[0].Position == Profile.SelectingPoint
							                   ? Profile.Cursors[1].Position
							                   : Profile.Cursors[0].Position;
						movingPoint -= 1;
						Profile.Cursors[0].Position = movingPoint < 0 ? 0 : movingPoint;
						Profile.Cursors[1].Position = Profile.SelectingPoint;
					} else {
						Profile.SelectingPoint = Profile.Cursors[0].Position;
						Profile.Cursors[0].Position -= 1;
						if (Profile.Cursors[0].Position < 0) {
							Profile.Cursors[0].Position = 0;
						}
					}

					std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
						return a.Position < b.Position;
					});
				} else {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = movingPosition - 1;
						newPosition      = newPosition < 0 ? 0 : newPosition;

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					} else {
						auto newPosition = Profile.Cursors[0].Position - 1;
						newPosition      = newPosition < 0 ? 0 : newPosition;

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					}
				}
			}
			// Right select or move
			if (Message.KeyCode == VK_RIGHT) {
				Profile.CursorClock   = clock();
				Profile.DisplayCursor = true;
				if (Message.InShift) {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPoint = Profile.Cursors[0].Position == Profile.SelectingPoint
							                   ? Profile.Cursors[1].Position
							                   : Profile.Cursors[0].Position;
						movingPoint += 1;
						Profile.Cursors[0].Position = movingPoint > Profile.Text.size() + 1
							                              ? Profile.Text.size() + 1
							                              : movingPoint;
						Profile.Cursors[1].Position = Profile.SelectingPoint;
					} else {
						Profile.SelectingPoint = Profile.Cursors[0].Position;
						Profile.Cursors[0].Position += 1;
						if (Profile.Cursors[0].Position > Profile.Text.size()) {
							Profile.Cursors[0].Position = Profile.Text.size();
						}
					}

					std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
						return a.Position < b.Position;
					});
				} else {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = movingPosition + 1;
						newPosition = newPosition > Profile.Text.size() + 1 ? Profile.Text.size() + 1 : newPosition;

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					} else {
						auto newPosition = Profile.Cursors[0].Position + 1;
						newPosition = newPosition > Profile.Text.size() + 1 ? Profile.Text.size() + 1 : newPosition;

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					}
				}
			}
			// Down select or move
			if (Message.KeyCode == VK_DOWN) {
				Profile.CursorClock   = clock();
				Profile.DisplayCursor = true;

				if (Message.InShift) {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveUpDown(Profile, movingPosition, false);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = Profile.SelectingPoint;

					} else {
						auto newPosition = MoveUpDown(Profile, Profile.Cursors[0].Position, false);

						Profile.Cursors[0].Position = newPosition;
						Profile.SelectingPoint      = Profile.Cursors[1].Position;
					}

					std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
						return a.Position < b.Position;
					});
				} else {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveUpDown(Profile, movingPosition, false);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					} else {
						auto newPosition = MoveUpDown(Profile, Profile.Cursors[0].Position, false);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					}
				}
			}
			// Up select or move
			if (Message.KeyCode == VK_UP) {
				Profile.CursorClock   = clock();
				Profile.DisplayCursor = true;

				if (Message.InShift) {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveUpDown(Profile, movingPosition, true);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = Profile.SelectingPoint;

					} else {
						auto newPosition = MoveUpDown(Profile, Profile.Cursors[0].Position, true);

						Profile.Cursors[0].Position = newPosition;
						Profile.SelectingPoint      = Profile.Cursors[1].Position;
					}

					std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
						return a.Position < b.Position;
					});
				} else {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveUpDown(Profile, movingPosition, true);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					} else {
						auto newPosition = MoveUpDown(Profile, Profile.Cursors[0].Position, true);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					}
				}
			}
			// Move the cursor to home
			if (Message.KeyCode == VK_HOME) {
				Profile.CursorClock   = clock();
				Profile.DisplayCursor = true;

				if (Message.InShift) {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveToHome(Profile, movingPosition);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = Profile.SelectingPoint;

					} else {
						auto newPosition = MoveToHome(Profile, Profile.Cursors[0].Position);

						Profile.Cursors[0].Position = newPosition;
						Profile.SelectingPoint      = Profile.Cursors[1].Position;
					}

					std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
						return a.Position < b.Position;
					});
				} else {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveToHome(Profile, movingPosition);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					} else {
						auto newPosition = MoveToHome(Profile, Profile.Cursors[0].Position);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					}
				}
			}
			// Move the cursor to end
			if (Message.KeyCode == VK_END) {
				Profile.CursorClock   = clock();
				Profile.DisplayCursor = true;

				if (Message.InShift) {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveToEnd(Profile, movingPosition);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = Profile.SelectingPoint;
					} else {
						auto newPosition = MoveToEnd(Profile, Profile.Cursors[0].Position);

						Profile.Cursors[0].Position = newPosition;
						Profile.SelectingPoint      = Profile.Cursors[1].Position;
					}

					std::sort(Profile.Cursors.begin(), Profile.Cursors.end(), [](Cursor a, Cursor b) {
						return a.Position < b.Position;
					});
				} else {
					if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
						auto movingPosition = Profile.Cursors[0].Position == Profile.SelectingPoint
							                      ? Profile.Cursors[1].Position
							                      : Profile.Cursors[0].Position;
						auto newPosition = MoveToEnd(Profile, movingPosition);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					} else {
						auto newPosition = MoveToEnd(Profile, Profile.Cursors[0].Position);

						Profile.Cursors[0].Position = newPosition;
						Profile.Cursors[1].Position = newPosition;
					}
				}
			}
		}
	}
}

void CopyToClipboard(TextInputProfile &Profile) {
	if (Profile.Cursors[0].Position != Profile.Cursors[1].Position) {
		auto string = Profile.Text.substr(Profile.Cursors[0].Position,
		                                  Profile.Cursors[1].Position - Profile.Cursors[0].Position + 1);
		auto &context = GetContext();
		context.OSAPI->CopyToClipboard(string);
	}
}

HXString ReadFromClipboard() {
	auto &context = GetContext();
	return context.OSAPI->ReadFromClipboard();
}

void ProcessCharacterMessage(TextInputProfile &Profile) {
	auto &context  = GetContext();
	auto  position = CurrentLayoutPosition();

	if (!Profile.OnFocus) {
		return;
	}

	for (auto &Message : context.MessageQuery) {
		auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
		if (Message.Processed) {
			continue;
		}

		if (Message.InputChar) {
			Message.Processed = true;
			// Ctrl + C
			if (Message.Char == '\x3') {
				CopyToClipboard(Profile);
				return;
			}
			// Ctrl + A select all
			if (Message.Char == '\x1') {
				Profile.Cursors[0].Position = 0;
				Profile.Cursors[1].Position = Profile.Text.size();
				Profile.SelectingPoint      = 0;

				return;
			}
			// Ctrl + V
			if (Message.Char == '\x16') {
				if (Profile.ReadOnly) {
					return;
				}
				auto string = ReadFromClipboard();
				if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
					Profile.Text.erase(Profile.Text.begin() + Profile.Cursors[0].Position,
					                   Profile.Text.begin() + Profile.Cursors[1].Position);
					Profile.Cursors[1] = Profile.Cursors[0];
				}

				if (Profile.Cursors[0].Position < Profile.Text.size()) {
					Profile.Text.insert(Profile.Cursors[0].Position, string);
				}
				else {
					Profile.Text += string;
				}
				Profile.Cursors[0].Position += string.size();
				Profile.Cursors[1].Position += string.size();

				return;
			}
			// Suppress normal characters during IME composition or shortly after
			// Also suppress if a composition string is present (handles the case
			// where WM_CHAR arrives before WM_IME_STARTCOMPOSITION is processed).
			if (Profile.InIMEComposition || !Profile.IMECompositionString.empty()) {
				return;
			}
			if (clock() - Profile.LastIMEEndTime < 50) {
				return;
			}
			if (Profile.ReadOnly) {
				return;
			}
			if (Message.Char == '\r') {
				Message.Char = '\n';
			}
			if (Message.Char == '\b') {
				return;
			}
			// Delete selecting text first
			if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
				Profile.Text.erase(Profile.Text.begin() + Profile.Cursors[0].Position,
				                   Profile.Text.begin() + Profile.Cursors[1].Position);
				Profile.Cursors[1] = Profile.Cursors[0];
			}

			Profile.Text.insert(Profile.Text.begin() + Profile.Cursors[0].Position, Message.Char);
			++Profile.Cursors[0].Position;
			++Profile.Cursors[1].Position;
		}
	}
}

HXPoint GetCursorScreenPos(TextInputProfile &Profile, HXPoint Offset, HXBufferPainter *Painter) {
	auto leftPadding = Profile.PaddingX;
	HXFont defaultFont{};

	HXGInt cursorPos = 0;
	if (!Profile.Cursors.empty()) {
		cursorPos = Profile.Cursors.back().Position;
		if (Profile.Cursors[0].Position != Profile.Cursors[1].Position) {
			cursorPos = Profile.Cursors[0].Position == Profile.SelectingPoint
			                ? Profile.Cursors[1].Position
			                : Profile.Cursors[0].Position;
		}
	}

	HXGInt baseLine = Offset.Y;
	HXGInt lastPara = 0;
	HXString paraString;

	if (cursorPos == 0) {
		HXPoint result{leftPadding + Offset.X, baseLine};
		// During IME composition, the caret must be at the END of the composition
		// string so that Microsoft Pinyin can follow it correctly.
		if (Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
			result.X += Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
		}
		return result;
	}

	for (auto index = 0; index < Profile.Text.size(); ++index) {
		paraString += Profile.Text[index];
		if (Profile.Text[index] == '\n' || index + 1 == Profile.Text.size()) {
			if (cursorPos >= lastPara && cursorPos <= index + 1) {
				// Cursor is right after a newline -> it belongs to the next (possibly empty) paragraph
				if (cursorPos == index + 1 && Profile.Text[index] == '\n') {
					HXPoint result{leftPadding + Offset.X, baseLine + Profile.FontSize + Profile.LineGap};
					if (Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
						result.X += Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
					}
					return result;
				}
				auto sub   = paraString.substr(0, cursorPos - lastPara);
				auto width = Painter->MeasureText(sub, defaultFont, Profile.FontSize).CalWidth();
				HXPoint result{leftPadding + Offset.X + width, baseLine};
				// Place caret at the end of the composition string
				if (cursorPos == index + 1 && Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
					result.X += Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
				}
				return result;
			}
			baseLine += Profile.FontSize + Profile.LineGap;
			paraString.clear();
			lastPara = index + 1;
		}
	}

	HXPoint result{leftPadding + Offset.X, baseLine};
	if (Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
		result.X += Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
	}
	return result;
}

void UpdateIMERect(TextInputProfile &Profile, HXPoint Offset, HXBufferPainter *Painter) {
	if (!Profile.OnFocus) {
		return;
	}

	auto pos = GetCursorScreenPos(Profile, Offset, Painter);

	HXFont defaultFont{};
	HXGInt compWidth = 0;
	if (Profile.InIMEComposition && !Profile.IMECompositionString.empty()) {
		compWidth = Painter->MeasureText(Profile.IMECompositionString, defaultFont, Profile.FontSize).CalWidth();
	}

	auto &context = GetContext();

	// Compute absolute client-area coordinates (all in EasyX HWND client space).
	HXGInt absX = Profile.Where.X + pos.X + context.CurrentWindow->ScrollerX + context.CurrentWindow->Where.X;
	HXGInt absY = Profile.Where.Y + pos.Y + context.CurrentWindow->ScrollerY + context.CurrentWindow->Where.Y;

	HXRect imeRect;
	imeRect.Left = absX;
	imeRect.Top = absY;
	imeRect.Right = absX + compWidth;
	imeRect.Bottom = absY + Profile.FontSize;

	// Always update IME position while in composition so the candidate window
	// tracks the cursor even if the rect hasn't changed numerically.
	if (imeRect.Left != Profile.IMELastRect.Left || imeRect.Top != Profile.IMELastRect.Top ||
	    imeRect.Right != Profile.IMELastRect.Right || HX::HXIMEPendingStartComposition() ||
	    Profile.InIMEComposition) {
		context.OSAPI->SetIMERect(imeRect);
		Profile.IMELastRect = imeRect;
	}
}

void ProcessIMEMessage(TextInputProfile &Profile) {
	if (Profile.ReadOnly) return;
	auto &context = GetContext();
	for (auto &Message : context.MessageQuery) {
		if (Message.Processed) {
			continue;
		}

		if (Message.IMEStartComposition) {
			Profile.InIMEComposition = true;
			Profile.IMECompositionString.clear();
			Message.Processed = true;
		}
		if (Message.IMEResult) {
			if (!Message.IMEResultString.empty()) {
				if (Profile.Cursors[1].Position != Profile.Cursors[0].Position) {
					Profile.Text.erase(Profile.Text.begin() + Profile.Cursors[0].Position,
					                   Profile.Text.begin() + Profile.Cursors[1].Position);
					Profile.Cursors[1] = Profile.Cursors[0];
				}
				Profile.Text.insert(Profile.Cursors[0].Position, Message.IMEResultString);
				Profile.Cursors[0].Position += Message.IMEResultString.size();
				Profile.Cursors[1].Position += Message.IMEResultString.size();
			}
			// Receiving result means composition is effectively complete (some IMEs do not send WM_IME_ENDCOMPOSITION)
			Profile.InIMEComposition = false;
			Profile.LastIMEEndTime = clock();
			Profile.IMECompositionString.clear();
			Message.Processed = true;
		}
		if (Message.IMEComposition) {
			Profile.IMECompositionString = Message.IMECompositionString;
			if (!Profile.IMECompositionString.empty()) {
				Profile.InIMEComposition = true;
			}
			Message.Processed = true;
		}
		if (Message.IMEEndComposition) {
			Profile.InIMEComposition = false;
			Profile.LastIMEEndTime = clock();
			Profile.IMECompositionString.clear();
			Message.Processed = true;
		}
	}
}

void ProcessMessage(TextInputProfile &Profile, HXPoint TextBlock, HXBufferPainter *Painter) {
	ProcessScrollButtonMessage(Profile, TextBlock);

	HXPoint offset{
		static_cast<HXGInt>(-(TextBlock.X - Profile.Size.X / 2) * Profile.HorizontalVPer),
		static_cast<HXGInt>(-(TextBlock.Y - Profile.Size.Y / 2) * Profile.VerticalVPer)};

	ProcessMouseMessage(Profile, offset, TextBlock, Painter);
	ProcessKeyMessage(Profile);
	ProcessIMEMessage(Profile);
	ProcessCharacterMessage(Profile);
}


////////////////////////////////////////////////////////////
///        The following:
///				Main API

void TextInput(TextInputProfile &Profile) {
	if (RoughVisibility()) {
		return;
	}
	if (AccVisibility(Profile.Size)) {
		return;
	}
	if (!CurrentVisibility(Profile.Size)) {
		if (NeedLayoutCal()) {
			LayoutNextControl(Profile.Size);
		}

		return;
	}

	auto painter = CurrentPainter();
	auto block   = MeasureTextBlock(painter, Profile);

	ProcessMessage(Profile, block, painter);

	RenderAndLayoutTextInput(painter, Profile, block);

	if (Profile.OnFocus) {
		HXPoint offset{
			static_cast<HXGInt>(-(block.X - Profile.Size.X / 2) * Profile.HorizontalVPer),
			static_cast<HXGInt>(-(block.Y - Profile.Size.Y / 2) * Profile.VerticalVPer)};
		UpdateIMERect(Profile, offset, painter);
	}
}

}
