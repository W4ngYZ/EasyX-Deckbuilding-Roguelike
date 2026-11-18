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
 * \file hex_modal.cpp
 * \brief Modal dialog implementation
 */

#include <include/hex.h>
#include <include/hex_modal.h>

#undef max
#undef min

namespace HX {

struct ModalState {
	bool Open = false;
};

struct ModalOverlayInfo {
	HXRect   ModalRect;
	HXString Title;
	bool     CloseButton;
	HXColor  BackdropColor;
	int      TitleBarHeight;
	int      CloseButtonSize;
};

static std::vector<ModalOverlayInfo> G_PendingModalOverlays;

void OpenModal(const HXString &Id) {
	auto *state = GetStatePool().Get<ModalState>(Id);
	state->Open = true;
}

void CloseModal(const HXString &Id) {
	auto *state = GetStatePool().Find<ModalState>(Id);
	if (state) {
		state->Open = false;
	}
}

bool IsModalOpen(const HXString &Id) {
	auto *state = GetStatePool().Find<ModalState>(Id);
	return state && state->Open;
}

bool BeginModal(const HXString &Id, ModalProfile &Profile) {
	auto *state = GetStatePool().Find<ModalState>(Id);
	if (!state || !state->Open) {
		return false;
	}

	auto &context = GetContext();

	// Calculate centered position relative to the current window, or screen fallback
	HXPoint modalPos;
	if (context.CurrentWindow != nullptr) {
		modalPos = {
			context.CurrentWindow->Where.X + (context.CurrentWindow->Size.X - Profile.Size.X) / 2,
			context.CurrentWindow->Where.Y + (context.CurrentWindow->Size.Y - Profile.Size.Y) / 2
		};
	} else {
		modalPos = {640 - Profile.Size.X / 2, 360 - Profile.Size.Y / 2};
	}

	// Clamp so the modal stays somewhat visible
	if (modalPos.X < 0) modalPos.X = 10;
	if (modalPos.Y < 0) modalPos.Y = 10;

	HXRect modalRect{
		modalPos.X, modalPos.Y,
		modalPos.X + Profile.Size.X, modalPos.Y + Profile.Size.Y
	};

	PushFocusScope(&Profile, Id);

	// Only the topmost focus scope may capture outside messages.
	if (IsFocusScopeTop(&Profile)) {
		// Close button handling
		if (Profile.CloseButton) {
			HXRect closeBtnRect{modalRect.Right - Profile.CloseButtonSize, modalRect.Top, modalRect.Right, modalRect.Top + Profile.CloseButtonSize};
			for (auto &msg : context.MessageQuery) {
				if (msg.MouseLeftPressed && !msg.Processed && closeBtnRect.PointInside({msg.MouseX, msg.MouseY})) {
					msg.Processed = true;
					CloseModal(Id);
				}
			}
		}

		// Capture-phase: block background input outside the modal rect
		bool clickedOutside = ConsumeMessagesOutsideRect(modalRect, Profile.CloseOnClickOutside);
		if (clickedOutside) {
			CloseModal(Id);
		}
	}

	// Push layout context for modal content (heap-allocated to avoid std::any rehash dangling)
	auto *layout = new HXLayoutContext();
	layout->AvailableRegion = {
		modalPos.X + 10,
		modalPos.Y + Profile.TitleBarHeight,
		modalPos.X + Profile.Size.X - 10,
		modalPos.Y + Profile.Size.Y - 10
	};
	layout->BaseLine         = modalPos.Y + Profile.TitleBarHeight;
	layout->LeftPadding      = modalPos.X + 10;
	layout->LeftPaddingValue = modalPos.X + 10;
	layout->StartBaseLine    = modalPos.Y + Profile.TitleBarHeight;
	layout->ControlGap       = 5;
	context.LayoutStack.push_back(layout);

	// Record for deferred backdrop rendering (after all windows)
	G_PendingModalOverlays.push_back({modalRect, Profile.Title, Profile.CloseButton, Profile.BackdropColor, Profile.TitleBarHeight, Profile.CloseButtonSize});

	return true;
}

void EndModal() {
	auto &context = GetContext();
	if (!context.LayoutStack.empty()) {
		delete context.LayoutStack.back();
		context.LayoutStack.pop_back();
	}
	PopFocusScope();
}

void RenderModalBackdrops(HXBufferPainter *Painter) {
	if (G_PendingModalOverlays.empty()) {
		return;
	}

	auto &theme = GetTheme();
	auto &context = GetContext();

	auto backBufferSize = context.RenderContext->DefaultPainter()->GetSize();
	for (auto &overlay : G_PendingModalOverlays) {
		// Full-screen backdrop
		Painter->DrawFilledRectangle({0, 0, backBufferSize.X, backBufferSize.Y},
		                             overlay.BackdropColor, overlay.BackdropColor);

		// Shadow
		HXRect shadowRect = overlay.ModalRect;
		shadowRect.Top += 8;
		shadowRect.Bottom += 8;
		Painter->DrawFilledRoundedRectangle(shadowRect, theme.ShadowColor, theme.ShadowColor, theme.RadiusXl);

		// Modal frame
		Painter->DrawFilledRoundedRectangle(overlay.ModalRect, theme.ElevatedBorder, theme.ElevatedBackground, theme.RadiusXl);

		// Title bar separator
		Painter->DrawLine(
			{overlay.ModalRect.Left + theme.RadiusXl, overlay.ModalRect.Top + overlay.TitleBarHeight - 1},
			{overlay.ModalRect.Right - theme.RadiusXl, overlay.ModalRect.Top + overlay.TitleBarHeight - 1},
			theme.BorderSubtle);

		// Title text
		Painter->DrawText(overlay.Title, HXFont{},
		                  {overlay.ModalRect.Left + 16, overlay.ModalRect.Top + (overlay.TitleBarHeight - theme.FontSizeHeading) / 2},
		                  theme.WindowTitleText, theme.FontSizeHeading);

		// Close button (visual only; click handled in BeginModal)
		if (overlay.CloseButton) {
			HXRect closeBtnRect{overlay.ModalRect.Right - overlay.CloseButtonSize, overlay.ModalRect.Top,
			                    overlay.ModalRect.Right, overlay.ModalRect.Top + overlay.CloseButtonSize};
			bool closeHovered = false;
			for (auto &msg : context.MessageQuery) {
				if (msg.MouseAction) {
					if (closeBtnRect.PointInside({msg.MouseX, msg.MouseY})) {
						closeHovered = true;
						break;
					}
				}
			}
			int cbSize = 20;
			int cbX = closeBtnRect.Left + (overlay.CloseButtonSize - cbSize) / 2;
			int cbY = closeBtnRect.Top + (overlay.CloseButtonSize - cbSize) / 2;
			if (closeHovered) {
				Painter->DrawFilledRoundedRectangle(
					{cbX, cbY, cbX + cbSize, cbY + cbSize},
					{220, 60, 60, 255}, {220, 60, 60, 255}, cbSize / 2);
			}
			HXColor closeTextColor = closeHovered ? HXColor{255, 255, 255, 255} : theme.TextSecondary;
			Painter->DrawText(HXStr("X"), HXFont{},
			                  {cbX + 6, cbY + 3},
			                  closeTextColor, theme.FontSizeBody);
		}
	}

	G_PendingModalOverlays.clear();
}

}
