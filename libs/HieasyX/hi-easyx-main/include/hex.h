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
 * \file hex.h
 * \brief The HiEasyX major code
 */

#pragma once

/**
 * Core layout enums (must be defined before container headers)
 */
enum class HXLayoutDirection {
	Vertical,
	Horizontal
};

enum class HXLayoutAlign {
	Start,
	Center,
	End,
	Stretch
};

#include <include/hex_string.h>
#include <include/hex_state.h>
#include <include/impl/hex_impl.h>
#include <include/hex_button.h>
#include <include/hex_window.h>
#include <include/hex_text.h>
#include <include/hex_slider.h>
#include <include/hex_canvas.h>
#include <include/hex_text_input.h>
#include <include/impl/EasyX/hex_easyx.h>
#include <include/hex_image.h>
#include <include/hex_dropdown.h>
#include <include/hex_checkbox.h>
#include <include/hex_checkbox_group.h>
#include <include/hex_scroller.h>
#include <include/hex_container.h>
#include <include/hex_tooltip.h>
#include <include/hex_progress.h>
#include <include/hex_menu.h>
#include <include/hex_tab.h>
#include <include/hex_animation.h>
#include <include/hex_popup.h>
#include <include/hex_modal.h>
#include <include/hex_dockspace.h>
#include <include/hex_viewport.h>
#include <include/hex_focus.h>
#include <include/hex_event.h>
#include <include/hex_blueprint.h>
#include <include/hex_dragdrop.h>
#include <include/hex_shortcut.h>
#include <include/hex_treeview.h>
#include <include/hex_table.h>
#include <include/hex_propertygrid.h>
#include <include/hex_debug.h>
#include <include/hex_editor.h>
#include <include/hex_plugin.h>
#include <include/hex_filewatcher.h>
#include <include/hex_splitter.h>
#include <include/hex_drawlist.h>

#include <vector>

struct HXTheme;
struct HXWindow;
struct HXScroller;
struct HXLayoutContext;
struct HXRuntimeContext;

namespace HX {
struct HXDrawList;
}

namespace HX {
enum class HXThemeMode {
	ModernDark,
	ModernDim,
	ModernLight
};

/**
 * Get the runtime context of HiEasyX
 * @return The runtime context of HiEasyX
 */
HXRuntimeContext &GetContext();

/**
 * Get the theme of the HiEasyX
 * @return The theme of the HiEasyX
 */
HXTheme &GetTheme();

/**
 * Create the theme for HiEasyX
 */
void CreateTheme();

/**
 * Apply a complete modern flat theme.
 *
 * ModernDark is the default high-contrast dark product-tool theme.
 * ModernDim is a softer low-contrast dark theme for long IDE/editor sessions.
 */
void ApplyModernTheme(HXThemeMode Mode = HXThemeMode::ModernDark);

/**
 * Switch theme mode at runtime. Alias for ApplyModernTheme.
 */
void SetThemeMode(HXThemeMode Mode);

/**
 * Setting the API of the current OS
 * @param API The API of the OS
 */
void OSAPI(HXOSOperation *API);

/**
 * Setting the buffer where the rendering result will be displayed
 * @param Buffer The buffer to be rendered
 */
void SetBuffer(void *Buffer);

/**
 * Register a control profile into the focus chain for this frame.
 * Called automatically by interactive controls; user code should not call this directly.
 */
void RegisterFocus(void *Profile);

/**
 * Check whether the given profile currently has keyboard focus.
 */
bool IsFocused(void *Profile);

/**
 * Getting the last error in string format
 * @return The last error in string format
 */
HXString GetLastError();

/**
 * Init the HiEasyX UI, preparing for UI layout
 * @param RenderContext The context for rendering
 */
void Begin(HXContext *RenderContext);

/**
 * Setting the message sender
 * @param Sender The sender to be set
 */
void MessageSender(HXMessageSender *Sender);

/**
 * Pushing the message to the message query
 * @param Message The message to be pushed
 * @param Size The size of the message structure
 */
void PushMessage(void *Message);

/**
 * End the UI layout progress
 */
void End();

/**
 * Check whether the operation has any error
 * @return If there is an error, returning true, nor returning false
 */
bool Wined();

/**
 * Render the UI to the buffer
 */
void Render();

/**
 * Set the gap between controls
 * @param Gap The gap between control
 */
void SetControlGap(HXGInt Gap);

/**
 * Setting a new baseline for control layout
 * @param BaseLine The new baseline to be setted
 */
void SetBaseLine(HXGInt BaseLine);

/**
 * Clipping the coord into the relative coord
 * @param Point The point needed to clip
 * @return The clipped coord
 */
HXPoint ClipCoord(HXPoint Point);

/**
 * Counting the new layout position for next control
 * @param Size The size of the new inserted control
 */
void LayoutNextControl(HXPoint Size);

/**
 * Get the current layout position
 * @return The position of the current layout
 */
HXPoint CurrentLayoutPosition();

/**
 * Getting the current working painter
 * @return The current working painter
 */
HXBufferPainter *CurrentPainter();

/**
 * Get the visibility of current context
 * @param Size The size of the control
 * @return If the return value is true, the controls are all visible, nor invisible
 */
bool CurrentVisibility(HXPoint Size);

/**
 * Check whether the window is folded
 * @return If the window is folded, returning true, nor false
 */
bool RoughVisibility();

/**
 * Check the visibility more accurately
 * @param Size The size of the control
 * @return If the control is invisible, returning false
 */
bool AccVisibility(HXPoint Size);

/**
 * Get whether the control needs to calculate the layout
 * @return If the control needs, returning true, nor false
 */
bool NeedLayoutCal();

/**
 * Set the next control and its follow will keep the same line of the current control
 */
void BeginSameLine();

/**
 * Getting the current layout
 * @return The pointer to the current layout
 */
HXLayoutContext *CurrentLayout();

/**
 * Setting the left margin of this layout
 * @param Margin The size of the left margin
 */
void SetLeftMargin(HXGInt Margin);

/**
 * Returning to normal layout
 */
void EndSameLine();
}

