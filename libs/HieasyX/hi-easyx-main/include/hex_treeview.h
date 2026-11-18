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
 * \file hex_treeview.h
 * \brief TreeView control with virtual scrolling
 */

#pragma once

#include <include/hex_string.h>
#include <include/hex_geo.h>
#include <vector>
#include <functional>

struct HXBufferPainter;

namespace HX {

/**
 * A single node in the tree view
 */
struct HXTreeNode {
	HXString                Label;
	bool                    Expanded = true;
	bool                    Selected = false;
	std::vector<HXTreeNode> Children;
};

/**
 * Profile for TreeView
 */
struct TreeViewProfile {
	HXPoint Size      = {200, 300};
	int     RowHeight = 24;
	int     Indent    = 16;
	bool    MultiSelect = false;
	bool    Activated = false;
	HXString ActivatedLabel;
	HXString ActivatedPath;
	bool    ContextMenuRequested = false;
	HXString ContextMenuLabel;
	HXString ContextMenuPath;
	HXPoint ContextMenuPos = {0, 0};
	std::function<void(HXBufferPainter* p, const HXRect& rowRect, const HXTreeNode& node)> RenderItemCallback;
	std::function<void(const HXTreeNode& node)> ItemActivatedCallback;
	HXGInt ScrollbarWidth = 8;
};

/**
 * Render a hierarchical tree view with expand/collapse and virtual scrolling.
 * Mutates node.Expanded and node.Selected on interaction.
 */
void TreeView(const HXString &id, std::vector<HXTreeNode> &nodes, TreeViewProfile &profile);

}
