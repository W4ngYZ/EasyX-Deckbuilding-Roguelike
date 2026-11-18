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
 * \file hex_checkbox_group.cpp
 * \brief The checkbox group for HiEasyX
 */

#include <include/hex.h>
#include <include/hex_checkbox_group.h>

namespace HX {
void CheckboxGroupSingle(const HXString &Title, CheckboxGroupProfile &Profile, bool SameLine) {
	if (RoughVisibility()) {
		return;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto position = CurrentLayoutPosition();

	auto painter = CurrentPainter();

	auto font  = HXFont{};
	font.Style = HXFontStyle::Bold;
	const auto fontRect = painter->MeasureText(Title, font, 18);

	const auto textRectangle = HXRect{position.X, position.Y,
	                                  position.X + fontRect.Right,
	                                  position.Y + fontRect.Bottom};

	// Process the presse or hover
	if (!AccVisibility({textRectangle.CalWidth(), textRectangle.CalHeight() + 5})) {
		painter->Begin();
		painter->DrawText(Title, font,
						  {position.X, position.Y + 5},
						  theme.ButtonPressedText, 18);
		painter->End();
		LayoutNextControl({textRectangle.CalWidth(), textRectangle.CalHeight() + 5});
	}

	if (SameLine) {
		BeginSameLine();
	}

	for (auto pf = Profile.MappingStatus.begin(); pf != Profile.MappingStatus.end(); ++pf) {
		auto before = pf->second;
		Checkbox(pf->first, std::ref(pf->second));
		if (pf->second.Checked && !before.Checked) {
			for (auto checkPf = Profile.MappingStatus.begin(); checkPf != Profile.MappingStatus.end(); ++checkPf) {
				if (checkPf != pf && checkPf->second.Checked) {
					checkPf->second.Checked = false;
				}
			}
		}
	}

	if (SameLine) {
		EndSameLine();
	}
}

void CheckboxGroup(const HXString &Title, CheckboxGroupProfile &Profile, bool SameLine) {
	if (RoughVisibility()) {
		return;
	}

	auto &context = GetContext();
	auto &theme   = GetTheme();

	auto position = CurrentLayoutPosition();

	auto font  = HXFont{};
	font.Style = HXFontStyle::Bold;

	auto       painter  = CurrentPainter();
	const auto fontRect = painter->MeasureText(Title, font, 18);

	const auto textRectangle = HXRect{position.X, position.Y,
	                                  position.X + fontRect.Right,
	                                  position.Y + fontRect.Bottom};

	// Process the presse or hover
	if (!AccVisibility({textRectangle.CalWidth(), textRectangle.CalHeight() + 5})) {
		painter->Begin();
		painter->DrawText(Title, font,
						  {position.X, position.Y + 5},
						  theme.ButtonPressedText, 18);
		painter->End();

		LayoutNextControl({textRectangle.CalWidth(), textRectangle.CalHeight() + 5});
	}

	if (SameLine) {
		BeginSameLine();
	}

	for (auto pf = Profile.MappingStatus.begin(); pf != Profile.MappingStatus.end(); ++pf) {
		Checkbox(pf->first, std::ref(pf->second));
	}

	if (SameLine) {
		EndSameLine();
	}
}
}
