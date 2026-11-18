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
  * \file hex_dockspace.cpp
  * \brief DockSpace implementation with splitter drag and push-neighbor overlap prevention
  */

#include <include/hex.h>
#include <include/hex_dockspace.h>
#include <include/hex_pool.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <unordered_map>
#include <stdio.h>

#undef max
#undef min

namespace HX {

	// Minimum pixel size for any dock slot (prevents collapse to zero)
	static constexpr HXGInt MIN_SLOT_SIZE = 20;

	// Splitter indices for drag tracking
	enum class SplitterIndex {
		None = -1,
		Left = 0,
		Right = 1,
		Top = 2,
		Bottom = 3
	};

	struct HXDockSlotTabState {
		std::vector<HXString> PrevIds;
		std::vector<HXString> CurrIds;
		HXGInt                ActiveIndex = 0;
		HXString              RequestedActivePanel;
		HXString              ClosedPanelId; // Set when user clicks tab close button
		HXGInt                TabScrollX = 0; // Horizontal scroll offset for tab bar
		std::unordered_set<HXString> UnclosableIds; // Panels that cannot be closed
	};

	struct DockSpaceState {
		HXRect        SlotRects[5];
		bool          HasSlot[5] = { false, false, false, false, false };

		// Active ratios (snapshot for hit-testing, since HasSlot is reset each frame)
		float         Ratio[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Drag state
		SplitterIndex ActiveSplitter = SplitterIndex::None;
		HXGInt        DragStartX = 0;
		HXGInt        DragStartY = 0;
		HXGInt        DragStartSize = 0;
		HXRect        AvailableRect;      // In window-local coordinates

		// Sub-painter per slot (for Begin/EndDockSlot clip-to-rect)
		HXBufferPainter* SlotSubPainters[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
		HXBufferPainter* SlotSavedPainters[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
		HXGInt          SlotSavedScrollerX[5] = { 0, 0, 0, 0, 0 };
		HXGInt          SlotSavedScrollerY[5] = { 0, 0, 0, 0, 0 };

		// Tab bar height reserved per slot (0 = no tabs)
		HXGInt          SlotTabBarHeight[5] = { 0, 0, 0, 0, 0 };

		// Ensures loaded ratios are applied to Profile only once
		bool          RatiosRestored = false;
	};

	// Stack tracking currently-open dock slots so EndDockSlot can blit the sub-painter
	static std::vector<std::pair<HXString, HXDockSlot>> G_DockSlotStack;

	// Registry of active DockSpace IDs for layout serialization
	static std::set<HXString> G_RegisteredDockSpaces;
	static std::unordered_map<HXString, HXPoint> G_SavedWindowPositions;
	static std::unordered_map<HXString, HXPoint> G_SavedWindowSizes;

	// ─── Push-Neighbor Constraint Enforcement ────────────────────────────────
	//
	// When a splitter is dragged, the opposing splitter on the same axis is
	// pushed along to maintain a minimum gap. This prevents panel rects from
	// crossing (which causes both panels to render to the same pixels — a
	// "control fight").
	//

	static void EnforceHorizontalConstraints(
		float& leftRatio,
		float& rightRatio,
		float totalW,
		HXGInt  draggedSide) // 0 = Left dragged, 1 = Right dragged, -1 = neither
	{
		if (totalW <= 0.0f) return;
		const float minGap = MIN_SLOT_SIZE / totalW;

		if (draggedSide == 0) {
			// Left is being dragged rightward. Push Right to maintain minGap.
			if (leftRatio + rightRatio > 1.0f - minGap) {
				rightRatio = std::max(0.0f, 1.0f - leftRatio - minGap);
			}
		}
		else if (draggedSide == 1) {
			// Right is being dragged leftward. Push Left to maintain minGap.
			if (leftRatio + rightRatio > 1.0f - minGap) {
				leftRatio = std::max(0.0f, 1.0f - rightRatio - minGap);
			}
		}
		else {
			// Safety net: if both ratios somehow exceed, scale them down
			// proportionally, preserving their relative weight.
			if (leftRatio + rightRatio > 1.0f - minGap) {
				float total = leftRatio + rightRatio;
				if (total > 0.001f) {
					float scale = (1.0f - minGap) / total;
					leftRatio *= scale;
					rightRatio *= scale;
				}
			}
		}

		// Clamp to valid range
		leftRatio = std::clamp(leftRatio, 0.0f, 1.0f - minGap);
		rightRatio = std::clamp(rightRatio, 0.0f, 1.0f - minGap);
	}

	static void EnforceVerticalConstraints(
		float& topRatio,
		float& bottomRatio,
		float totalH,
		HXGInt draggedSide) // 0 = Top dragged, 1 = Bottom dragged, -1 = neither
	{
		if (totalH <= 0.0f) return;
		const float minGap = MIN_SLOT_SIZE / totalH;

		if (draggedSide == 0) {
			if (topRatio + bottomRatio > 1.0f - minGap) {
				bottomRatio = std::max(0.0f, 1.0f - topRatio - minGap);
			}
		}
		else if (draggedSide == 1) {
			if (topRatio + bottomRatio > 1.0f - minGap) {
				topRatio = std::max(0.0f, 1.0f - bottomRatio - minGap);
			}
		}
		else {
			if (topRatio + bottomRatio > 1.0f - minGap) {
				float total = topRatio + bottomRatio;
				if (total > 0.001f) {
					float scale = (1.0f - minGap) / total;
					topRatio *= scale;
					bottomRatio *= scale;
				}
			}
		}

		topRatio = std::clamp(topRatio, 0.0f, 1.0f - minGap);
		bottomRatio = std::clamp(bottomRatio, 0.0f, 1.0f - minGap);
	}

	static void RecomputeSlotRects(DockSpaceState& state, const DockSpaceProfile& Profile) {
		const auto& r = state.AvailableRect;
		const HXGInt totalW = r.CalWidth();
		const HXGInt totalH = r.CalHeight();
		if (totalW <= 0 || totalH <= 0) return;

		float leftR = std::clamp(Profile.LeftRatio, 0.0f, 1.0f);
		float rightR = std::clamp(Profile.RightRatio, 0.0f, 1.0f);
		float topR = std::clamp(Profile.TopRatio, 0.0f, 1.0f);
		float bottomR = std::clamp(Profile.BottomRatio, 0.0f, 1.0f);

		// Always enforce constraints so slots never overlap (safety net)
		EnforceHorizontalConstraints(leftR, rightR, static_cast<float>(totalW), -1);
		EnforceVerticalConstraints(topR, bottomR, static_cast<float>(totalH), -1);

		HXRect remaining = r;

		// ─── Recursive split-tree logic (flattened) ──────────────────────
		// Each active slot "steals" space from the current remainder,
		// then passes the shrunken remainder to the next slot. This is
		// exactly what a recursive split tree does:
		//   Root -> Split(H, Left) -> Remainder
		//   Remainder -> Split(H, Right) -> Remainder
		//   Remainder -> Split(V, Top) -> Remainder
		//   Remainder -> Split(V, Bottom) -> Center

		// Left slot — steals from the left edge
		if (leftR > 0.001f) {
			HXGInt leftW = static_cast<HXGInt>(static_cast<float>(totalW) * leftR);
			if (leftW < MIN_SLOT_SIZE) leftW = MIN_SLOT_SIZE;
			if (leftW > remaining.CalWidth() - MIN_SLOT_SIZE) leftW = remaining.CalWidth() - MIN_SLOT_SIZE;
			if (leftW < 0) leftW = 0;
			state.SlotRects[0] = { remaining.Left, remaining.Top, remaining.Left + leftW, remaining.Bottom };
			remaining.Left += leftW;
		}
		else {
			state.SlotRects[0] = { remaining.Left, remaining.Top, remaining.Left, remaining.Bottom };
		}

		// Right slot — steals from the right edge
		if (rightR > 0.001f) {
			HXGInt rightW = static_cast<HXGInt>(static_cast<float>(totalW) * rightR);
			if (rightW < MIN_SLOT_SIZE) rightW = MIN_SLOT_SIZE;
			if (rightW > remaining.CalWidth() - MIN_SLOT_SIZE) rightW = remaining.CalWidth() - MIN_SLOT_SIZE;
			if (rightW < 0) rightW = 0;
			state.SlotRects[1] = { remaining.Right - rightW, remaining.Top, remaining.Right, remaining.Bottom };
			remaining.Right -= rightW;
		}
		else {
			state.SlotRects[1] = { remaining.Right, remaining.Top, remaining.Right, remaining.Bottom };
		}

		// Top slot — steals from the top edge
		if (topR > 0.001f) {
			HXGInt topH = static_cast<HXGInt>(static_cast<float>(totalH) * topR);
			if (topH < MIN_SLOT_SIZE) topH = MIN_SLOT_SIZE;
			if (topH > remaining.CalHeight() - MIN_SLOT_SIZE) topH = remaining.CalHeight() - MIN_SLOT_SIZE;
			if (topH < 0) topH = 0;
			state.SlotRects[2] = { remaining.Left, remaining.Top, remaining.Right, remaining.Top + topH };
			remaining.Top += topH;
		}
		else {
			state.SlotRects[2] = { remaining.Left, remaining.Top, remaining.Right, remaining.Top };
		}

		// Bottom slot — steals from the bottom edge
		if (bottomR > 0.001f) {
			HXGInt bottomH = static_cast<HXGInt>(static_cast<float>(totalH) * bottomR);
			if (bottomH < MIN_SLOT_SIZE) bottomH = MIN_SLOT_SIZE;
			if (bottomH > remaining.CalHeight() - MIN_SLOT_SIZE) bottomH = remaining.CalHeight() - MIN_SLOT_SIZE;
			if (bottomH < 0) bottomH = 0;
			state.SlotRects[3] = { remaining.Left, remaining.Bottom - bottomH, remaining.Right, remaining.Bottom };
			remaining.Bottom -= bottomH;
		}
		else {
			state.SlotRects[3] = { remaining.Left, remaining.Bottom, remaining.Right, remaining.Bottom };
		}

		// Center slot — whatever remains
		if (remaining.Right <= remaining.Left + 1) {
			remaining.Right = remaining.Left + 2;
			if (remaining.Right > r.Right) remaining.Right = r.Right;
		}
		if (remaining.Bottom <= remaining.Top + 1) {
			remaining.Bottom = remaining.Top + 2;
			if (remaining.Bottom > r.Bottom) remaining.Bottom = r.Bottom;
		}
		state.SlotRects[4] = remaining;
	}

	// Returns the slot rectangle index for a given SplitterIndex (-1 for None)
	static int SplitterSlotIdx(SplitterIndex si) {
		switch (si) {
		case SplitterIndex::Left:   return 0;
		case SplitterIndex::Right:  return 1;
		case SplitterIndex::Top:    return 2;
		case SplitterIndex::Bottom: return 3;
		default:                    return -1;
		}
	}

	// Set the appropriate resize cursor for a splitter index
	static void SetSplitterCursor(SplitterIndex si) {
		if (si == SplitterIndex::None) return;
		auto& context = GetContext();
		switch (si) {
		case SplitterIndex::Left:
		case SplitterIndex::Right:
			context.FrameCursor = HXCursorStyle::ResizeE;
			break;
		case SplitterIndex::Top:
		case SplitterIndex::Bottom:
			context.FrameCursor = HXCursorStyle::ResizeN;
			break;
		default: break;
		}
	}

	// Hit-test a splitter line using the actual slot rect geometry (not ratios).
	// The 'useSlotPresence' flag controls whether we check HasSlot[]:
	//   - During drag detection (in BeginDockSpace), HasSlot[] hasn't been set yet,
	//     so we fall back to checking whether the slot rect has non-trivial size.
	//   - During hover highlighting (in EndDockSpace), HasSlot[] is accurate.
	static SplitterIndex HitTestSplitter(
		const DockSpaceState& state,
		HXPoint mousePos,
		HXGInt splitterSize,
		bool useSlotPresence)
	{
		HXGInt halfSz = (splitterSize + 1) / 2;
		const auto& r = state.AvailableRect;
		if (r.CalWidth() <= 0 || r.CalHeight() <= 0) return SplitterIndex::None;

		const HXGInt leftSplitX = state.SlotRects[0].Right;
		const HXGInt rightSplitX = state.SlotRects[1].Left;
		const HXGInt topSplitY = state.SlotRects[2].Bottom;
		const HXGInt bottomSplitY = state.SlotRects[3].Top;

		auto isValidX = [&](HXGInt x) { return x > r.Left + MIN_SLOT_SIZE && x < r.Right - MIN_SLOT_SIZE; };
		auto isValidY = [&](HXGInt y) { return y > r.Top + MIN_SLOT_SIZE && y < r.Bottom - MIN_SLOT_SIZE; };

		// Detect presence of each splitter by checking its ratio (snapshot)
		// or slot presence (if already populated). This prevents hit-testing
		// splitters that don't exist (e.g., when LeftRatio == 0).
		auto splitterExists = [&](SplitterIndex si) -> bool {
			int idx = SplitterSlotIdx(si);
			if (idx < 0) return false;
			if (useSlotPresence) return state.HasSlot[idx];
			// Before slots are populated, use ratio snapshot
			return state.Ratio[idx] > 0.001f;
			};

		// Left splitter — vertical, full height
		if (splitterExists(SplitterIndex::Left) && isValidX(leftSplitX)) {
			HXRect hitZone = { leftSplitX - halfSz, r.Top, leftSplitX + halfSz, r.Bottom };
			if (hitZone.PointInside(mousePos)) return SplitterIndex::Left;
		}
		// Right splitter — vertical, full height
		if (splitterExists(SplitterIndex::Right) && isValidX(rightSplitX)) {
			HXRect hitZone = { rightSplitX - halfSz, r.Top, rightSplitX + halfSz, r.Bottom };
			if (hitZone.PointInside(mousePos)) return SplitterIndex::Right;
		}
		// Top splitter — horizontal, between left and right boundaries
		if (splitterExists(SplitterIndex::Top) && isValidY(topSplitY)) {
			HXRect hitZone = { leftSplitX, topSplitY - halfSz, rightSplitX, topSplitY + halfSz };
			if (hitZone.PointInside(mousePos)) return SplitterIndex::Top;
		}
		// Bottom splitter — horizontal, between left and right boundaries
		if (splitterExists(SplitterIndex::Bottom) && isValidY(bottomSplitY)) {
			HXRect hitZone = { leftSplitX, bottomSplitY - halfSz, rightSplitX, bottomSplitY + halfSz };
			if (hitZone.PointInside(mousePos)) return SplitterIndex::Bottom;
		}

		return SplitterIndex::None;
	}

	static void ApplyDrag(
		DockSpaceProfile& Profile,
		SplitterIndex     splitter,
		DockSpaceState& state,
		HXGInt            deltaX,
		HXGInt            deltaY,
		float             totalW,
		float             totalH)
	{
		if (totalW <= 0.0f || totalH <= 0.0f) return;

		// Compute new ratio, then enforce push-neighbor constraints
		switch (splitter) {
		case SplitterIndex::Left: {
			float newLeftW = static_cast<float>(state.DragStartSize) + static_cast<float>(deltaX);
			if (newLeftW < MIN_SLOT_SIZE) newLeftW = static_cast<float>(MIN_SLOT_SIZE);
			float newRatio = newLeftW / totalW;
			Profile.LeftRatio = newRatio;
			EnforceHorizontalConstraints(Profile.LeftRatio, Profile.RightRatio, totalW, 0);
			break;
		}
		case SplitterIndex::Right: {
			float newRightW = static_cast<float>(state.DragStartSize) - static_cast<float>(deltaX);
			if (newRightW < MIN_SLOT_SIZE) newRightW = static_cast<float>(MIN_SLOT_SIZE);
			float newRatio = newRightW / totalW;
			Profile.RightRatio = newRatio;
			EnforceHorizontalConstraints(Profile.LeftRatio, Profile.RightRatio, totalW, 1);
			break;
		}
		case SplitterIndex::Top: {
			float newTopH = static_cast<float>(state.DragStartSize) + static_cast<float>(deltaY);
			if (newTopH < MIN_SLOT_SIZE) newTopH = static_cast<float>(MIN_SLOT_SIZE);
			float newRatio = newTopH / totalH;
			Profile.TopRatio = newRatio;
			EnforceVerticalConstraints(Profile.TopRatio, Profile.BottomRatio, totalH, 0);
			break;
		}
		case SplitterIndex::Bottom: {
			float newBottomH = static_cast<float>(state.DragStartSize) - static_cast<float>(deltaY);
			if (newBottomH < MIN_SLOT_SIZE) newBottomH = static_cast<float>(MIN_SLOT_SIZE);
			float newRatio = newBottomH / totalH;
			Profile.BottomRatio = newRatio;
			EnforceVerticalConstraints(Profile.TopRatio, Profile.BottomRatio, totalH, 1);
			break;
		}
		default: break;
		}
	}

	void BeginDockSpace(const HXString& Id, DockSpaceProfile& Profile) {
		G_RegisteredDockSpaces.insert(Id);
		auto* state = GetStatePool().Get<DockSpaceState>(Id);
		for (int i = 0; i < 5; ++i) {
			state->HasSlot[i] = false;
			state->SlotTabBarHeight[i] = 0;
		}
		// Prepare tab states: swap current -> previous and clear current
		for (int i = 0; i < 5; ++i) {
			HXString key = Id + L"_slot_" + std::to_wstring(i);
			auto* tabState = GetStatePool().Find<HXDockSlotTabState>(key);
			if (tabState) {
				tabState->PrevIds = tabState->CurrIds;
				tabState->CurrIds.clear();
				if (tabState->ActiveIndex < 0 || tabState->ActiveIndex >= static_cast<HXGInt>(tabState->PrevIds.size())) {
					tabState->ActiveIndex = 0;
				}
			}
		}
		// Apply saved ratios once after loading (prevents drag snap-back).
		// Only restore fields where Profile still has the "not set" sentinel (-1.0f).
		// If the user explicitly set a ratio (including 0.0f), respect that.
		if (!state->RatiosRestored) {
			if (Profile.LeftRatio < 0.0f && state->Ratio[0] != 0.0f) Profile.LeftRatio = state->Ratio[0];
			if (Profile.RightRatio < 0.0f && state->Ratio[1] != 0.0f) Profile.RightRatio = state->Ratio[1];
			if (Profile.TopRatio < 0.0f && state->Ratio[2] != 0.0f) Profile.TopRatio = state->Ratio[2];
			if (Profile.BottomRatio < 0.0f && state->Ratio[3] != 0.0f) Profile.BottomRatio = state->Ratio[3];
			state->RatiosRestored = true;
		}

		auto* layout = CurrentLayout();

		// Compute AvailableRect in the current layout's content-local coordinates.
		// DockSpace is a fixed-area container: it anchors to the left edge of the
		// available region and the current baseline (top of next content row).
		// This ignores LeftMargin so the dockspace can span the full width.
		if (layout != nullptr) {
			HXGInt baseLeft = layout->AvailableRegion.Left;
			HXGInt baseTop = layout->BaseLine;

			if (Profile.AvailableRect.CalWidth() <= 0 || Profile.AvailableRect.CalHeight() <= 0) {
				state->AvailableRect = {
					baseLeft,
					baseTop,
					layout->AvailableRegion.Right,
					layout->AvailableRegion.Bottom
				};
			}
			else {
				HXGInt left = baseLeft + Profile.AvailableRect.Left;
				HXGInt top = baseTop + Profile.AvailableRect.Top;
				HXGInt right = baseLeft + Profile.AvailableRect.Right;
				HXGInt bottom = baseTop + Profile.AvailableRect.Bottom;

				if (right > layout->AvailableRegion.Right)  right = layout->AvailableRegion.Right;
				if (bottom > layout->AvailableRegion.Bottom) bottom = layout->AvailableRegion.Bottom;
				if (right - left < MIN_SLOT_SIZE * 2) right = left + MIN_SLOT_SIZE * 2;
				if (bottom - top < MIN_SLOT_SIZE * 2) bottom = top + MIN_SLOT_SIZE * 2;

				state->AvailableRect = { left, top, right, bottom };
			}
		}
		else {
			state->AvailableRect = Profile.AvailableRect;
		}

		// Snapshot ratios for hit-testing before RecomputeSlotRects may modify them
		state->Ratio[0] = Profile.LeftRatio;
		state->Ratio[1] = Profile.RightRatio;
		state->Ratio[2] = Profile.TopRatio;
		state->Ratio[3] = Profile.BottomRatio;

		// Compute slot rects early (for hit-testing and slot geometry)
		RecomputeSlotRects(*state, Profile);

		const float totalW = static_cast<float>(state->AvailableRect.CalWidth());
		const float totalH = static_cast<float>(state->AvailableRect.CalHeight());

		// ─── Continue active drag ───────────────────────────────────────────
		if (state->ActiveSplitter != SplitterIndex::None && Profile.SplitterDraggable) {
			bool dragging = false;
			for (auto& msg : GetContext().MessageQuery) {
				if (msg.Processed) continue;

				HXPoint clipMouse = ClipCoord({ msg.MouseX, msg.MouseY });

				if (msg.MouseAction && !msg.MouseLeftRelease) {
					dragging = true;
					HXGInt deltaX = clipMouse.X - state->DragStartX;
					HXGInt deltaY = clipMouse.Y - state->DragStartY;

					ApplyDrag(Profile, state->ActiveSplitter, *state, deltaX, deltaY, totalW, totalH);
					msg.Processed = true;
					RecomputeSlotRects(*state, Profile);
				}

				if (msg.MouseLeftRelease) {
					state->ActiveSplitter = SplitterIndex::None;
					msg.Processed = true;
				}
			}

		}

		// ─── Detect new drag ────────────────────────────────────────────────
		if (state->ActiveSplitter == SplitterIndex::None && Profile.SplitterDraggable) {
			for (auto& msg : GetContext().MessageQuery) {
				if (msg.Processed) continue;
				if (msg.MouseLeftPressed) {
					HXPoint clipMouse = ClipCoord({ msg.MouseX, msg.MouseY });
					auto hit = HitTestSplitter(*state, clipMouse, Profile.SplitterSize, false);
					if (hit != SplitterIndex::None) {
						state->ActiveSplitter = hit;
						state->DragStartX = clipMouse.X;
						state->DragStartY = clipMouse.Y;
						msg.Processed = true;

						switch (hit) {
						case SplitterIndex::Left:
							state->DragStartSize = state->SlotRects[0].CalWidth();
							break;
						case SplitterIndex::Right:
							state->DragStartSize = state->SlotRects[1].CalWidth();
							break;
						case SplitterIndex::Top:
							state->DragStartSize = state->SlotRects[2].CalHeight();
							break;
						case SplitterIndex::Bottom:
							state->DragStartSize = state->SlotRects[3].CalHeight();
							break;
						default: break;
						}
						break;
					}
				}
			}

			// Set resize cursor for active drag or splitter hover
			if (Profile.SplitterDraggable) {
				SplitterIndex cursorSplitter = state->ActiveSplitter;
				if (cursorSplitter == SplitterIndex::None) {
					for (auto& msg : GetContext().MessageQuery) {
						if (!msg.MouseAction) continue;
						HXPoint clipMouse = ClipCoord({ msg.MouseX, msg.MouseY });
						auto hit = HitTestSplitter(*state, clipMouse, Profile.SplitterSize, false);
						if (hit != SplitterIndex::None) {
							cursorSplitter = hit;
							break;
						}
					}
				}
				if (cursorSplitter != SplitterIndex::None) {
					SetSplitterCursor(cursorSplitter);
				}
			}
		}
	}

	void EndDockSpace(const HXString& Id, DockSpaceProfile& Profile) {
		auto* state = GetStatePool().Find<DockSpaceState>(Id);
		if (!state) return;

		auto& theme = GetTheme();
		auto  painter = CurrentPainter();
		const auto& r = state->AvailableRect;

		painter->Begin();

		HXLineStyle splitStyle;
		splitStyle.Thickness = 1;

		if (r.CalWidth() <= 0 || r.CalHeight() <= 0) { painter->End(); return; }

		// Left splitter (vertical, full height)
		if (state->HasSlot[0]) {
			HXGInt splitX = state->SlotRects[0].Right;
			painter->DrawLineStyled({ splitX, r.Top }, { splitX, r.Bottom }, theme.SplitLine, splitStyle);
		}

		// Right splitter (vertical, full height)
		if (state->HasSlot[1]) {
			HXGInt splitX = state->SlotRects[1].Left;
			painter->DrawLineStyled({ splitX, r.Top }, { splitX, r.Bottom }, theme.SplitLine, splitStyle);
		}

		// Top splitter (horizontal, between left and right splitter boundaries)
		if (state->HasSlot[2]) {
			const auto& tr = state->SlotRects[2];
			painter->DrawLineStyled({ tr.Left, tr.Bottom }, { tr.Right, tr.Bottom }, theme.SplitLine, splitStyle);
		}

		// Bottom splitter (horizontal, between left and right splitter boundaries)
		if (state->HasSlot[3]) {
			const auto& br = state->SlotRects[3];
			painter->DrawLineStyled({ br.Left, br.Top }, { br.Right, br.Top }, theme.SplitLine, splitStyle);
		}

		// ─── Drag highlight ─────────────────────────────────────────────────
		HXColor highlightColor = Profile.SplitterHighlightColor;
		switch (state->ActiveSplitter) {
		case SplitterIndex::Left:
			painter->DrawLine({ state->SlotRects[0].Right, r.Top }, { state->SlotRects[0].Right, r.Bottom }, highlightColor);
			break;
		case SplitterIndex::Right:
			painter->DrawLine({ state->SlotRects[1].Left, r.Top }, { state->SlotRects[1].Left, r.Bottom }, highlightColor);
			break;
		case SplitterIndex::Top:
			painter->DrawLine({ state->SlotRects[2].Left, state->SlotRects[2].Bottom },
				{ state->SlotRects[2].Right, state->SlotRects[2].Bottom }, highlightColor);
			break;
		case SplitterIndex::Bottom:
			painter->DrawLine({ state->SlotRects[3].Left, state->SlotRects[3].Top },
				{ state->SlotRects[3].Right, state->SlotRects[3].Top }, highlightColor);
			break;
		default: break;
		}

		// ─── Hover highlight ────────────────────────────────────────────────
		if (Profile.SplitterDraggable && state->ActiveSplitter == SplitterIndex::None) {
			for (auto& msg : GetContext().MessageQuery) {
				if (msg.Processed) continue;
				HXPoint mousePos = ClipCoord({ msg.MouseX, msg.MouseY });
				auto hit = HitTestSplitter(*state, mousePos, Profile.SplitterSize, true);
				if (hit != SplitterIndex::None) {
					HXColor hoverColor = Profile.SplitterHoverColor;
					switch (hit) {
					case SplitterIndex::Left:
						painter->DrawLine({ state->SlotRects[0].Right, r.Top },
							{ state->SlotRects[0].Right, r.Bottom }, hoverColor);
						break;
					case SplitterIndex::Right:
						painter->DrawLine({ state->SlotRects[1].Left, r.Top },
							{ state->SlotRects[1].Left, r.Bottom }, hoverColor);
						break;
					case SplitterIndex::Top:
						painter->DrawLine({ state->SlotRects[2].Left, state->SlotRects[2].Bottom },
							{ state->SlotRects[2].Right, state->SlotRects[2].Bottom }, hoverColor);
						break;
					case SplitterIndex::Bottom:
						painter->DrawLine({ state->SlotRects[3].Left, state->SlotRects[3].Top },
							{ state->SlotRects[3].Right, state->SlotRects[3].Top }, hoverColor);
						break;
					default: break;
					}
					break;
				}
			}
		}

		painter->End();



		// ─── TabBar rendering & input (after all EndDockSlot calls) ───
		for (int idx = 0; idx < 5; ++idx) {
			if (state->SlotTabBarHeight[idx] <= 0) continue;
			HXString key = Id + L"_slot_" + std::to_wstring(idx);
			auto* tabState = GetStatePool().Find<HXDockSlotTabState>(key);
			if (!tabState || tabState->CurrIds.empty()) continue;

			const auto& rect = state->SlotRects[idx];
			HXRect tabBarRect{ rect.Left, rect.Top, rect.Right, rect.Top + state->SlotTabBarHeight[idx] };
			HXGInt slotWidth = rect.CalWidth();
			constexpr HXGInt arrowW = 20;

			// Compute total tab width
			HXGInt totalTabWidth = 0;
			for (HXGInt i = 0; i < static_cast<HXGInt>(tabState->CurrIds.size()); ++i) {
				auto textRect = painter->MeasureText(tabState->CurrIds[i], HXFont{}, GetTheme().FontSize);
				constexpr HXGInt hPad = 12;
				constexpr HXGInt closeBtnW = 20;
				HXGInt tabW = textRect.Right + hPad * 2 + closeBtnW;
				if (tabW < 80) tabW = 80;
				totalTabWidth += tabW;
			}

			HXGInt maxScroll = std::max(0, totalTabWidth - slotWidth);
			if (tabState->TabScrollX > maxScroll) tabState->TabScrollX = maxScroll;
			bool canScrollLeft = tabState->TabScrollX > 0;
			bool canScrollRight = tabState->TabScrollX < maxScroll;

			// Message handling: tab switch / close / wheel scroll / arrow click
			for (auto& msg : GetContext().MessageQuery) {
				if (msg.Processed) continue;
				if (!msg.MouseAction) continue;
				HXPoint mouse{ msg.MouseX, msg.MouseY };
				if (!tabBarRect.PointInside(mouse)) continue;

				if (msg.MouseLeftPressed) {
					// Left arrow
					if (canScrollLeft) {
						HXRect leftArrow{ rect.Left, rect.Top, rect.Left + arrowW, rect.Top + state->SlotTabBarHeight[idx] };
						if (leftArrow.PointInside(mouse)) {
							msg.Processed = true;
							tabState->TabScrollX = std::max(0, tabState->TabScrollX - 80);
							continue;
						}
					}
					// Right arrow
					if (canScrollRight) {
						HXRect rightArrow{ rect.Right - arrowW, rect.Top, rect.Right, rect.Top + state->SlotTabBarHeight[idx] };
						if (rightArrow.PointInside(mouse)) {
							msg.Processed = true;
							tabState->TabScrollX = std::min(maxScroll, tabState->TabScrollX + 80);
							continue;
						}
					}
					// Tabs
					HXGInt xPos = rect.Left - tabState->TabScrollX;
					for (HXGInt i = 0; i < static_cast<HXGInt>(tabState->CurrIds.size()); ++i) {
						auto textRect = painter->MeasureText(tabState->CurrIds[i], HXFont{}, GetTheme().FontSize);
						constexpr HXGInt hPad = 12;
						constexpr HXGInt closeBtnW = 20;
						bool closable = (tabState->UnclosableIds.find(tabState->CurrIds[i]) == tabState->UnclosableIds.end());
						HXGInt tabW = textRect.Right + hPad * 2 + (closable ? closeBtnW : 0);
						if (tabW < 80) tabW = 80;
						HXRect tabRect{ xPos, rect.Top, xPos + tabW, rect.Top + state->SlotTabBarHeight[idx] };
						HXRect closeRect{ xPos + tabW - closeBtnW, rect.Top, xPos + tabW, rect.Top + state->SlotTabBarHeight[idx] };

						if (tabRect.PointInside(mouse)) {
							msg.Processed = true;
							if (closable && closeRect.PointInside(mouse)) {
								tabState->ClosedPanelId = tabState->CurrIds[i];
							}
							else {
								tabState->ActiveIndex = i;
							}
							break;
						}
						xPos += tabW;
					}
				}
				else if (msg.MouseMiddlePressed) {
					// Middle-click to close tab
					HXGInt xPos = rect.Left - tabState->TabScrollX;
					for (HXGInt i = 0; i < static_cast<HXGInt>(tabState->CurrIds.size()); ++i) {
						auto textRect = painter->MeasureText(tabState->CurrIds[i], HXFont{}, GetTheme().FontSize);
						constexpr HXGInt hPad = 12;
						constexpr HXGInt closeBtnW = 20;
						bool closable = (tabState->UnclosableIds.find(tabState->CurrIds[i]) == tabState->UnclosableIds.end());
						HXGInt tabW = textRect.Right + hPad * 2 + (closable ? closeBtnW : 0);
						if (tabW < 80) tabW = 80;
						if (!closable) { xPos += tabW; continue; }
						HXRect tabRect{ xPos, rect.Top, xPos + tabW, rect.Top + state->SlotTabBarHeight[idx] };
						if (tabRect.PointInside(mouse)) {
							msg.Processed = true;
							tabState->ClosedPanelId = tabState->CurrIds[i];
							break;
						}
						xPos += tabW;
					}
				}
				else if (msg.MouseWheel) {
					msg.Processed = true;
					if (msg.MouseWheelUp) {
						tabState->TabScrollX = std::min(maxScroll, tabState->TabScrollX + 40);
					}
					else {
						tabState->TabScrollX = std::max(0, tabState->TabScrollX - 40);
					}
				}
			}

			// Drawing into a sub-painter so we are naturally clipped to the slot width
			auto* tabPainter = painter->CreateSubPainter(slotWidth, state->SlotTabBarHeight[idx]);
			if (tabPainter) {
				tabPainter->Begin();
				tabPainter->DrawFilledRectangle({ 0, 0, slotWidth, state->SlotTabBarHeight[idx] }, theme.WindowBackground, theme.WindowBackground);

				HXGInt xPos = -tabState->TabScrollX;
				for (HXGInt i = 0; i < static_cast<HXGInt>(tabState->CurrIds.size()); ++i) {
					auto textRect = tabPainter->MeasureText(tabState->CurrIds[i], HXFont{}, GetTheme().FontSize);
					constexpr HXGInt hPad = 12;
					constexpr HXGInt closeBtnW = 20;
					bool closable = (tabState->UnclosableIds.find(tabState->CurrIds[i]) == tabState->UnclosableIds.end());
					HXGInt tabW = textRect.Right + hPad * 2 + (closable ? closeBtnW : 0);
					if (tabW < 80) tabW = 80;

					// Cull tabs outside the sub-painter bounds
					if (xPos + tabW < 0 || xPos > slotWidth) {
						xPos += tabW;
						continue;
					}

					HXRect tabRect{ xPos, 0, xPos + tabW, state->SlotTabBarHeight[idx] };
					HXRect closeRect{ xPos + tabW - closeBtnW, 0, xPos + tabW, state->SlotTabBarHeight[idx] };

					bool hovered = false;
					bool closeHovered = false;
					for (auto& msg : GetContext().MessageQuery) {
						if (!msg.MouseAction) continue;
						if (msg.Processed && !msg.MouseLeftPressed) continue;
						HXPoint mouse{ msg.MouseX, msg.MouseY };
						// Convert screen mouse to sub-painter local coords
						HXPoint localMouse{ mouse.X - rect.Left, mouse.Y - rect.Top };
						if (tabRect.PointInside(localMouse)) {
							hovered = true;
							if (closable && closeRect.PointInside(localMouse)) closeHovered = true;
						}
					}

					bool active = (i == tabState->ActiveIndex);
					if (active) {
						tabPainter->DrawFilledRectangle(tabRect, theme.ButtonOnHoverBorder, theme.ButtonOnHoverBackground);
						tabPainter->DrawText(tabState->CurrIds[i], HXFont{}, { xPos + hPad, 6 }, theme.ButtonOnHoverText, GetTheme().FontSize);
					}
					else if (hovered) {
						tabPainter->DrawFilledRectangle(tabRect, theme.ButtonBorder, theme.ButtonBackground);
						tabPainter->DrawText(tabState->CurrIds[i], HXFont{}, { xPos + hPad, 6 }, theme.ButtonOnHoverText, GetTheme().FontSize);
					}
					else {
						tabPainter->DrawText(tabState->CurrIds[i], HXFont{}, { xPos + hPad, 6 }, theme.WindowTitleText, GetTheme().FontSize);
					}

					if (closable) {
						HXColor closeColor = closeHovered ? theme.ButtonOnHoverText : (active ? theme.ButtonOnHoverText : theme.WindowTitleText);
						tabPainter->DrawText(HXStr("\u00D7"), HXFont{}, { xPos + tabW - closeBtnW + 4, 5 }, closeColor, GetTheme().FontSize);
					}

					xPos += tabW;
				}

				// Bottom border
				tabPainter->DrawLine({ 0, state->SlotTabBarHeight[idx] }, { slotWidth, state->SlotTabBarHeight[idx] }, theme.ButtonBorder);

				// Scroll arrows
				if (canScrollLeft) {
					tabPainter->DrawFilledRectangle({ 0, 0, arrowW, state->SlotTabBarHeight[idx] }, theme.WindowBackground, theme.WindowBackground);
					tabPainter->DrawText(HXStr("<"), HXFont{}, { 6, 5 }, theme.ButtonOnHoverText, GetTheme().FontSize);
				}
				if (canScrollRight) {
					tabPainter->DrawFilledRectangle({ slotWidth - arrowW, 0, slotWidth, state->SlotTabBarHeight[idx] }, theme.WindowBackground, theme.WindowBackground);
					tabPainter->DrawText(HXStr(">"), HXFont{}, { slotWidth - 14, 5 }, theme.ButtonOnHoverText, GetTheme().FontSize);
				}

				tabPainter->End();
				painter->Begin();
				painter->DrawPainter(tabPainter, { rect.Left, rect.Top });
				painter->End();
				GetContext().CurrentWindow->PendingSubPainters.push_back(tabPainter);
			}
		}

		// Advance parent layout by the full dockspace size
		LayoutNextControl({ r.CalWidth(), r.CalHeight() });
	}

	bool BeginDockSlot(const HXString& DockSpaceId, HXDockSlot Slot, const HXString& PanelId) {
		return BeginDockSlot(DockSpaceId, Slot, PanelId, true);
	}

	bool BeginDockSlot(const HXString& DockSpaceId, HXDockSlot Slot, const HXString& PanelId, bool Closable) {
		auto* state = GetStatePool().Find<DockSpaceState>(DockSpaceId);
		if (!state) return false;

		int idx = static_cast<int>(Slot);
		const auto& rect = state->SlotRects[idx];
		if (rect.CalWidth() <= 0 || rect.CalHeight() <= 0) return false;

		state->HasSlot[idx] = true;

		bool isActive = true;
		HXGInt tabBarHeight = 0;
		HXDockSlotTabState* tabState = nullptr;

		if (!PanelId.empty()) {
			HXString key = DockSpaceId + L"_slot_" + std::to_wstring(idx);
			tabState = GetStatePool().Get<HXDockSlotTabState>(key);

			// Register this panel for the current frame
			bool found = false;
			for (const auto& id : tabState->CurrIds) {
				if (id == PanelId) { found = true; break; }
			}
			if (!found) tabState->CurrIds.push_back(PanelId);

			HXGInt myIndex = -1;
			for (HXGInt i = 0; i < static_cast<HXGInt>(tabState->CurrIds.size()); ++i) {
				if (tabState->CurrIds[i] == PanelId) { myIndex = i; break; }
			}

			if (!tabState->RequestedActivePanel.empty()) {
				if (PanelId == tabState->RequestedActivePanel && myIndex >= 0) {
					tabState->ActiveIndex = myIndex;
					tabState->RequestedActivePanel.clear();
				}
				else if (tabState->CurrIds.size() == 1) {
					tabState->ActiveIndex = -1;
				}
			}
			else if (!tabState->PrevIds.empty()) {
				HXString prevActive;
				if (tabState->ActiveIndex >= 0 && tabState->ActiveIndex < static_cast<HXGInt>(tabState->PrevIds.size())) {
					prevActive = tabState->PrevIds[tabState->ActiveIndex];
				}
				if (PanelId == prevActive && myIndex >= 0) {
					tabState->ActiveIndex = myIndex;
				}
			}
			else if (tabState->CurrIds.size() == 1) {
				tabState->ActiveIndex = 0;
			}

			isActive = (myIndex == tabState->ActiveIndex);
			tabBarHeight = 28;

			// Track closable status
			if (!Closable) {
				tabState->UnclosableIds.insert(PanelId);
			}
			else {
				tabState->UnclosableIds.erase(PanelId);
			}
		}

		if (!isActive) {
			return false;
		}

		auto& context = GetContext();
		auto* parentPainter = CurrentPainter();
		auto* subPainter = GetSubPainterPool().Acquire(rect.CalWidth(), rect.CalHeight());
		if (!subPainter) return false;

		// Clear sub-painter to window background so un-drawn areas don't show garbage
		auto& theme = GetTheme();
		subPainter->Begin();
		subPainter->Clear(theme.WindowBackground);
		subPainter->End();

		// Save current painter / scroller so we can restore in EndDockSlot
		state->SlotSavedPainters[idx] = context.CurrentWindow->Painter;
		state->SlotSavedScrollerX[idx] = context.CurrentWindow->ScrollerX;
		state->SlotSavedScrollerY[idx] = context.CurrentWindow->ScrollerY;
		state->SlotSubPainters[idx] = subPainter;
		state->SlotTabBarHeight[idx] = tabBarHeight;

		// Redirect rendering into the sub-painter and offset scroller so
		// ClipCoord produces slot-local coordinates.
		context.CurrentWindow->Painter = subPainter;
		context.CurrentWindow->ScrollerX += rect.Left;
		context.CurrentWindow->ScrollerY += rect.Top;

		auto* layout = new HXLayoutContext();
		layout->AvailableRegion = { 0, tabBarHeight,
									rect.CalWidth(),
									rect.CalHeight() };
		layout->BaseLine = tabBarHeight;
		layout->LeftPadding = 0;
		layout->LeftPaddingValue = 0;
		layout->StartBaseLine = tabBarHeight;
		layout->StartLeftPadding = 0;
		layout->ControlGap = 5;
		layout->Direction = HXLayoutDirection::Vertical;
		layout->ItemAlign = HXLayoutAlign::Start;
		layout->LeftMargin = 0;

		context.LayoutStack.push_back(layout);
		G_DockSlotStack.emplace_back(DockSpaceId, Slot);
		return true;
	}

	void SetDockSlotActivePanel(const HXString& DockSpaceId, HXDockSlot Slot, const HXString& PanelId) {
		HXString key = DockSpaceId + L"_slot_" + std::to_wstring(static_cast<int>(Slot));
		auto* tabState = GetStatePool().Get<HXDockSlotTabState>(key);
		tabState->RequestedActivePanel = PanelId;
		for (HXGInt i = 0; i < static_cast<HXGInt>(tabState->CurrIds.size()); ++i) {
			if (tabState->CurrIds[i] == PanelId) {
				tabState->ActiveIndex = i;
				tabState->RequestedActivePanel.clear();
				break;
			}
		}
	}

	HXString GetDockSlotClosedPanel(const HXString& DockSpaceId, HXDockSlot Slot) {
		HXString key = DockSpaceId + L"_slot_" + std::to_wstring(static_cast<int>(Slot));
		auto* tabState = GetStatePool().Find<HXDockSlotTabState>(key);
		if (tabState) {
			HXString closed = tabState->ClosedPanelId;
			tabState->ClosedPanelId.clear();
			return closed;
		}
		return HXString();
	}

	void EndDockSlot() {
		auto& context = GetContext();

		// Pop slot layout
		if (!context.LayoutStack.empty()) {
			delete context.LayoutStack.back();
			context.LayoutStack.pop_back();
		}

		// Blit sub-painter back to the parent painter and restore state
		if (!G_DockSlotStack.empty()) {
			auto [dockSpaceId, slot] = G_DockSlotStack.back();
			G_DockSlotStack.pop_back();

			auto* state = GetStatePool().Find<DockSpaceState>(dockSpaceId);
			if (state) {
				int idx = static_cast<int>(slot);
				auto* subPainter = state->SlotSubPainters[idx];
				auto* savedPainter = state->SlotSavedPainters[idx];

				if (subPainter && savedPainter) {
					const auto& rect = state->SlotRects[idx];

					// Restore window painter / scroller
					context.CurrentWindow->Painter = savedPainter;
					context.CurrentWindow->ScrollerX = state->SlotSavedScrollerX[idx];
					context.CurrentWindow->ScrollerY = state->SlotSavedScrollerY[idx];

					// Blit the clipped slot buffer back to its window position
					savedPainter->Begin();
					savedPainter->DrawPainter(subPainter, { rect.Left, rect.Top });
					savedPainter->End();

					// Defer release until after the parent window's draw list has been flushed
					context.CurrentWindow->PendingSubPainters.push_back(subPainter);
					state->SlotSubPainters[idx] = nullptr;
				}

				// Consume unhandled mouse messages that fall inside this slot
				// Prevents messages from penetrating to controls that lie underneath
				// this slot in the call order (z-order).
				// NOTE: Tab bar area is NOT consumed here so that EndDockSpace can
				// handle tab switching and closing uniformly after all panels registered.
				const auto& slotRect = state->SlotRects[idx];
				for (auto& msg : context.MessageQuery) {
					if (!msg.MouseAction) continue;
					if (msg.Processed) continue;
					HXPoint mouse{ msg.MouseX, msg.MouseY };
					if (slotRect.PointInside(mouse)) {
						if (state->SlotTabBarHeight[idx] > 0 && mouse.Y < slotRect.Top + state->SlotTabBarHeight[idx]) {
							continue;
						}
						msg.Processed = true;
					}
				}
			}
		}
	}

	// ─── Layout Serialization ──────────────────────────────────────────────

	void SaveDockLayout(const HXString& path) {
		std::wofstream ofs(path.c_str());
		if (!ofs.is_open()) return;

		// Save DockSpace ratios and active tabs
		for (const auto& id : G_RegisteredDockSpaces) {
			auto* state = GetStatePool().Find<DockSpaceState>(id);
			if (!state) continue;
			ofs << L"[DockSpace:" << id << L"]" << std::endl;
			ofs << L"LeftRatio=" << state->Ratio[0] << std::endl;
			ofs << L"RightRatio=" << state->Ratio[1] << std::endl;
			ofs << L"TopRatio=" << state->Ratio[2] << std::endl;
			ofs << L"BottomRatio=" << state->Ratio[3] << std::endl;
			for (int i = 0; i < 5; ++i) {
				HXString key = id + L"_slot_" + std::to_wstring(i);
				auto* tabState = GetStatePool().Find<HXDockSlotTabState>(key);
				if (tabState && !tabState->CurrIds.empty()) {
					ofs << L"Slot" << i << L"ActiveTab=" << tabState->ActiveIndex << std::endl;
				}
			}
			ofs << std::endl;
		}

		// Save window positions
		auto& context = GetContext();
		for (auto* win : context.Windows) {
			ofs << L"[Window:" << win->Title << L"]" << std::endl;
			ofs << L"X=" << win->Where.X << std::endl;
			ofs << L"Y=" << win->Where.Y << std::endl;
			ofs << L"W=" << win->Size.X << std::endl;
			ofs << L"H=" << win->Size.Y << std::endl;
			ofs << std::endl;
		}
	}

	bool HasSavedWindowPosition(const HXString& title) {
		return G_SavedWindowPositions.find(title) != G_SavedWindowPositions.end();
	}

	HXPoint GetSavedWindowPosition(const HXString& title) {
		auto it = G_SavedWindowPositions.find(title);
		if (it != G_SavedWindowPositions.end()) return it->second;
		return { 0, 0 };
	}

	HXPoint GetSavedWindowSize(const HXString& title) {
		auto it = G_SavedWindowSizes.find(title);
		if (it != G_SavedWindowSizes.end()) return it->second;
		return { 0, 0 };
	}

	void LoadDockLayout(const HXString& path) {
		std::wifstream ifs(path.c_str());
		if (!ifs.is_open()) return;

		G_SavedWindowPositions.clear();
		G_SavedWindowSizes.clear();

		HXString line;
		HXString currentSection;
		while (std::getline(ifs, line)) {
			// Trim whitespace
			size_t start = line.find_first_not_of(L" \t\r\n");
			if (start == HXString::npos) continue;
			size_t end = line.find_last_not_of(L" \t\r\n");
			HXString trimmed = line.substr(start, end - start + 1);
			if (trimmed.empty() || trimmed[0] == L'#') continue;

			if (trimmed.front() == L'[' && trimmed.back() == L']') {
				currentSection = trimmed.substr(1, trimmed.size() - 2);
				continue;
			}

			size_t eq = trimmed.find(L'=');
			if (eq == HXString::npos) continue;
			HXString key = trimmed.substr(0, eq);
			HXString val = trimmed.substr(eq + 1);

			// DockSpace ratios and active tabs
			if (currentSection.find(L"DockSpace:") == 0) {
				HXString id = currentSection.substr(10);
				auto* state = GetStatePool().Get<DockSpaceState>(id);
				if (key == L"LeftRatio") { state->Ratio[0] = std::stof(val); state->RatiosRestored = false; }
				if (key == L"RightRatio") { state->Ratio[1] = std::stof(val); state->RatiosRestored = false; }
				if (key == L"TopRatio") { state->Ratio[2] = std::stof(val); state->RatiosRestored = false; }
				if (key == L"BottomRatio") { state->Ratio[3] = std::stof(val); state->RatiosRestored = false; }
				for (int i = 0; i < 5; ++i) {
					HXString expected = HXString(L"Slot") + std::to_wstring(i) + L"ActiveTab";
					if (key == expected) {
						HXString tabKey = id + L"_slot_" + std::to_wstring(i);
						auto* tabState = GetStatePool().Get<HXDockSlotTabState>(tabKey);
						tabState->ActiveIndex = std::stoi(val);
						break;
					}
				}
			}
			// Window positions
			if (currentSection.find(L"Window:") == 0) {
				HXString title = currentSection.substr(7);
				if (key == L"X") G_SavedWindowPositions[title].X = std::stoi(val);
				if (key == L"Y") G_SavedWindowPositions[title].Y = std::stoi(val);
				if (key == L"W") G_SavedWindowSizes[title].X = std::stoi(val);
				if (key == L"H") G_SavedWindowSizes[title].Y = std::stoi(val);
			}
		}
	}

} // namespace HX
