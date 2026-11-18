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
 * \file hex_propertygrid.h
 * \brief PropertyGrid control for editing key-value properties
 */

#pragma once

#include <include/hex_string.h>
#include <include/hex_geo.h>
#include <vector>
#include <functional>

struct HXBufferPainter;

namespace HX {

enum class PropertyType {
	String,
	Bool,
	Int,
	Float,
	Enum,
	Button
};

/**
 * A single property item
 */
struct PropertyItem {
	HXString Name;
	HXString StringValue;
	bool     BoolValue   = false;
	float    FloatValue  = 0.0f;
	int      IntValue    = 0;
	PropertyType Type    = PropertyType::String;
	std::vector<HXString> EnumOptions;
	HXString Id;
};

/**
 * Profile for PropertyGrid
 */
struct PropertyGridProfile {
	HXPoint Size      = {300, 400};
	int     RowHeight = 28;
	int     NameWidth = 120;
	std::function<bool(HXBufferPainter* p, const HXRect& valueRect, PropertyItem& item)> CustomEditorCallback;
};

/**
 * Render a property grid. Mutates items in place on interaction.
 * @return Index of clicked button item, or -1 if no button was clicked.
 */
int PropertyGrid(const HXString &id, std::vector<PropertyItem> &items, PropertyGridProfile &profile);

}
