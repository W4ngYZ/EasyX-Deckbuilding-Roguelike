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
 * \file hex.cpp
 * \brief The HiEasyX major code
 */

#include <include/hex.h>
#include <include/hex_pool.h>
#include <include/hex_render_backend.h>

#include <map>
#include <vector>

#undef max

namespace HX {
HXTheme          Theme;
HXRuntimeContext Context;
HXMessageSender *MsgSender;

static HXRenderBackend *G_RenderBackend = nullptr;

std::vector<HXWindow*> G_WindowPool;

void SetRenderBackend(HXRenderBackend *backend) {
	G_RenderBackend = backend;
}

HXRenderBackend *GetRenderBackend() {
	return G_RenderBackend;
}

void ResetHXWindow(HXWindow *Window) {
	Window->Title.clear();
	Window->Size          = {0, 0};
	Window->Where         = {0, 0};
	Window->Folded        = false;
	Window->Layout        = HXLayoutContext{};
	Window->CurrentLayout = nullptr;
	Window->ScrollerX     = 0;
	Window->ScrollerY     = 0;
	Window->Scroller      = nullptr;
	GetSubPainterPool().Release(Window->RealPainter);
	Window->RealPainter = nullptr;
	delete Window->Painter;
	Window->Painter = nullptr;
	Window->DrawList->Clear();
	for (auto *p : Window->PendingSubPainters) {
		GetSubPainterPool().Release(p);
	}
	Window->PendingSubPainters.clear();
}

HXRuntimeContext &GetContext() {
	return Context;
}

HXTheme &GetTheme() {
	return Theme;
}

void SetBuffer(void *Buffer) {
	Context.LocalBuffer = Buffer;
}

void OSAPI(HXOSOperation *API) {
	Context.OSAPI = API;
}

HXString GetLastError() {
	return Context.LastError;
}

void MessageSender(HXMessageSender *Sender) {
	MsgSender = Sender;
	if (Sender == nullptr) {
		Context.Win       = false;
		Context.LastError = HXStr("Invalid message sender");
	}
}

void PushMessage(void *Message) {
	Context.MessageQuery.push_back(MsgSender->Message(Message));
	SyncEventRouter();
}

void Begin(HXContext *RenderContext) {
	if (G_RenderBackend) G_RenderBackend->NewFrame();
	HX::ResetTooltipFrame();

	for (auto *window : Context.Windows) {
		ResetHXWindow(window);
		G_WindowPool.push_back(window);
	}

	Context.Windows.clear();

	if (Context.Initialized) {
		Context.Win       = false;
		Context.LastError = HXStr("End is needed for another UI layout progress");
	} else {
		// Move out vectors to preserve capacity and avoid per-frame reallocations
		auto savedMessageQuery = std::move(Context.MessageQuery);
		auto savedWindows      = std::move(Context.Windows);
		auto savedLayoutStack  = std::move(Context.LayoutStack);
		auto savedFocusChain   = std::move(Context.FocusChain);
		void *savedLocalBuffer = Context.LocalBuffer;
		void *savedFocusedProfile = Context.FocusedProfile;

		Context = HXRuntimeContext{};

		Context.MessageQuery    = std::move(savedMessageQuery);
		Context.Windows         = std::move(savedWindows);
		Context.LayoutStack     = std::move(savedLayoutStack);
		Context.FocusChain      = std::move(savedFocusChain);
		Context.LocalBuffer     = savedLocalBuffer;
		Context.FocusedProfile  = savedFocusedProfile;
		Context.RenderContext   = RenderContext;
		Context.Initialized     = true;
		Context.FrameCursor     = HXCursorStyle::Normal;

		Context.MessageQuery.clear();
		Context.EventRouter.Reset();
		Context.Windows.clear();
		Context.LayoutStack.clear();
		Context.FocusChain.clear();
	}
}

void WindowLocate(HXPoint Where) {
	Context.CurrentWindow->Where = Where;
}

void End() {
	// Process Tab navigation before ending the frame
	for (auto &msg : Context.MessageQuery) {
		if (msg.Processed) {
			continue;
		}
		if (msg.KeyPressed && msg.KeyCode == VK_TAB) {
			if (!Context.FocusChain.empty()) {
				int currentIdx = -1;
				for (int i = 0; i < static_cast<int>(Context.FocusChain.size()); ++i) {
					if (Context.FocusChain[i] == Context.FocusedProfile) {
						currentIdx = i;
						break;
					}
				}
				if (msg.InShift) {
					currentIdx = (currentIdx <= 0)
						             ? static_cast<int>(Context.FocusChain.size()) - 1
						             : currentIdx - 1;
				} else {
					currentIdx = (currentIdx + 1) % static_cast<int>(Context.FocusChain.size());
				}
				Context.FocusedProfile = Context.FocusChain[currentIdx];
				msg.Processed = true;
			}
		}
	}

	HX::ProcessShortcuts();

	// Flush the last window's draw list before ending the frame
	if (!Context.Windows.empty()) {
		auto *last = Context.Windows.back();
		if (last->DrawList && !last->DrawList->Commands.empty()) {
			FlushDrawList(last->RealPainter, *last->DrawList);
			last->DrawList->Clear();
		}
		for (auto *p : last->PendingSubPainters) {
			GetSubPainterPool().Release(p);
		}
		last->PendingSubPainters.clear();
	}

	if (G_RenderBackend) G_RenderBackend->EndFrame();

	Context.LayoutStack.clear();
	Context.FocusStack.Clear();
	Context.Initialized = false;
}

bool Wined() {
	return Context.Win;
}

void ApplyModernTheme(HXThemeMode Mode) {
	HXTheme t{};
	if (Mode == HXThemeMode::ModernLight) {
		t.SplitLine                = HXColor{229, 229, 229, 255};
		t.WindowBackground         = HXColor{247, 248, 250, 255};
		t.WindowTitleText          = HXColor{17,  17,  17,  255};
		t.WindowTitleBackground    = HXColor{255, 255, 255, 255};
		t.ButtonBorder             = HXColor{59,  130, 246, 255};
		t.ButtonBackground         = HXColor{59,  130, 246, 255};
		t.ButtonText               = HXColor{255, 255, 255, 255};
		t.ButtonOnHoverBorder      = HXColor{96,  165, 250, 255};
		t.ButtonOnHoverBackground  = HXColor{96,  165, 250, 255};
		t.ButtonOnHoverText        = HXColor{255, 255, 255, 255};
		t.ButtonPressedBorder      = HXColor{37,  99,  235, 255};
		t.ButtonPressedBackground  = HXColor{37,  99,  235, 255};
		t.ButtonPressedText        = HXColor{255, 255, 255, 255};
		t.SliderText               = HXColor{106, 106, 106, 255};
		t.SliderBackground         = HXColor{229, 229, 229, 255};
		t.SliderActiveText         = HXColor{17,  17,  17,  255};
		t.SliderActiveBackground   = HXColor{59,  130, 246, 255};
		t.SliderBox                = HXColor{17,  17,  17,  255};
		t.CheckboxBackground       = HXColor{247, 248, 250, 255};
		t.CheckboxActiveCross      = HXColor{59,  130, 246, 255};
		t.CheckboxText             = HXColor{17,  17,  17,  255};
		t.ScrollerBackground       = HXColor{255, 255, 255, 255};
		t.DropdownBackground       = HXColor{255, 255, 255, 255};
		t.DropdownBorder           = HXColor{229, 229, 229, 255};
		t.DropdownText             = HXColor{17,  17,  17,  255};
		t.DropdownHoverBackground  = HXColor{240, 240, 242, 255};
		t.DropdownHoverBorder      = HXColor{229, 229, 229, 255};
		t.DropdownHoverText        = HXColor{17,  17,  17,  255};
		t.TextInputBackground      = HXColor{247, 248, 250, 255};
		t.TextInputBorder          = HXColor{229, 229, 229, 255};
		t.TextInputText            = HXColor{17,  17,  17,  255};
		t.TextInputSelectedBackground = HXColor{59,  130, 246, 64};
		t.TextInputCursorColor     = HXColor{59,  130, 246, 255};
		t.FontSize                 = 16;
		t.ProgressFill             = HXColor{59,  130, 246, 255};
		t.ProgressTrack            = HXColor{229, 229, 229, 255};
		t.PopupBackground          = HXColor{255, 255, 255, 255};
		t.PopupBorder              = HXColor{229, 229, 229, 255};
		t.PopupText                = HXColor{17,  17,  17,  255};
		t.PopupHoverBackground     = HXColor{240, 240, 242, 255};
		t.TooltipBackground        = HXColor{247, 248, 250, 240};
		t.TooltipBorder            = HXColor{229, 229, 229, 255};
		t.TooltipText              = HXColor{17,  17,  17,  255};
		// Tokens
		t.PanelBackground          = HXColor{247, 248, 250, 255};
		t.PanelBorder              = HXColor{229, 229, 229, 255};
		t.ElevatedBackground       = HXColor{255, 255, 255, 255};
		t.ElevatedBorder           = HXColor{229, 229, 229, 255};
		t.HoverBackground          = HXColor{240, 240, 242, 255};
		t.HoverBorder              = HXColor{200, 200, 200, 255};
		t.SelectedBackground       = HXColor{59,  130, 246, 40};
		t.SelectedBorder           = HXColor{59,  130, 246, 255};
		t.SelectedText             = HXColor{17,  17,  17,  255};
		t.DisabledText             = HXColor{156, 163, 175, 255};
		t.ShadowColor              = HXColor{0,   0,   0,   60};
		t.BorderSubtle             = HXColor{229, 229, 229, 255};
		// Editor
		t.EditorKeywordColor       = HXColor{175, 0,   219, 255};
		t.EditorStringColor        = HXColor{196, 26,  22,  255};
		t.EditorNumberColor        = HXColor{28,  0,   207, 255};
		t.EditorCommentColor       = HXColor{121, 121, 121, 255};
		t.EditorOperatorColor      = HXColor{29,  29,  31,  255};
		t.EditorIdentifierColor    = HXColor{29,  29,  31,  255};
		t.EditorPreprocessorColor  = HXColor{121, 121, 121, 255};
		t.EditorGutterColor        = HXColor{180, 180, 185, 255};
		t.EditorCurrentLineColor   = HXColor{0,   0,   0,   8};
		// Blueprint pins (light)
		t.PinTypeFlow              = HXColor{142, 142, 147, 255};
		t.PinTypeFloat             = HXColor{0,   163, 255, 255};
		t.PinTypeInt               = HXColor{48,  209, 88,  255};
		t.PinTypeBool              = HXColor{255, 69,  58,  255};
		t.PinTypeString            = HXColor{255, 214, 10,  255};
		t.PinTypeVector            = HXColor{191, 90,  242, 255};
		t.PinTypeColor             = HXColor{255, 159, 10,  255};
		t.PinTypeTexture           = HXColor{94,  92,  230, 255};
		// Blueprint categories (light)
		t.CategoryMathColor        = HXColor{48,  209, 88,  255};
		t.CategoryFlowColor        = HXColor{142, 142, 147, 255};
		t.CategoryCompareColor     = HXColor{255, 214, 10,  255};
		t.CategoryLogicColor       = HXColor{48,  209, 88,  255};
		t.CategoryConstantColor    = HXColor{191, 90,  242, 255};
		t.CategoryFunctionColor    = HXColor{255, 55,  95,  255};
		t.CategoryConversionColor  = HXColor{94,  92,  230, 255};
		t.CategoryUtilityColor     = HXColor{142, 142, 147, 255};
		t.CategoryVariableColor    = HXColor{255, 69,  58,  255};
	}
	else if (Mode == HXThemeMode::ModernDim) {
		t.SplitLine                = HXColor{50,  55,  65,  255};
		t.WindowBackground         = HXColor{26,  29,  35,  255};
		t.WindowTitleText          = HXColor{220, 225, 232, 255};
		t.WindowTitleBackground    = HXColor{16,  18,  22,  255};
		t.ButtonBorder             = HXColor{75,  130, 220, 255};
		t.ButtonBackground         = HXColor{75,  130, 220, 255};
		t.ButtonText               = HXColor{220, 225, 232, 255};
		t.ButtonOnHoverBorder      = HXColor{100, 160, 240, 255};
		t.ButtonOnHoverBackground  = HXColor{100, 160, 240, 255};
		t.ButtonOnHoverText        = HXColor{220, 225, 232, 255};
		t.ButtonPressedBorder      = HXColor{60,  110, 200, 255};
		t.ButtonPressedBackground  = HXColor{60,  110, 200, 255};
		t.ButtonPressedText        = HXColor{220, 225, 232, 255};
		t.SliderText               = HXColor{130, 138, 148, 255};
		t.SliderBackground         = HXColor{50,  55,  65,  255};
		t.SliderActiveText         = HXColor{220, 225, 232, 255};
		t.SliderActiveBackground   = HXColor{75,  130, 220, 255};
		t.SliderBox                = HXColor{220, 225, 232, 255};
		t.CheckboxBackground       = HXColor{26,  29,  35,  255};
		t.CheckboxActiveCross      = HXColor{220, 225, 232, 255};
		t.CheckboxText             = HXColor{220, 225, 232, 255};
		t.ScrollerBackground       = HXColor{16,  18,  22,  255};
		t.DropdownBackground       = HXColor{32,  36,  43,  255};
		t.DropdownBorder           = HXColor{50,  55,  65,  255};
		t.DropdownText             = HXColor{220, 225, 232, 255};
		t.DropdownHoverBackground  = HXColor{40,  45,  52,  255};
		t.DropdownHoverBorder      = HXColor{50,  55,  65,  255};
		t.DropdownHoverText        = HXColor{220, 225, 232, 255};
		t.TextInputBackground      = HXColor{26,  29,  35,  255};
		t.TextInputBorder          = HXColor{50,  55,  65,  255};
		t.TextInputText            = HXColor{220, 225, 232, 255};
		t.TextInputSelectedBackground = HXColor{75,  130, 220, 64};
		t.TextInputCursorColor     = HXColor{75,  130, 220, 255};
		t.FontSize                 = 16;
		t.ProgressFill             = HXColor{75,  130, 220, 255};
		t.ProgressTrack            = HXColor{50,  55,  65,  255};
		t.PopupBackground          = HXColor{32,  36,  43,  255};
		t.PopupBorder              = HXColor{50,  55,  65,  255};
		t.PopupText                = HXColor{220, 225, 232, 255};
		t.PopupHoverBackground     = HXColor{40,  45,  52,  255};
		t.TooltipBackground        = HXColor{26,  29,  35,  240};
		t.TooltipBorder            = HXColor{50,  55,  65,  255};
		t.TooltipText              = HXColor{220, 225, 232, 255};
		// Tokens
		t.PanelBackground          = HXColor{26,  29,  35,  255};
		t.PanelBorder              = HXColor{50,  55,  65,  255};
		t.ElevatedBackground       = HXColor{32,  36,  43,  255};
		t.ElevatedBorder           = HXColor{50,  55,  65,  255};
		t.HoverBackground          = HXColor{40,  45,  52,  255};
		t.HoverBorder              = HXColor{65,  72,  82,  255};
		t.SelectedBackground       = HXColor{75,  130, 220, 40};
		t.SelectedBorder           = HXColor{75,  130, 220, 255};
		t.SelectedText             = HXColor{220, 225, 232, 255};
		t.DisabledText             = HXColor{130, 138, 148, 255};
		t.ShadowColor              = HXColor{0,   0,   0,   120};
		t.BorderSubtle             = HXColor{50,  55,  65,  255};
		// Editor
		t.EditorKeywordColor       = HXColor{100, 180, 255, 255};
		t.EditorStringColor        = HXColor{120, 220, 120, 255};
		t.EditorNumberColor        = HXColor{255, 200, 100, 255};
		t.EditorCommentColor       = HXColor{128, 128, 128, 255};
		t.EditorOperatorColor      = HXColor{200, 200, 200, 255};
		t.EditorIdentifierColor    = HXColor{220, 230, 240, 255};
		t.EditorPreprocessorColor  = HXColor{180, 160, 220, 255};
		t.EditorGutterColor        = HXColor{99,  116, 139, 255};
		t.EditorCurrentLineColor   = HXColor{255, 255, 255, 12};
		// Blueprint pins (dim)
		t.PinTypeFlow              = HXColor{176, 176, 181, 255};
		t.PinTypeFloat             = HXColor{78,  200, 236, 255};
		t.PinTypeInt               = HXColor{120, 220, 120, 255};
		t.PinTypeBool              = HXColor{220, 120, 120, 255};
		t.PinTypeString            = HXColor{220, 180, 100, 255};
		t.PinTypeVector            = HXColor{180, 160, 220, 255};
		t.PinTypeColor             = HXColor{255, 140, 200, 255};
		t.PinTypeTexture           = HXColor{160, 220, 220, 255};
		// Categories (dim)
		t.CategoryMathColor        = HXColor{78,  200, 236, 255};
		t.CategoryFlowColor        = HXColor{176, 176, 181, 255};
		t.CategoryCompareColor     = HXColor{220, 180, 100, 255};
		t.CategoryLogicColor       = HXColor{120, 220, 120, 255};
		t.CategoryConstantColor    = HXColor{180, 160, 220, 255};
		t.CategoryFunctionColor    = HXColor{255, 140, 200, 255};
		t.CategoryConversionColor  = HXColor{160, 220, 220, 255};
		t.CategoryUtilityColor     = HXColor{200, 200, 200, 255};
		t.CategoryVariableColor    = HXColor{220, 120, 120, 255};
	}
	else {
		// ModernDark
		t.SplitLine                = HXColor{42,  46,  54,  255};
		t.WindowBackground         = HXColor{20,  23,  28,  255};
		t.WindowTitleText          = HXColor{232, 234, 237, 255};
		t.WindowTitleBackground    = HXColor{13,  15,  18,  255};
		t.ButtonBorder             = HXColor{59,  130, 246, 255};
		t.ButtonBackground         = HXColor{59,  130, 246, 255};
		t.ButtonText               = HXColor{232, 234, 237, 255};
		t.ButtonOnHoverBorder      = HXColor{96,  165, 250, 255};
		t.ButtonOnHoverBackground  = HXColor{96,  165, 250, 255};
		t.ButtonOnHoverText        = HXColor{232, 234, 237, 255};
		t.ButtonPressedBorder      = HXColor{37,  99,  235, 255};
		t.ButtonPressedBackground  = HXColor{37,  99,  235, 255};
		t.ButtonPressedText        = HXColor{232, 234, 237, 255};
		t.SliderText               = HXColor{139, 144, 152, 255};
		t.SliderBackground         = HXColor{42,  46,  54,  255};
		t.SliderActiveText         = HXColor{232, 234, 237, 255};
		t.SliderActiveBackground   = HXColor{59,  130, 246, 255};
		t.SliderBox                = HXColor{232, 234, 237, 255};
		t.CheckboxBackground       = HXColor{20,  23,  28,  255};
		t.CheckboxActiveCross      = HXColor{232, 234, 237, 255};
		t.CheckboxText             = HXColor{232, 234, 237, 255};
		t.ScrollerBackground       = HXColor{13,  15,  18,  255};
		t.DropdownBackground       = HXColor{24,  31,  38,  255};
		t.DropdownBorder           = HXColor{42,  46,  54,  255};
		t.DropdownText             = HXColor{232, 234, 237, 255};
		t.DropdownHoverBackground  = HXColor{30,  35,  42,  255};
		t.DropdownHoverBorder      = HXColor{42,  46,  54,  255};
		t.DropdownHoverText        = HXColor{232, 234, 237, 255};
		t.TextInputBackground      = HXColor{20,  23,  28,  255};
		t.TextInputBorder          = HXColor{42,  46,  54,  255};
		t.TextInputText            = HXColor{232, 234, 237, 255};
		t.TextInputSelectedBackground = HXColor{59,  130, 246, 64};
		t.TextInputCursorColor     = HXColor{59,  130, 246, 255};
		t.FontSize                 = 16;
		t.ProgressFill             = HXColor{59,  130, 246, 255};
		t.ProgressTrack            = HXColor{42,  46,  54,  255};
		t.PopupBackground          = HXColor{24,  31,  38,  255};
		t.PopupBorder              = HXColor{42,  46,  54,  255};
		t.PopupText                = HXColor{232, 234, 237, 255};
		t.PopupHoverBackground     = HXColor{30,  35,  42,  255};
		t.TooltipBackground        = HXColor{20,  23,  28,  240};
		t.TooltipBorder            = HXColor{42,  46,  54,  255};
		t.TooltipText              = HXColor{232, 234, 237, 255};
		// Tokens
		t.PanelBackground          = HXColor{20,  23,  28,  255};
		t.PanelBorder              = HXColor{42,  46,  54,  255};
		t.ElevatedBackground       = HXColor{24,  31,  38,  255};
		t.ElevatedBorder           = HXColor{42,  46,  54,  255};
		t.HoverBackground          = HXColor{30,  35,  42,  255};
		t.HoverBorder              = HXColor{55,  60,  70,  255};
		t.SelectedBackground       = HXColor{59,  130, 246, 40};
		t.SelectedBorder           = HXColor{59,  130, 246, 255};
		t.SelectedText             = HXColor{232, 234, 237, 255};
		t.DisabledText             = HXColor{90,  96,  112, 255};
		t.ShadowColor              = HXColor{0,   0,   0,   160};
		t.BorderSubtle             = HXColor{42,  46,  54,  255};
		// Editor
		t.EditorKeywordColor       = HXColor{100, 180, 255, 255};
		t.EditorStringColor        = HXColor{120, 220, 120, 255};
		t.EditorNumberColor        = HXColor{255, 200, 100, 255};
		t.EditorCommentColor       = HXColor{128, 128, 128, 255};
		t.EditorOperatorColor      = HXColor{200, 200, 200, 255};
		t.EditorIdentifierColor    = HXColor{220, 230, 240, 255};
		t.EditorPreprocessorColor  = HXColor{180, 160, 220, 255};
		t.EditorGutterColor        = HXColor{99,  116, 139, 255};
		t.EditorCurrentLineColor   = HXColor{255, 255, 255, 12};
		// Blueprint pins (dark)
		t.PinTypeFlow              = HXColor{176, 176, 181, 255};
		t.PinTypeFloat             = HXColor{102, 227, 255, 255};
		t.PinTypeInt               = HXColor{81,  207, 102, 255};
		t.PinTypeBool              = HXColor{255, 107, 107, 255};
		t.PinTypeString            = HXColor{255, 212, 59,  255};
		t.PinTypeVector            = HXColor{179, 136, 255, 255};
		t.PinTypeColor             = HXColor{255, 159, 67,  255};
		t.PinTypeTexture           = HXColor{108, 92,  230, 255};
		// Categories (dark)
		t.CategoryMathColor        = HXColor{78,  205, 196, 255};
		t.CategoryFlowColor        = HXColor{176, 176, 181, 255};
		t.CategoryCompareColor     = HXColor{255, 212, 59,  255};
		t.CategoryLogicColor       = HXColor{81,  207, 102, 255};
		t.CategoryConstantColor    = HXColor{179, 136, 255, 255};
		t.CategoryFunctionColor    = HXColor{255, 140, 200, 255};
		t.CategoryConversionColor  = HXColor{108, 92,  230, 255};
		t.CategoryUtilityColor     = HXColor{160, 160, 160, 255};
		t.CategoryVariableColor    = HXColor{255, 107, 107, 255};
	}

	// Typography & radius tokens are shared across modes
	t.FontSizeSmall   = 13;
	t.FontSizeCaption = 14;
	t.FontSizeBody    = 15;
	t.FontSizeLabel   = 15;
	t.FontSizeHeading = 17;
	t.FontSizeTitle   = 19;

	t.RadiusSm  = 4;
	t.RadiusMd  = 6;
	t.RadiusLg  = 8;
	t.RadiusXl  = 12;
	t.Radius2Xl = 16;

	t.Space1 = 4;
	t.Space2 = 8;
	t.Space3 = 12;
	t.Space4 = 16;
	t.Space5 = 20;
	t.Space6 = 24;
	t.Space8 = 32;

	Theme = t;
}

void SetThemeMode(HXThemeMode Mode) {
	ApplyModernTheme(Mode);
}

void CreateTheme() {
	ApplyModernTheme(HXThemeMode::ModernDark);
}

void Render() {
	auto *defaultPainter = Context.RenderContext->DefaultPainter();
	if (defaultPainter) {
		defaultPainter->Begin();
	}

	HXBufferPainter *Painter = Context.RenderContext->DefaultPainter()->CreateFromBuffer(Context.LocalBuffer);
	if (Painter) {
		for (auto window = Context.Windows.rbegin(); window != Context.Windows.rend(); ++window) {
			Painter->DrawPainter((*window)->RealPainter, (*window)->Where);
		}
		delete Painter;
	}

	HX::RenderTooltip();
	HX::RenderPendingDropdownOverlays(Context.RenderContext->DefaultPainter());
	HX::RenderPendingPopupMenus(Context.RenderContext->DefaultPainter());
	HX::RenderPendingMenuBarDropdowns(Context.RenderContext->DefaultPainter());
	HX::RenderModalBackdrops(Context.RenderContext->DefaultPainter());
	HX::RenderDragDropOverlay(Context.RenderContext->DefaultPainter());

	if (defaultPainter) {
		defaultPainter->End();
	}

	// Apply accumulated cursor style once per frame
	if (Context.OSAPI) {
		Context.OSAPI->SetCursorStyle(Context.FrameCursor);
	}
}

HXPoint ClipCoord(HXPoint Point) {
	return {Point.X - Context.CurrentWindow->Where.X - Context.CurrentWindow->ScrollerX,
	        Point.Y - Context.CurrentWindow->Where.Y - Context.CurrentWindow->ScrollerY};
}

void SetControlGap(HXGInt Gap) {
	Context.CurrentWindow->CurrentLayout->ControlGap = Gap;
}

void SetBaseLine(HXGInt BaseLine) {
	Context.CurrentWindow->CurrentLayout->BaseLine = BaseLine;
}

void LayoutNextControl(HXPoint Size) {
	auto *layout = CurrentLayout();
	if (layout == nullptr) {
		return;
	}

	layout->CurrentHeightMax = std::max(Size.Y, layout->CurrentHeightMax);

	if (!layout->InSameLine) {
		layout->BaseLine += layout->CurrentHeightMax + layout->ControlGap;
		layout->LeftPadding      = layout->LeftPaddingValue;
		layout->CurrentHeightMax = 0;
	} else {
		if (!layout->LockingSameLine) {
			layout->InSameLine = false;
		}
		layout->LeftPadding += Size.X + layout->ControlGap * 2;
	}
}

HXPoint CurrentLayoutPosition() {
	auto *layout = CurrentLayout();
	if (layout == nullptr) {
		return {0, 0};
	}
	return {layout->LeftMargin + layout->LeftPadding, layout->BaseLine};
}

void BeginSameLine() {
	auto *layout = CurrentLayout();
	if (layout == nullptr) {
		return;
	}
	layout->InSameLine      = true;
	layout->LockingSameLine = true;
}

bool RoughVisibility() {
	if (Context.CurrentWindow->Scroller != nullptr && !Context.CurrentWindow->Scroller->Visible) {
		return true;
	}

	auto layout   = CurrentLayout();
	auto position = CurrentLayoutPosition();
	if (position.X > layout->AvailableRegion.Right || position.Y > layout->AvailableRegion.Bottom) {
		return true;
	}

	return Context.CurrentWindow->Folded;
}

bool AccVisibility(HXPoint Size) {
	if (!CurrentVisibility(Size)) {
		if (NeedLayoutCal()) {
			LayoutNextControl(Size);
		}

		return true;
	}

	return false;
}

bool CurrentVisibility(HXPoint Size) {
	auto position = CurrentLayoutPosition();
	auto layout   = CurrentLayout();

	if (Context.CurrentWindow->Folded) {
		return false;
	}
	if (position.X > layout->AvailableRegion.Right || position.Y > layout->AvailableRegion.Bottom) {
		return false;
	}

	HXRect ControlRect = {position.X, position.Y, position.X + Size.X, position.Y + Size.Y};

	return (ControlRect.Left < layout->AvailableRegion.Right) && (layout->AvailableRegion.Left < ControlRect.Right) &&
	       (ControlRect.Top < layout->AvailableRegion.Bottom) && (layout->AvailableRegion.Top < ControlRect.Bottom);
}

bool NeedLayoutCal() {
	auto position = CurrentLayoutPosition();

	if (Context.CurrentWindow->Folded) {
		return false;
	}

	if (position.X < CurrentLayout()->AvailableRegion.Left || position.Y < CurrentLayout()->AvailableRegion.Top) {
		return true;
	}

	return false;
}

HXBufferPainter *CurrentPainter() {
	if (Context.CurrentWindow->Scroller != nullptr) {
		return Context.CurrentWindow->Scroller->Painter;
	}

	return Context.CurrentWindow->Painter;
}

void EndSameLine() {
	auto *layout = CurrentLayout();
	if (layout == nullptr) {
		return;
	}
	layout->InSameLine      = false;
	layout->LockingSameLine = false;

	layout->BaseLine += layout->CurrentHeightMax + layout->ControlGap;
	layout->LeftPadding      = layout->LeftPaddingValue;
	layout->CurrentHeightMax = 0;
}

HXLayoutContext *CurrentLayout() {
	if (!Context.LayoutStack.empty()) {
		return Context.LayoutStack.back();
	}
	if (Context.CurrentWindow != nullptr) {
		return &Context.CurrentWindow->Layout;
	}
	return nullptr;
}

void SetLeftMargin(HXGInt Margin) {
	auto *layout = CurrentLayout();
	if (layout != nullptr) {
		layout->LeftMargin = Margin;
	}
}

////////////////////////////////////////////////////////////
/// Focus chain

void RegisterFocus(void *Profile) {
	Context.FocusChain.push_back(Profile);
}

bool IsFocused(void *Profile) {
	return Context.FocusedProfile == Profile;
}

}
