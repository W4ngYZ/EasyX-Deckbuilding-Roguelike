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
 * \file hex_propertygrid.cpp
 * \brief PropertyGrid control implementation
 */

#include <include/hex_propertygrid.h>
#include <include/hex.h>
#include <include/hex_checkbox.h>
#include <include/hex_slider.h>
#include <include/hex_text_input.h>
#include <include/hex_dropdown.h>

#include <algorithm>

#undef max
#undef min

namespace HX {

struct PropertyGridState {
	int ScrollY = 0;
};

int PropertyGrid(const HXString &id, std::vector<PropertyItem> &items, PropertyGridProfile &profile) {
	auto &context = GetContext();
	auto &theme   = GetTheme();

	if (RoughVisibility()) {
		LayoutNextControl(profile.Size);
		return -1;
	}

	auto position = CurrentLayoutPosition();
	if (AccVisibility({profile.Size.X, profile.Size.Y})) {
		LayoutNextControl(profile.Size);
		return -1;
	}

	auto *state = GetStatePool().Get<PropertyGridState>(id);
	auto  painter = CurrentPainter();

	HXRect controlRect{position.X, position.Y, position.X + profile.Size.X, position.Y + profile.Size.Y};

	// Wheel scrolling
	for (auto &msg : context.MessageQuery) {
		if (msg.Processed || !msg.MouseWheel) continue;
		auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
		if (controlRect.PointInside(mouse)) {
			state->ScrollY += msg.MouseWheelUp ? profile.RowHeight : -profile.RowHeight;
			msg.Processed = true;
		}
	}

	int totalHeight = static_cast<int>(items.size()) * profile.RowHeight;
	int maxScroll   = std::max(0, totalHeight - profile.Size.Y);
	state->ScrollY  = std::max(0, std::min(state->ScrollY, maxScroll));

	int firstRow = state->ScrollY / profile.RowHeight;
	int lastRow  = std::min(static_cast<int>(items.size()) - 1,
	                        firstRow + profile.Size.Y / profile.RowHeight + 1);

	int clickedButton = -1;

	// Draw background & border
	painter->Begin();
	painter->DrawFilledRectangle(controlRect, theme.WindowBackground, theme.WindowBackground);
	painter->DrawRectangle(controlRect, theme.TextInputBorder);
	painter->End();

	for (int i = firstRow; i <= lastRow && i < static_cast<int>(items.size()); ++i) {
		auto &item = items[i];
		HXString itemKeySuffix = item.Id.empty() ? std::to_wstring(i) : item.Id;
		int   rowY = position.Y + i * profile.RowHeight - state->ScrollY;
		HXRect rowRect{position.X, rowY, position.X + profile.Size.X, rowY + profile.RowHeight};
		HXRect nameRect{position.X, rowY, position.X + profile.NameWidth, rowY + profile.RowHeight};
		HXRect valueRect{position.X + profile.NameWidth, rowY, position.X + profile.Size.X, rowY + profile.RowHeight};

		// Alternating row background
		painter->Begin();
		if (i % 2 == 1) {
			painter->DrawFilledRectangle(rowRect, theme.TextInputBackground, theme.TextInputBackground);
		}
		// Separator line
		painter->DrawLine({position.X, rowY + profile.RowHeight}, {position.X + profile.Size.X, rowY + profile.RowHeight}, theme.SplitLine);
		painter->End();

		// Name label
		painter->Begin();
		painter->DrawText(item.Name, HXFont{}, {nameRect.Left + 6, nameRect.Top + 4}, theme.TextInputText, GetTheme().FontSize);
		painter->End();

		// Value editor with a row-local layout context
		auto *rowLayout = new HXLayoutContext();
		rowLayout->AvailableRegion = valueRect;
		rowLayout->BaseLine        = valueRect.Top + 2;
		rowLayout->LeftMargin      = valueRect.Left + 4;
		rowLayout->LeftPadding     = 0;
		rowLayout->CurrentHeightMax = 0;
		rowLayout->ControlGap      = 4;
		context.LayoutStack.push_back(rowLayout);

		bool customHandled = false;
		if (profile.CustomEditorCallback) {
			customHandled = profile.CustomEditorCallback(painter, valueRect, item);
		}

		if (!customHandled) {
			switch (item.Type) {
		case PropertyType::Bool: {
			auto *cp = GetStatePool().Get<CheckboxProfile>(id + HXStr("_cb_") + itemKeySuffix);
			cp->Checked = item.BoolValue;
			HX::Checkbox(HXStr(""), *cp);
			item.BoolValue = cp->Checked;
			break;
		}
		case PropertyType::Float: {
			auto *sp = GetStatePool().Get<SliderProfile1f>(id + HXStr("_sl_") + itemKeySuffix);
			sp->Width = valueRect.CalWidth() - 8;
			HX::Slider1f(HXStr(""), item.FloatValue, *sp);
			break;
		}
		case PropertyType::Int: {
			auto *sp = GetStatePool().Get<SliderProfile1i>(id + HXStr("_sl_") + itemKeySuffix);
			sp->Width = valueRect.CalWidth() - 8;
			HX::Slider1i(HXStr(""), item.IntValue, *sp);
			break;
		}
		case PropertyType::String: {
			auto *tp = GetStatePool().Get<TextInputProfile>(id + HXStr("_ti_") + itemKeySuffix);
			tp->Size  = {valueRect.CalWidth() - 8, profile.RowHeight - 6};
			tp->Text = item.StringValue;
			HX::TextInput(*tp);
			item.StringValue = tp->Text;
			break;
		}
		case PropertyType::Enum: {
			// Simple click-to-cycle enum
			painter->Begin();
			painter->DrawText(item.StringValue, HXFont{}, {valueRect.Left + 4, valueRect.Top + 4}, theme.ButtonOnHoverBorder, GetTheme().FontSize);
			painter->End();
			for (auto &msg : context.MessageQuery) {
				if (msg.Processed || !msg.MouseLeftPressed) continue;
				auto mouse = ClipCoord({msg.MouseX, msg.MouseY});
				if (valueRect.PointInside(mouse)) {
					int idx = -1;
					for (int j = 0; j < static_cast<int>(item.EnumOptions.size()); ++j) {
						if (item.EnumOptions[j] == item.StringValue) { idx = j; break; }
					}
					idx = (idx + 1) % static_cast<int>(item.EnumOptions.size());
					if (idx >= 0) item.StringValue = item.EnumOptions[idx];
					msg.Processed = true;
				}
			}
			break;
		}
		case PropertyType::Button: {
			auto *bp = GetStatePool().Get<ButtonProfile>(id + HXStr("_bt_") + itemKeySuffix);
			bp->Size = {valueRect.CalWidth() - 8, profile.RowHeight - 6};
			if (HX::Button(item.StringValue, *bp)) {
				clickedButton = i;
			}
			break;
		}
			}
		}

		delete context.LayoutStack.back();
		context.LayoutStack.pop_back();
	}

	LayoutNextControl(profile.Size);
	return clickedButton;
}

}