/**
 * The type of controls
 */
enum class HXControlType {
	Button
};

/**
 * The status of a control
 */
enum class HXControlStatus {
	Button_Hovering
};

/**
 * The structure to store controls
 */
struct HXControl {
	HXControlType   Type;
	HXString        Id;
	HXControlStatus Status;
};

struct HXLayoutContext {
	HXGInt BaseLine         = 50;
	bool   InSameLine       = false;
	bool   LockingSameLine  = false;
	HXGInt LeftPadding      = 0;
	HXGInt LeftPaddingValue = 0;
	HXGInt LeftMargin       = 5;
	HXRect AvailableRegion{};
	HXGInt CurrentHeightMax = 0;
	HXGInt ControlGap       = 5;

	/**
	 * The layout direction for child controls inside this context.
	 * Vertical  = stack downwards (default)
	 * Horizontal = stack rightwards
	 */
	HXLayoutDirection Direction = HXLayoutDirection::Vertical;

	/**
	 * The alignment of items within this layout context.
	 */
	HXLayoutAlign ItemAlign = HXLayoutAlign::Start;

	/**
	 * The initial baseline (Y origin) when this layout was created.
	 * Used by containers to calculate consumed height.
	 */
	HXGInt StartBaseLine = 0;

	/**
	 * The initial left padding (X origin) when this layout was created.
	 * Used by containers to calculate consumed width.
	 */
	HXGInt StartLeftPadding = 0;
};

struct HXScroller {
	HXBufferPainter *Painter;
	HXLayoutContext  Layout;
	bool             Visible = true;
	HXScroller *     Last;
};

/**
 * The structure to store the window, including title, position and controls
 */
struct HXWindow {
	HXString         Title;
	HXPoint          Size;
	HXPoint          Where;
	HXBufferPainter *Painter = nullptr;
	HXBufferPainter *RealPainter = nullptr;
	bool             Folded;
	HXLayoutContext  Layout;
	HXLayoutContext *CurrentLayout;

	HXGInt      ScrollerX = 0;
	HXGInt      ScrollerY = 0;
	HXScroller *Scroller  = nullptr;

	/**
	 * The deferred draw list for this window. All control drawing commands
	 * during the frame are recorded here, then executed at Render() time.
	 */
	HX::HXDrawList *DrawList = nullptr;

	/**
	 * Sub-painters that have been blitted into this window's draw list
	 * but must not be released until after FlushDrawList has executed.
	 */
	std::vector<HXBufferPainter *> PendingSubPainters;

