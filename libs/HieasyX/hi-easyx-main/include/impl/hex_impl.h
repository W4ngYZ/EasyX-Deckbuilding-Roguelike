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
 * \file hex_impl.h
 * \brief The abstracted API definition
 */

#pragma once

#include <include/hex_geo.h>
#include <include/font/hex_font.h>
#include <include/impl/EasyX/hex_easyx.h>

#include <vector>

#undef DrawText

/**
 * The identity for abstracted API
 */
#define HX_IMPL_API

using HXColInt = uint8_t;

/**
 * The buffer structure
 */
struct HXBuffer {
	HXColInt R;
	HXColInt G;
	HXColInt B;
	HXColInt A;
};

using HXColor     = HXBuffer;
using HXBufferPtr = HXBuffer *;

/**
 * The enum of the shape style
 */
enum class HXLineShapeStyle {
	Solid, Dash, Dotted, DashDotted
};

/**
 * The line style
 */
HX_IMPL_API struct HXLineStyle {
	HXColInt         Thickness = 1;
	HXLineShapeStyle Shape     = HXLineShapeStyle::Solid;
};

/**
 * The abstracted painter for a HXBufferPtr
 */
HX_IMPL_API class HXBufferPainter {
public:
	virtual ~HXBufferPainter() = default;

public:
	/**
	 * Drawing a line on the buffer
	 * @param Point1 The first point of the line
	 * @param Point2 The second point of the line
	 * @param Color The color of the line
	 */
	virtual void DrawLine(HXPoint Point1, HXPoint Point2, HXColor Color) = 0;

	/**
	 * Drawing a styled line on the buffer
	 * @param Point1 The first point of the line
	 * @param Point2 The second point of the line
	 * @param Color The color of the line
	 * @param Style The style of the line
	 */
	virtual void DrawLineStyled(HXPoint Point1, HXPoint Point2, HXColor Color, HXLineStyle Style) = 0;

	/**
	 * Drawing a filled rectangle on the buffer
	 * @param Rect The rectangle to be drawn
	 * @param Color The color of the rectangle
	 * @param FillColor The color to fill the rectangle
	 */
	virtual void DrawFilledRectangle(HXRect Rect, HXColor Color, HXColor FillColor) = 0;

	/**
	 * Drawing a filled rounded rectangle on the buffer
	 * @param Rect The rectangle to be drawn
	 * @param Color The color of the rectangle
	 * @param FillColor The color to fill the rectangle
	 * @param Radius The radius of the rectangle border
	 */
	virtual void DrawFilledRoundedRectangle(HXRect Rect, HXColor Color, HXColor FillColor, HXGInt Radius) = 0;

	/**
	 * Drawing a filled polygon
	 * @param Points Points of the polygon
	 * @param Color The color of the polygon
	 * @param FillColor The color to fill the polygon
	 */
	virtual void DrawFilledPolygon(std::vector<HXPoint> Points, HXColor Color, HXColor FillColor) = 0;

	/**
	 * Drawing a rectangle on the buffer
	 * @param Rect The rectangle to be drawn
	 * @param Color The color of the rectangle
	 */
	virtual void DrawRectangle(HXRect Rect, HXColor Color) = 0;

	/**
	 * Drawing a painter to this painter
	 * @param Painter The painter to be drawn on this painter
	 * @param Where Where to draw the painter
	 */
	virtual void DrawPainter(HXBufferPainter *Painter, HXPoint Where) = 0;

	/**
	 * Drawing a painter to this painter within clipped
	 * @param Painter The painter to be drawn on this painter
	 * @param Where Where to draw the painter
	 * @param Origin The origin point of the source painter
	 * @param Size The clip size of the source painter
	 */
	virtual void DrawPainter(HXBufferPainter *Painter, HXPoint Where, HXPoint Origin, HXPoint Size) = 0;

	/**
	 * Drawing the text on the buffer
	 * @param Text The text to be drawn
	 * @param Font The font using in draw
	 * @param Where Where to be drawn
	 * @param Color The color of the text
	 * @param Height The height of the text
	 */
	virtual void DrawText(const HXString &Text, HXFont Font, HXPoint Where, HXColor Color, HXGUInt Height) = 0;

	/**
	 * Drawing the image at the specified position
	 * @param Buffer The buffer of the image
	 * @param Where Where to draw the image
	 * @param Size The size of the image
	 */
	virtual void DrawImage(void *Buffer, HXPoint Where, HXPoint Size) = 0;

	/**
	 * Measuring the image size
	 * @param Buffer The buffer of the image
	 * @return The size of the image
	 */
	virtual HXPoint MeasureImage(void *Buffer) = 0;

	/**
	 * Clearing the painter with specified color
	 * @param Color The color for clearing
	 */
	virtual void Clear(HXColor Color) = 0;

	/**
	 * Measuring the specified text
	 * @param Text The text to be measured
	 * @param Font The font using to measure
	 * @param Height The height of the font
	 * @return The rectangle of the font measured
	 */
	virtual HXRect MeasureText(const HXString &Text, HXFont Font, HXGUInt Height) = 0;

	/**
	 * Measuring the specified text
	 * @param Text The text to be measured
	 * @param Font The font using to measure
	 * @param Height The height of the font
	 * @return The rectangle of the font measured
	 */
	virtual HXRect MeasureText(const HXChar &Text, HXFont Font, HXGUInt Height) = 0;

public:
	/**
	 * Begin to draw with the current painter
	 */
	virtual void Begin() = 0;

	/**
	 * Drawing ended
	 */
	virtual void End() = 0;

public:
	/**
	 * Creating a sub painter
	 * @param Width The width of the sub buffer painter
	 * @param Height The height of the sub buffer painter
	 * @return The buffer painter pointer
	 */
	virtual HXBufferPainter *CreateSubPainter(HXGInt Width, HXGInt Height) = 0;

	/**
	 * Creating the buffer painter from a buffer pointer
	 * @param Buffer The buffer to be drawn
	 * @return The buffer painter to be drawn
	 */
	virtual HXBufferPainter *CreateFromBuffer(void *Buffer) = 0;

	/**
	 * Getting the underlying buffer pointer of this painter
	 * @return The buffer pointer, or nullptr if not applicable
	 */
	virtual void *GetBuffer() = 0;

	/**
	 * Getting the size (width, height) of this painter's buffer
	 * @return The size in pixels
	 */
	virtual void DrawShadowRect(HXRect Rect, HXColor ShadowColor, HXGInt Radius, HXGInt Blur, HXGInt OffsetX, HXGInt OffsetY) {}

	virtual HXPoint GetSize() = 0;
};

