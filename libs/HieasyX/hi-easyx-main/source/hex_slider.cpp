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
 * \file hex_slider.cpp
 * \brief The value slider control for HiEasyX
 */

#include <include/hex.h>
#include <include/hex_slider.h>

#include <format>

#undef max
#undef min

#include <iostream>

namespace HX {
template <class Type>
void Slider(const HXString &Title, Type &Value, SliderProfile<Type> &Profile) {
	if (RoughVisibility()) {
		return;
	}

	if (Profile.Width == -1) {
		Profile.Width = 300;
	}
	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto painter = CurrentPainter();

	HXGInt height = Profile.Height;

	auto position      = CurrentLayoutPosition();
	auto textRectangle = painter->MeasureText(Title, HXFont{}, theme.FontSizeCaption);
	auto rectangle     = HXRect{position.X, position.Y, position.X + Profile.Width, position.Y + height};

	if (AccVisibility({rectangle.CalWidth() + textRectangle.Right, rectangle.CalHeight()})) {
		return;
	}

	RegisterFocus(&Profile);
	Profile.OnFocus = IsFocused(&Profile);

	// Auto-commit edit when focus is lost
	if (Profile.InEditMode && !Profile.OnFocus) {
		try {
			if constexpr (std::is_same_v<Type, float>) {
				Value = static_cast<Type>(std::stof(Profile.EditText));
			} else {
				Value = static_cast<Type>(std::stoi(Profile.EditText));
			}
			Value = std::max(Profile.MinValue, std::min(Profile.MaxValue, Value));
		} catch (...) {
			// Invalid input, keep old value
		}
		Profile.InEditMode = false;
	}

	// Edit mode input handling
	if (Profile.InEditMode) {
		// Update blink
		if (clock() - Profile.EditBlinkClock > Profile.EditBlinkMs) {
			Profile.EditShowCursor = !Profile.EditShowCursor;
			Profile.EditBlinkClock = clock();
		}

		// Mouse handling for cursor placement / selection
		for (auto &Message : context.MessageQuery) {
			if (Message.Processed) continue;
			if (!Message.MouseAction) continue;

			auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
			if (rectangle.PointInside(mouse)) {
				context.FrameCursor = HXCursorStyle::Editing;
				if (Message.MouseLeftPressed) {
					Message.Processed = true;
					Profile.EditDragging = true;

					int relX = mouse.X - (position.X + 5);
					HXFont defaultFont{};
					int bestPos = 0;
					int bestDist = INT_MAX;
					for (size_t i = 0; i <= Profile.EditText.size(); ++i) {
						auto sub = Profile.EditText.substr(0, i);
						int width = painter->MeasureText(sub, defaultFont, Profile.FontSize).CalWidth();
						int dist = abs(width - relX);
						if (dist < bestDist) {
							bestDist = dist;
							bestPos = static_cast<int>(i);
						}
					}
					Profile.EditCursorPos = bestPos;
					Profile.EditSelectStart = Profile.EditCursorPos;
					Profile.EditSelectEnd = Profile.EditCursorPos;
				}
				if (Profile.EditDragging && !Message.MouseLeftRelease) {
					Message.Processed = true;
					int relX = mouse.X - (position.X + 5);
					HXFont defaultFont{};
					int bestPos = 0;
					int bestDist = INT_MAX;
					for (size_t i = 0; i <= Profile.EditText.size(); ++i) {
						auto sub = Profile.EditText.substr(0, i);
						int width = painter->MeasureText(sub, defaultFont, Profile.FontSize).CalWidth();
						int dist = abs(width - relX);
						if (dist < bestDist) {
							bestDist = dist;
							bestPos = static_cast<int>(i);
						}
					}
					Profile.EditCursorPos = bestPos;
					Profile.EditSelectEnd = bestPos;
				}
				if (Message.MouseLeftRelease) {
					Profile.EditDragging = false;
				}
			} else {
				if (Message.MouseLeftPressed) {
					Message.Processed = true;
					try {
						if constexpr (std::is_same_v<Type, float>) {
							Value = static_cast<Type>(std::stof(Profile.EditText));
						} else {
							Value = static_cast<Type>(std::stoi(Profile.EditText));
						}
						Value = std::max(Profile.MinValue, std::min(Profile.MaxValue, Value));
					} catch (...) {}
					Profile.InEditMode = false;
				}
			}
		}

		// Keyboard handling
		for (auto &Message : context.MessageQuery) {
			if (Message.Processed) continue;
			if (Message.KeyPressed) {
				bool hasSelection = Profile.EditSelectStart != Profile.EditSelectEnd;
				int selStart = std::min(Profile.EditSelectStart, Profile.EditSelectEnd);
				int selEnd = std::max(Profile.EditSelectStart, Profile.EditSelectEnd);

				auto doDelete = [&](int start, int count) {
					if (start >= 0 && count > 0 && start + count <= static_cast<int>(Profile.EditText.size())) {
						Profile.EditText.erase(start, count);
						Profile.EditCursorPos = start;
						Profile.EditSelectStart = start;
						Profile.EditSelectEnd = start;
					}
				};

				if (Message.KeyCode == VK_LEFT) {
					Message.Processed = true;
					Profile.EditBlinkClock = clock();
					Profile.EditShowCursor = true;
					if (Message.InShift) {
						if (hasSelection) {
							Profile.EditCursorPos = (Profile.EditCursorPos == Profile.EditSelectStart)
								? Profile.EditSelectEnd : Profile.EditSelectStart;
						}
						Profile.EditCursorPos = std::max(0, Profile.EditCursorPos - 1);
						Profile.EditSelectEnd = Profile.EditCursorPos;
					} else {
						if (hasSelection) {
							Profile.EditCursorPos = selStart;
						} else {
							Profile.EditCursorPos = std::max(0, Profile.EditCursorPos - 1);
						}
						Profile.EditSelectStart = Profile.EditCursorPos;
						Profile.EditSelectEnd = Profile.EditCursorPos;
					}
					continue;
				}
				if (Message.KeyCode == VK_RIGHT) {
					Message.Processed = true;
					Profile.EditBlinkClock = clock();
					Profile.EditShowCursor = true;
					if (Message.InShift) {
						if (hasSelection) {
							Profile.EditCursorPos = (Profile.EditCursorPos == Profile.EditSelectStart)
								? Profile.EditSelectEnd : Profile.EditSelectStart;
						}
						Profile.EditCursorPos = std::min(static_cast<int>(Profile.EditText.size()), Profile.EditCursorPos + 1);
						Profile.EditSelectEnd = Profile.EditCursorPos;
					} else {
						if (hasSelection) {
							Profile.EditCursorPos = selEnd;
						} else {
							Profile.EditCursorPos = std::min(static_cast<int>(Profile.EditText.size()), Profile.EditCursorPos + 1);
						}
						Profile.EditSelectStart = Profile.EditCursorPos;
						Profile.EditSelectEnd = Profile.EditCursorPos;
					}
					continue;
				}
				if (Message.KeyCode == VK_HOME) {
					Message.Processed = true;
					Profile.EditBlinkClock = clock();
					Profile.EditShowCursor = true;
					if (Message.InShift) {
						if (!hasSelection) Profile.EditSelectEnd = Profile.EditCursorPos;
						Profile.EditCursorPos = 0;
						Profile.EditSelectStart = 0;
					} else {
						Profile.EditCursorPos = 0;
						Profile.EditSelectStart = 0;
						Profile.EditSelectEnd = 0;
					}
					continue;
				}
				if (Message.KeyCode == VK_END) {
					Message.Processed = true;
					Profile.EditBlinkClock = clock();
					Profile.EditShowCursor = true;
					if (Message.InShift) {
						if (!hasSelection) Profile.EditSelectStart = Profile.EditCursorPos;
						Profile.EditCursorPos = static_cast<int>(Profile.EditText.size());
						Profile.EditSelectEnd = Profile.EditCursorPos;
					} else {
						Profile.EditCursorPos = static_cast<int>(Profile.EditText.size());
						Profile.EditSelectStart = Profile.EditCursorPos;
						Profile.EditSelectEnd = Profile.EditCursorPos;
					}
					continue;
				}
				if (Message.KeyCode == VK_DELETE) {
					Message.Processed = true;
					if (hasSelection) {
						doDelete(selStart, selEnd - selStart);
					} else if (Profile.EditCursorPos < static_cast<int>(Profile.EditText.size())) {
						Profile.EditText.erase(Profile.EditCursorPos, 1);
					}
					continue;
				}
				if (Message.KeyCode == VK_BACK) {
					Message.Processed = true;
					if (hasSelection) {
						doDelete(selStart, selEnd - selStart);
					} else if (Profile.EditCursorPos > 0) {
						Profile.EditText.erase(Profile.EditCursorPos - 1, 1);
						--Profile.EditCursorPos;
					}
					Profile.EditSelectStart = Profile.EditCursorPos;
					Profile.EditSelectEnd = Profile.EditCursorPos;
					continue;
				}
				if (Message.KeyCode == VK_RETURN) {
					try {
						if constexpr (std::is_same_v<Type, float>) {
							Value = static_cast<Type>(std::stof(Profile.EditText));
						} else {
							Value = static_cast<Type>(std::stoi(Profile.EditText));
						}
						Value = std::max(Profile.MinValue, std::min(Profile.MaxValue, Value));
					} catch (...) {
						// Invalid input, keep old value
					}
					Profile.InEditMode = false;
					Message.Processed = true;
					continue;
				}
				if (Message.KeyCode == VK_ESCAPE) {
					Profile.InEditMode = false;
					Message.Processed = true;
					continue;
				}
			}
			if (Message.InputChar) {
				Message.Processed = true;
				if (Message.Char == '\x1') { // Ctrl+A
					Profile.EditSelectStart = 0;
					Profile.EditSelectEnd = static_cast<int>(Profile.EditText.size());
					Profile.EditCursorPos = Profile.EditSelectEnd;
					continue;
				}
				HXChar ch = Message.Char;
				bool valid = false;
				if (ch >= L'0' && ch <= L'9') valid = true;
				if (ch == L'.' && std::is_same_v<Type, float>) valid = true;
				if (ch == L'-' && Profile.EditText.empty()) valid = true;
				if (!valid) continue;

				int selStart = std::min(Profile.EditSelectStart, Profile.EditSelectEnd);
				int selEnd = std::max(Profile.EditSelectStart, Profile.EditSelectEnd);
				if (selStart != selEnd) {
					Profile.EditText.erase(selStart, selEnd - selStart);
					Profile.EditCursorPos = selStart;
				}
				Profile.EditText.insert(Profile.EditCursorPos, 1, ch);
				++Profile.EditCursorPos;
				Profile.EditSelectStart = Profile.EditCursorPos;
				Profile.EditSelectEnd = Profile.EditCursorPos;
			}
		}
	}

	// Keyboard nudging when focused (disabled in edit mode)
	if (Profile.OnFocus && !Profile.InEditMode) {
		for (auto &Message : context.MessageQuery) {
			if (Message.Processed) {
				continue;
			}
			if (Message.KeyPressed) {
				auto range = Profile.MaxValue - Profile.MinValue;
				auto step  = range / 100;
				if (step == 0) {
					step = 1;
				}
				if (Message.KeyCode == VK_LEFT || Message.KeyCode == VK_DOWN) {
					Message.Processed = true;
					Value -= step;
				}
				if (Message.KeyCode == VK_RIGHT || Message.KeyCode == VK_UP) {
					Message.Processed = true;
					Value += step;
				}
			}
		}
	}

	auto percentage = static_cast<float>(Value - Profile.MinValue) / static_cast<float>(
						  Profile.MaxValue - Profile.MinValue);

	// Process the presse or hover
	for (auto &Message : context.MessageQuery) {
		auto mouse = ClipCoord({Message.MouseX, Message.MouseY});
		if (Message.Processed) {
			continue;
		}
		if (Message.MouseLeftPressed && rectangle.PointInside(mouse)) {
			Message.Processed = true;
			context.FocusedProfile = &Profile;

			// Double-click detection to enter edit mode
			clock_t now = clock();
			if (Profile.LastClickClock != 0 && (now - Profile.LastClickClock) < CLOCKS_PER_SEC / 3) {
				Profile.InEditMode = true;
				Profile.LastClickClock = 0;
				Profile.InDrag = false;
#ifdef UNICODE
				Profile.EditText = std::to_wstring(Value);
				Profile.EditText.erase(Profile.EditText.find_last_not_of(L'0') + 1, std::wstring::npos);
				if (!Profile.EditText.empty() && Profile.EditText.back() == L'.') Profile.EditText += L'0';
#else
				Profile.EditText = std::to_string(Value);
				Profile.EditText.erase(Profile.EditText.find_last_not_of('0') + 1, std::string::npos);
				if (!Profile.EditText.empty() && Profile.EditText.back() == '.') Profile.EditText += '0';
#endif
				Profile.EditCursorPos = static_cast<int>(Profile.EditText.size());
				Profile.EditSelectStart = 0;
				Profile.EditSelectEnd = 0;
				Profile.EditBlinkClock = clock();
				Profile.EditShowCursor = true;
			} else {
				Profile.LastClickClock = now;
				if (!Profile.InEditMode) {
					percentage = static_cast<float>(mouse.X - position.X) / Profile.Width;
					Profile.InDrag = true;
				}
			}
		}
		if (!Profile.InEditMode) {
			if (Message.MouseAction && Profile.InDrag) {
				Message.Processed = true;
				percentage = static_cast<float>(mouse.X - position.X) / Profile.Width;
			}
			if (Message.MouseLeftRelease && Profile.InDrag) {
				Profile.InDrag = false;
			}
		}
	}

	if (percentage > 1.0) {
		percentage = 1.0;
	}
	if (percentage < 0.0) {
		percentage = 0.0;
	}

	Value = static_cast<Type>(percentage * (Profile.MaxValue - Profile.MinValue) + Profile.MinValue);

	painter->Begin();

	if (Profile.InEditMode) {
		// Edit mode: text-input style rendering
		painter->DrawFilledRoundedRectangle(rectangle, theme.TextInputBackground,
								 theme.TextInputBackground, theme.RadiusMd);

		HXFont defaultFont{};
		int textX = position.X + 5;
		int textY = position.Y + height / 2 - 8;

		// Draw selection
		int selStart = std::min(Profile.EditSelectStart, Profile.EditSelectEnd);
		int selEnd = std::max(Profile.EditSelectStart, Profile.EditSelectEnd);
		if (selStart != selEnd && selEnd <= static_cast<int>(Profile.EditText.size())) {
			auto beforeSel = Profile.EditText.substr(0, selStart);
			auto selText = Profile.EditText.substr(selStart, selEnd - selStart);
			int selX = textX + painter->MeasureText(beforeSel, defaultFont, Profile.FontSize).CalWidth();
			int selW = painter->MeasureText(selText, defaultFont, Profile.FontSize).CalWidth();
			if (selW < 2) selW = 2;
			painter->DrawFilledRoundedRectangle({selX, textY, selX + selW, textY + Profile.FontSize},
										 theme.TextInputSelectedBackground, theme.TextInputSelectedBackground, theme.RadiusSm);
		}

		painter->DrawText(Profile.EditText, HXFont{}, {textX, textY}, theme.TextInputText, Profile.FontSize);

		// Blinking cursor
		if (Profile.EditShowCursor) {
			auto beforeCursor = Profile.EditText.substr(0, Profile.EditCursorPos);
			int cursorX = textX + painter->MeasureText(beforeCursor, defaultFont, Profile.FontSize).CalWidth();
			painter->DrawLine({cursorX, position.Y + 3},
							  {cursorX, position.Y + height - 3},
							  theme.TextInputCursorColor);
		}
	} else {
		int trackHeight = 4;
		int trackY = position.Y + (height - trackHeight) / 2;

		// Track background
		HXRect trackRect = {position.X, trackY, position.X + Profile.Width, trackY + trackHeight};
		painter->DrawFilledRoundedRectangle(trackRect, theme.SliderBackground, theme.SliderBackground, trackHeight / 2);

		// Track fill
		int fillWidth = static_cast<int>(percentage * Profile.Width);
		if (fillWidth > 0) {
			HXRect fillRect = {position.X, trackY, position.X + fillWidth, trackY + trackHeight};
			painter->DrawFilledRoundedRectangle(fillRect, theme.SliderActiveBackground, theme.SliderActiveBackground, trackHeight / 2);
		}
	}

	if (!Profile.InEditMode) {
		int trackHeight = 4;
		int trackY = position.Y + (height - trackHeight) / 2;
		int thumbCx = static_cast<HXGInt>(position.X + percentage * Profile.Width);
		int thumbCy = trackY + trackHeight / 2;
		int thumbR = 6;

		// Clamp thumb to track bounds
		if (thumbCx < position.X + thumbR) thumbCx = position.X + thumbR;
		if (thumbCx > position.X + Profile.Width - thumbR) thumbCx = position.X + Profile.Width - thumbR;

		// Thumb shadow (subtle outer glow)
		for (int i = 3; i >= 1; --i) {
			int alpha = 20 - i * 5;
			painter->DrawFilledRoundedRectangle(
				{thumbCx - thumbR - i, thumbCy - thumbR - i, thumbCx + thumbR + i, thumbCy + thumbR + i},
				{0, 0, 0, static_cast<uint8_t>(alpha)}, {0, 0, 0, static_cast<uint8_t>(alpha)}, thumbR + i);
		}

		// Thumb white fill with border
		HXRect thumbRect = {thumbCx - thumbR, thumbCy - thumbR, thumbCx + thumbR, thumbCy + thumbR};
		painter->DrawFilledRoundedRectangle(thumbRect, {180, 180, 185, 255}, {255, 255, 255, 255}, thumbR);

		// Value string on the right side of the track (NOT in the middle)
#ifdef UNICODE
		auto valueString = std::to_wstring(Value);
		valueString.erase(valueString.find_last_not_of(L'0') + 1, std::wstring::npos);
		if (!valueString.empty() && valueString.back() == L'.') valueString += L'0';
#else
		auto valueString = std::to_string(Value);
		valueString.erase(valueString.find_last_not_of('0') + 1, std::string::npos);
		if (!valueString.empty() && valueString.back() == '.') valueString += '0';
#endif
		auto valueTextRectangle = painter->MeasureText(valueString, HXFont{}, theme.FontSizeCaption);

		// Draw value to the right of the slider track
		int valueX = position.X + Profile.Width + 10;
		int valueY = position.Y + height / 2 - valueTextRectangle.Bottom / 2;
		painter->DrawText(valueString, HXFont{}, {valueX, valueY},
						  Profile.InDrag ? theme.SliderActiveText : theme.SliderText, theme.FontSizeCaption);
	}

	if (Profile.OnFocus) {
		painter->DrawFilledRoundedRectangle({rectangle.Left - 2, rectangle.Top - 2,
								rectangle.Right + 2, rectangle.Bottom + 2},
								   theme.SelectedBorder, {0,0,0,0}, theme.RadiusMd);
	}

	painter->End();

	LayoutNextControl({rectangle.CalWidth() + textRectangle.Right, rectangle.CalHeight()});
}

void Slider1f(const HXString &Title, float &Value, SliderProfile1f &Profile) {
	return Slider<float>(Title, Value, Profile);
}

void Slider1i(const HXString &Title, int &Value, SliderProfile1i &Profile) {
	return Slider<int>(Title, Value, Profile);
}

void DrawSliderTitle(const HXString &Title, int FontSize) {
	if (RoughVisibility()) {
		return;
	}

	// Drawing title
	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto position = CurrentLayoutPosition();

	auto painter = CurrentPainter();

	auto textRectangle = painter->MeasureText(Title, HXFont{}, theme.FontSizeCaption);

	if (AccVisibility({textRectangle.Right, textRectangle.Bottom})) {
		return;
	}

	painter->Begin();

	painter->DrawText(Title, HXFont{},
					  {position.X,
					   position.Y + 4}, theme.SliderActiveText
					  , theme.FontSizeCaption);

	painter->End();

	LayoutNextControl({textRectangle.Right, textRectangle.Bottom});
}

void Slider2f(const HXString &Title, float (&Value)[2], SliderProfile2f &Profile) {
	BeginSameLine();

	if (Profile[0].Width == -1) {
		Profile[0].Width = 150;
	}
	if (Profile[1].Width == -1) {
		Profile[1].Width = 150;
	}

	Slider1f(HXStr(""), Value[0], Profile[0]);
	Slider1f(HXStr(""), Value[1], Profile[1]);

	DrawSliderTitle(Title, Profile[0].FontSize);

	EndSameLine();
}

void Slider3f(const HXString &Title, float (&Value)[3], SliderProfile3f &Profile) {
	BeginSameLine();

	if (Profile[0].Width == -1) {
		Profile[0].Width = 100;
	}
	if (Profile[1].Width == -1) {
		Profile[1].Width = 100;
	}
	if (Profile[2].Width == -1) {
		Profile[2].Width = 100;
	}

	Slider1f(HXStr(""), Value[0], Profile[0]);
	Slider1f(HXStr(""), Value[1], Profile[1]);
	Slider1f(HXStr(""), Value[2], Profile[2]);

	DrawSliderTitle(Title, Profile[0].FontSize);

	EndSameLine();
}

void Slider4f(const HXString &Title, float (&Value)[4], SliderProfile4f &Profile) {
	BeginSameLine();

	if (Profile[0].Width == -1) {
		Profile[0].Width = 75;
	}
	if (Profile[1].Width == -1) {
		Profile[1].Width = 75;
	}
	if (Profile[2].Width == -1) {
		Profile[2].Width = 75;
	}
	if (Profile[3].Width == -1) {
		Profile[3].Width = 75;
	}

	Slider1f(HXStr(""), Value[0], Profile[0]);
	Slider1f(HXStr(""), Value[1], Profile[1]);
	Slider1f(HXStr(""), Value[2], Profile[2]);
	Slider1f(HXStr(""), Value[3], Profile[3]);

	DrawSliderTitle(Title, Profile[0].FontSize);

	EndSameLine();
}

void Slider2i(const HXString &Title, int (&Value)[2], SliderProfile2i &Profile) {
	BeginSameLine();

	if (Profile[0].Width == -1) {
		Profile[0].Width = 150;
	}
	if (Profile[1].Width == -1) {
		Profile[1].Width = 150;
	}

	Slider1i(HXStr(""), Value[0], Profile[0]);
	Slider1i(HXStr(""), Value[1], Profile[1]);

	DrawSliderTitle(Title, Profile[0].FontSize);

	EndSameLine();
}

void Slider3i(const HXString &Title, int (&Value)[3], SliderProfile3i &Profile) {
	BeginSameLine();

	if (Profile[0].Width == -1) {
		Profile[0].Width = 100;
	}
	if (Profile[1].Width == -1) {
		Profile[1].Width = 100;
	}
	if (Profile[2].Width == -1) {
		Profile[2].Width = 100;
	}

	Slider1i(HXStr(""), Value[0], Profile[0]);
	Slider1i(HXStr(""), Value[1], Profile[1]);
	Slider1i(HXStr(""), Value[2], Profile[2]);

	DrawSliderTitle(Title, Profile[0].FontSize);

	EndSameLine();
}

void Slider4i(const HXString &Title, int (&Value)[4], SliderProfile4i &Profile) {
	BeginSameLine();

	if (Profile[0].Width == -1) {
		Profile[0].Width = 75;
	}
	if (Profile[1].Width == -1) {
		Profile[1].Width = 75;
	}
	if (Profile[2].Width == -1) {
		Profile[2].Width = 75;
	}
	if (Profile[3].Width == -1) {
		Profile[3].Width = 75;
	}

	Slider1i(HXStr(""), Value[0], Profile[0]);
	Slider1i(HXStr(""), Value[1], Profile[1]);
	Slider1i(HXStr(""), Value[2], Profile[2]);
	Slider1i(HXStr(""), Value[3], Profile[3]);

	DrawSliderTitle(Title, Profile[0].FontSize);

	EndSameLine();
}
}