	HXWindow() {
		DrawList = new HX::HXDrawList();
	}

	~HXWindow() {
		delete Painter;
		delete DrawList;
	}
};

/**
 * The theme structure for HiEasyX
 */
struct HXTheme {
	HXColor SplitLine{42, 46, 54, 255};
	HXColor WindowBackground{20, 23, 28, 255};
	HXColor WindowTitleText{232, 234, 237, 255};
	HXColor WindowTitleBackground{13, 15, 18, 255};
	HXColor ButtonBorder{59, 130, 246, 255};
	HXColor ButtonBackground{59, 130, 246, 255};
	HXColor ButtonText{232, 234, 237, 255};
	HXColor ButtonOnHoverBorder{96, 165, 250, 255};
	HXColor ButtonOnHoverBackground{96, 165, 250, 255};
	HXColor ButtonOnHoverText{232, 234, 237, 255};
	HXColor ButtonPressedBorder{37, 99, 235, 255};
	HXColor ButtonPressedBackground{37, 99, 235, 255};
	HXColor ButtonPressedText{232, 234, 237, 255};
	HXColor SliderText{139, 144, 152, 255};
	HXColor SliderBackground{42, 46, 54, 255};
	HXColor SliderActiveText{232, 234, 237, 255};
	HXColor SliderActiveBackground{59, 130, 246, 255};
	HXColor SliderBox{232, 234, 237, 255};
	HXColor CheckboxBackground{20, 23, 28, 255};
	HXColor CheckboxActiveCross{232, 234, 237, 255};
	HXColor CheckboxText{232, 234, 237, 255};
	HXColor ScrollerBackground{13, 15, 18, 255};
	HXColor DropdownBackground{24, 31, 38, 255};
	HXColor DropdownBorder{42, 46, 54, 255};
	HXColor DropdownText{232, 234, 237, 255};
	HXColor DropdownHoverBackground{30, 35, 42, 255};
	HXColor DropdownHoverBorder{42, 46, 54, 255};
	HXColor DropdownHoverText{232, 234, 237, 255};
	HXColor TextInputBackground{20, 23, 28, 255};
	HXColor TextInputBorder{42, 46, 54, 255};
	HXColor TextInputText{232, 234, 237, 255};
	HXColor TextInputSelectedBackground{59, 130, 246, 64};
	HXColor TextInputCursorColor{59, 130, 246, 255};
	int     FontSize = 16;

	// Editor
	HXColor EditorKeywordColor{100, 180, 255, 255};
	HXColor EditorStringColor{120, 220, 120, 255};
	HXColor EditorNumberColor{255, 200, 100, 255};
	HXColor EditorCommentColor{128, 128, 128, 255};
	HXColor EditorOperatorColor{200, 200, 200, 255};
	HXColor EditorIdentifierColor{220, 230, 240, 255};
	HXColor EditorPreprocessorColor{180, 160, 220, 255};
	HXColor EditorGutterColor{99, 116, 139, 255};
	HXColor EditorCurrentLineColor{255, 255, 255, 12};

	// Blueprint pins
	HXColor PinTypeFlow{176, 176, 181, 255};
	HXColor PinTypeFloat{102, 227, 255, 255};
	HXColor PinTypeInt{81, 207, 102, 255};
	HXColor PinTypeBool{255, 107, 107, 255};
	HXColor PinTypeString{255, 212, 59, 255};
	HXColor PinTypeVector{179, 136, 255, 255};
	HXColor PinTypeColor{255, 159, 67, 255};
	HXColor PinTypeTexture{108, 92, 230, 255};

	// Blueprint categories
	HXColor CategoryMathColor{78, 205, 196, 255};
	HXColor CategoryFlowColor{176, 176, 181, 255};
	HXColor CategoryCompareColor{255, 212, 59, 255};
	HXColor CategoryLogicColor{81, 207, 102, 255};
	HXColor CategoryConstantColor{179, 136, 255, 255};
	HXColor CategoryFunctionColor{255, 140, 200, 255};
	HXColor CategoryConversionColor{108, 92, 230, 255};
	HXColor CategoryUtilityColor{160, 160, 160, 255};
	HXColor CategoryVariableColor{255, 107, 107, 255};