/**
 * The context
 */
HX_IMPL_API class HXContext {
public:
	virtual ~HXContext() = default;

public:
	/**
	 * Creating a buffer painter by a specified buffer
	 * @param Buffer The buffer to be converted
	 * @return The painter for the buffer
	 */
	virtual HXBufferPainter *BufferToPainter(void *Buffer) = 0;

	/**
	 * Getting the default painter
	 * @return The default painter
	 */
	virtual HXBufferPainter *DefaultPainter() = 0;

	/**
	 * Get the buffer of the device
	 * @return The buffer to the main device
	 */
	virtual HXBuffer *GetDeviceBuffer() = 0;
};

struct HXMessage {
	bool   Processed        = false;
	bool   MouseLeftPressed  = false;
	bool   MouseLeftRelease  = false;
	bool   MouseRightPressed = false;
	bool   MouseRightRelease = false;
	bool   MouseMiddlePressed = false;
	bool   MouseMiddleRelease = false;
	bool   MouseWheel       = false;
	bool   MouseWheelUp     = false;
	bool   MouseAction      = false;
	HXGInt MouseX           = 0;
	HXGInt MouseY           = 0;

	bool InShift = false;
	bool InCtrl  = false;
	bool InAlt   = false;

	bool   KeyPressed  = false;
	bool   KeyReleased = false;
	HXGInt KeyCode     = 0;

	bool   InputChar = false;
	HXChar Char;

	// IME support
	bool     IMEStartComposition = false;
	bool     IMEEndComposition   = false;
	bool     IMEComposition      = false;
	HXString IMECompositionString;
	bool     IMEResult = false;
	HXString IMEResultString;
};

HX_IMPL_API class HXMessageSender {
public:
	virtual ~HXMessageSender() = default;

public:
	/**
	 * Convert the message structure to the HiEasyX message structure
	 * @param Message The message pointer to be translated
	 * @return The message translated to the HiEasyX API
	 */
	virtual HXMessage Message(void *Message) = 0;
};

enum class HXCursorStyle {
	Normal,   // Just normal cursor
	Editing,  // |
	ResizeNW, // ↖↘
	ResizeES, // ↙↗
	ResizeN,  // ↑↓
	ResizeE   // ←→
};

HX_IMPL_API class HXOSOperation {
public:
	virtual ~HXOSOperation() = default;

public:
	/**
	 * Setting the style of the cursor
	 * @param Style The style of the cursor
	 */
	virtual void SetCursorStyle(HXCursorStyle Style) = 0;

	/**
	 * Copying string to clipboard
	 * @param String The string to be copied
	 */
	virtual void CopyToClipboard(const HXString &String) = 0;

	/**
	 * Reading string from clipboard
	 * @return The string read from the clipboard
	 */
	virtual HXString ReadFromClipboard() = 0;

	/**
	 * Set the IME composition window / candidate window position
	 * @param Rect The rectangle of the focused text input (screen / client coords)
	 */
	virtual void SetIMERect(const HXRect &Rect) = 0;
	virtual void CancelIMEComposition() = 0;
};
