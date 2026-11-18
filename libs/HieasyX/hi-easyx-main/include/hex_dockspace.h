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
 * \file hex_dockspace.h
 * \brief DockSpace layout system for HiEasyX
 *
 * Divides an available rectangle into five docking regions (Left, Right, Top,
 * Bottom, Center). Controls placed inside a slot are laid out within that region.
 * Splitters between regions can be dragged by the user when SplitterDraggable is
 * set to true.
 */

#pragma once

#include <include/hex_string.h>
#include <include/impl/hex_impl.h>

namespace HX {

enum class HXDockSlot {
	Left = 0,
	Right,
	Top,
	Bottom,
	Center
};

/**
 * Profile for a DockSpace. Ratios are in the range [0, 1].
 * The Center region occupies whatever space remains after the four edges.
 */
struct DockSpaceProfile {
	HXRect AvailableRect;
	// Use -1.0f to mean "not set / restore from saved state".
	// Use 0.0f to explicitly disable a slot.
	float  LeftRatio    = -1.0f;
	float  RightRatio   = -1.0f;
	float  TopRatio     = -1.0f;
	float  BottomRatio  = -1.0f;
	HXGInt SplitterSize = 8;
	bool   SplitterDraggable = true;
	HXColor SplitterHighlightColor{255, 200, 64, 200};
	HXColor SplitterHoverColor{255, 200, 64, 100};
};

/**
 * Begin a DockSpace. Must be paired with EndDockSpace().
 */
void BeginDockSpace(const HXString &Id, DockSpaceProfile &Profile);

/**
 * End a DockSpace. Draws splitter lines between regions.
 */
void EndDockSpace(const HXString &Id, DockSpaceProfile &Profile);

/**
 * Enter a docking slot. Returns true if the slot has non-zero area.
 * Controls between BeginDockSlot/EndDockSlot are laid out inside this slot.
 *
 * When PanelId is non-empty, multiple panels can share the same slot.
 * The DockSpace automatically renders a TabBar; only the active panel
 * returns true on a given frame.
 */
bool BeginDockSlot(const HXString &DockSpaceId, HXDockSlot Slot, const HXString &PanelId = HXString());
bool BeginDockSlot(const HXString &DockSpaceId, HXDockSlot Slot, const HXString &PanelId, bool Closable);

/**
 * Request a tabbed docking slot to activate the given panel.
 */
void SetDockSlotActivePanel(const HXString &DockSpaceId, HXDockSlot Slot, const HXString &PanelId);

/**
 * Get the panel ID that was closed by clicking the tab close button.
 * Returns empty string if no tab was closed this frame.
 * Call once per frame after EndDockSpace.
 */
HXString GetDockSlotClosedPanel(const HXString &DockSpaceId, HXDockSlot Slot);

/**
 * Leave the current docking slot.
 */
void EndDockSlot();

/**
 * Save all DockSpace ratios and Window positions to a file.
 */
void SaveDockLayout(const HXString &path);

/**
 * Load all DockSpace ratios and Window positions from a file.
 */
void LoadDockLayout(const HXString &path);

/**
 * Check if a saved window position exists for the given title.
 */
bool HasSavedWindowPosition(const HXString &title);

/**
 * Get the saved window position for the given title. Returns {0,0} if not found.
 */
HXPoint GetSavedWindowPosition(const HXString &title);

/**
 * Get the saved window size for the given title. Returns {0,0} if not found.
 */
HXPoint GetSavedWindowSize(const HXString &title);

}