	// Popup / Tooltip
	HXColor PopupBackground{24, 31, 38, 255};
	HXColor PopupBorder{42, 46, 54, 255};
	HXColor PopupText{232, 234, 237, 255};
	HXColor PopupHoverBackground{30, 35, 42, 255};
	HXColor TooltipBackground{20, 23, 28, 240};
	HXColor TooltipBorder{42, 46, 54, 255};
	HXColor TooltipText{232, 234, 237, 255};

	// Progress
	HXColor ProgressFill{59, 130, 246, 255};
	HXColor ProgressTrack{42, 46, 54, 255};

	// ------------------------------------------------------------
	// Apple-style Design Tokens (Modern UI System)
	// ------------------------------------------------------------
	// Generic surfaces
	HXColor PanelBackground{20, 23, 28, 255};
	HXColor PanelBorder{42, 46, 54, 255};
	HXColor ElevatedBackground{24, 31, 38, 255};
	HXColor ElevatedBorder{42, 46, 54, 255};

	// Generic interaction states
	HXColor HoverBackground{30, 35, 42, 255};
	HXColor HoverBorder{55, 60, 70, 255};
	HXColor SelectedBackground{59, 130, 246, 40};
	HXColor SelectedBorder{59, 130, 246, 255};
	HXColor SelectedText{232, 234, 237, 255};
	HXColor DisabledText{90, 96, 112, 255};

	// Shadow base color
	HXColor ShadowColor{0, 0, 0, 160};

	// Typography colors
	HXColor TextPrimary{232, 234, 237, 255};
	HXColor TextSecondary{139, 144, 152, 255};
	HXColor TextTertiary{90, 96, 112, 255};
	HXColor BorderSubtle{42, 46, 54, 255};
	HXColor AccentPrimary{59, 130, 246, 255};

	int FontSizeSmall = 15;
	int FontSizeCaption = 18;
	int FontSizeBody = 16;
	int FontSizeLabel = 16;
	int FontSizeHeading = 19;
	int FontSizeTitle = 16;

	// Radius tokens
	int RadiusSm = 4;
	int RadiusMd = 6;
	int RadiusLg = 8;
	int RadiusXl = 12;
	int Radius2Xl = 16;

	// Spacing tokens (4px grid)
	int Space1 = 4;
	int Space2 = 8;
	int Space3 = 12;
	int Space4 = 16;
	int Space5 = 20;
	int Space6 = 24;
	int Space8 = 32;
};

/**
 * The runtime context, including all value for UI running
 */
struct HXRuntimeContext {
	std::vector<HXMessage>  MessageQuery;
	std::vector<HXWindow *> Windows;
	HXWindow *              CurrentWindow = nullptr;
	HXContext *             RenderContext = nullptr;
	HXOSOperation *         OSAPI         = nullptr;
	void *                  LocalBuffer   = nullptr;
	HXString                LastError;
	bool                    Initialized = false;
	bool                    Win         = true;

	/**
	 * Per-frame cursor style accumulator.
	 * Controls write their desired cursor here instead of calling SetCursorStyle directly.
	 * The last control to set this wins. Applied once per frame in Render().
	 */
	HXCursorStyle FrameCursor = HXCursorStyle::Normal;

	/**
	 * The layout context stack. CurrentLayout() always returns the back of this stack.
	 * This enables nested containers (Scroller, Panel, Group) to compose correctly.
	 */
	std::vector<HXLayoutContext *> LayoutStack;

	/**
	 * Override flags for the next control item
	 */
	HXPoint NextItemSize   = {-1, -1};
	HXGInt  NextItemMargin = 0;
	bool    HasNextItemSize = false;

	// ------------------------------------------------------------------
	// Focus chain for Tab navigation
	// ------------------------------------------------------------------
	std::vector<void *> FocusChain;
	void *              FocusedProfile = nullptr;
	bool                TabPressed     = false;
	bool                ShiftPressed   = false;

	// ------------------------------------------------------------------
	// Focus Stack for modal / popup / dropdown overlay coordination
	// ------------------------------------------------------------------
	HXFocusStack FocusStack;

	// ------------------------------------------------------------------
	// Event router compatibility layer
	// ------------------------------------------------------------------
	HX::HXEventRouter EventRouter;
};
