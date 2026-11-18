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
 * \file hex_impl_easyx.cpp
 * \brief The HiEasyX impl of the EasyX
 */

#include <algorithm>
#include <include/impl/EasyX/hex_impl_easyx.h>
#include <include/impl/Direct2D/hex_impl_direct2d.h>
#include <include/hex_render_backend.h>
#include <include/hex.h>

#include <graphics.h>
#include <iterator>
#include <imm.h>

#pragma comment(lib, "MSIMG32.LIB")
#pragma comment(lib, "imm32.lib")

namespace HX {
void Begin(HXContext *RenderContext);

void OSAPI(HXOSOperation *API);

// IME pending-start flag: set when WM_IME_STARTCOMPOSITION arrives,
// consumed by HXIMEPendingStartComposition().
static bool G_IMEPendingStartComposition = false;

// Global IME state fed by WndProc and consumed by GetHXMessage
static HXString G_IMECompositionString;
static bool     G_IMECompositionPending = false;
static HXString G_IMEResultString;
static bool     G_IMEResultPending = false;
static bool     G_IMEEndPending = false;
static bool     G_IMEStateConsumed = false;

// Last IME rect set by UpdateIMERect (client-area coords).
// UpdateIMEPosition reads this at WM_IME_STARTCOMPOSITION time.
static HXRect   G_CurrentIMERect{0,0,0,0};

extern HXMessageSender *MsgSender;

static WNDPROC OriginalEasyXWndProc = nullptr;

static void UpdateIMEPosition(HWND hwnd) {
	HIMC hIMC = ImmGetContext(hwnd);
	if (!hIMC) return;

	// Use the last known rect that UpdateIMERect computed.
	// This is called at WM_IME_STARTCOMPOSITION time when the IME
	// first decides where to show the candidate window.
	COMPOSITIONFORM cf{};
	cf.dwStyle        = CFS_POINT;
	cf.ptCurrentPos.x = HX::G_CurrentIMERect.Left;
	cf.ptCurrentPos.y = HX::G_CurrentIMERect.Top;
	ImmSetCompositionWindow(hIMC, &cf);

	CANDIDATEFORM cdf{};
	cdf.dwIndex       = 0;
	cdf.dwStyle       = CFS_EXCLUDE;
	cdf.ptCurrentPos  = cf.ptCurrentPos;
	cdf.rcArea        = { G_CurrentIMERect.Left, G_CurrentIMERect.Top,
	                      HX::G_CurrentIMERect.Right, HX::G_CurrentIMERect.Bottom };
	ImmSetCandidateWindow(hIMC, &cdf);

	// Sync font so the IME candidate window matches our text rendering
	LOGFONTW lf{};
	GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
	ImmSetCompositionFontW(hIMC, &lf);

	ImmReleaseContext(hwnd, hIMC);
}

static LRESULT CALLBACK HXEasyXWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_IME_SETCONTEXT:
		if (wParam == TRUE) {
			lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
		}
		break;
	case WM_IME_STARTCOMPOSITION:
		G_IMEPendingStartComposition = true;
		UpdateIMEPosition(hwnd);
		return 0;
	case WM_IME_COMPOSITION: {
		HIMC hIMC = ImmGetContext(hwnd);
		if (hIMC) {
			if (lParam & GCS_RESULTSTR) {
				int bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0);
				if (bytes > 0) {
					G_IMEResultString.resize(bytes / sizeof(wchar_t));
					ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, &G_IMEResultString[0], bytes);
					G_IMEResultPending = true;
				}
			}
			if (lParam & GCS_COMPSTR) {
				int bytes = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, nullptr, 0);
				if (bytes > 0) {
					G_IMECompositionString.resize(bytes / sizeof(wchar_t));
					ImmGetCompositionStringW(hIMC, GCS_COMPSTR, &G_IMECompositionString[0], bytes);
				} else {
					G_IMECompositionString.clear();
				}
				G_IMECompositionPending = true; // always notify so empty comp clears display
			}
			ImmReleaseContext(hwnd, hIMC);
		}
		return 0;
	}
	case WM_IME_ENDCOMPOSITION:
		G_IMEEndPending = true;
		G_IMECompositionString.clear();
		G_IMECompositionPending = false;
		break;
	}
	if (OriginalEasyXWndProc) {
		return CallWindowProcW(OriginalEasyXWndProc, hwnd, msg, wParam, lParam);
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool HXIMEPendingStartComposition() {
	bool pending = G_IMEPendingStartComposition;
	G_IMEPendingStartComposition = false;
	return pending;
}

class HXEasyXRenderBackend : public HXRenderBackend {
public:
	void Init() override {}
	void Shutdown() override {}
	void NewFrame() override {}
	void EndFrame() override {}
	HXBufferPainter *CreatePainter(HXGInt width, HXGInt height) override {
		return new HXExHostedBufferPainterImpl(width, height);
	}
};

void HXBegin() {
	static HXContextImpl     context;
	static HXOSOperationImpl api;
	static HX::HXDirect2DRenderBackend d2dBackend;
	static HXEasyXRenderBackend easyxBackend;
	static bool initialized = false;
	if (!initialized) {
		d2dBackend.Init();
		if (d2dBackend.GetFactory()) {
			SetRenderBackend(&d2dBackend);
		} else {
			easyxBackend.Init();
			SetRenderBackend(&easyxBackend);
		}
		HWND hwnd = GetHWnd();
		if (hwnd && !OriginalEasyXWndProc) {
			OriginalEasyXWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)HXEasyXWndProc);
		}
		initialized = true;
	}
	// Reset IME consumption flag at the start of each frame
	G_IMEStateConsumed = false;
	Begin(&context);
	OSAPI(&api);

	// Ensure MessageQuery always has current cursor position even when peekmessage
	// returns nothing (mouse idle). This keeps hover/drag-draw working.
	// Must route through MsgSender->Message() so global IME state is attached.
	auto &msgQuery = HX::GetContext().MessageQuery;
	if (msgQuery.empty()) {
		ExMessage fake{};
		fake.message = WM_MOUSEMOVE;
		POINT pt;
		if (GetCursorPos(&pt) && ScreenToClient(GetHWnd(), &pt)) {
			fake.x = pt.x;
			fake.y = pt.y;
		}
		if (MsgSender) {
			msgQuery.push_back(MsgSender->Message(GetHXMessage(&fake)));
		} else {
			HXMessage synthetic;
			synthetic.MouseAction = true;
			synthetic.MouseX = pt.x;
			synthetic.MouseY = pt.y;
			msgQuery.push_back(synthetic);
		}
	}
}

void MessageSender(HXMessageSender *Sender);

void CreateTheme();

void HXInitForEasyX() {
	HXMessageSenderImpl *sender = new HXMessageSenderImpl;
	MessageSender(sender);

	CreateTheme();
}

void *GetHXBuffer(IMAGE *Buffer) {
	return static_cast<void *>(Buffer);
}
}

#define HXColorToEasyXColor(COLOR) (RGB(COLOR.R, COLOR.G, COLOR.B))

/////////////////////////////////////////////
/// HXBufferPainterImpl

HXBufferPainterImpl::HXBufferPainterImpl(IMAGE *Buffer) {
	_buffer = Buffer;

	setbkmode(TRANSPARENT);
}

void HXBufferPainterImpl::DrawLine(HXPoint Point1, HXPoint Point2, HXColor Color) {
	setlinecolor(HXColorToEasyXColor(Color));

	line(Point1.X, Point1.Y, Point2.X, Point2.Y);
}

void HXBufferPainterImpl::DrawLineStyled(HXPoint Point1, HXPoint Point2, HXColor Color, HXLineStyle Style) {
	LINESTYLE style;
	LINESTYLE oldStyle;
	getlinestyle(&style);

	oldStyle = style;

	style.thickness = Style.Thickness;
	switch (Style.Shape) {
	case HXLineShapeStyle::Dotted: {
		style.style = PS_DOT;
		break;
	}
	case HXLineShapeStyle::Dash: {
		style.style = PS_DASH;
		break;
	}
	case HXLineShapeStyle::DashDotted: {
		style.style = PS_DASHDOT;
		break;
	}
	default: {
		style.style = PS_SOLID;
		break;
	}
	}

	setlinestyle(&style);
	setlinecolor(HXColorToEasyXColor(Color));

	line(Point1.X, Point1.Y, Point2.X, Point2.Y);

	setlinestyle(&oldStyle);
}

void HXBufferPainterImpl::DrawRectangle(HXRect Rect, HXColor Color) {
	setlinecolor(HXColorToEasyXColor(Color));

	rectangle(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom);
}

void HXBufferPainterImpl::DrawFilledRectangle(HXRect Rect, HXColor Color, HXColor FillColor) {
	setlinecolor(HXColorToEasyXColor(Color));
	setfillcolor(HXColorToEasyXColor(FillColor));

	solidrectangle(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom);
	rectangle(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom);
}

void HXBufferPainterImpl::DrawImage(void *Buffer, HXPoint Where, HXPoint Size) {
	const auto image  = static_cast<IMAGE *>(Buffer);
	const HDC  target = GetImageHDC(GetWorkingImage());
	const HDC  source = GetImageHDC(image);

	constexpr BLENDFUNCTION blendFunction = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

	AlphaBlend(target, Where.X, Where.Y, Size.X, Size.Y, source, 0, 0,
	           image->getwidth(), image->getheight(), blendFunction);
}

HXPoint HXBufferPainterImpl::MeasureImage(void *Buffer) {
	const auto image = static_cast<IMAGE *>(Buffer);

	return {image->getwidth(), image->getheight()};
}

void HXBufferPainterImpl::DrawPainter(HXBufferPainter *Painter, HXPoint Where) {
	putimage(Where.X, Where.Y, static_cast<HXBufferPainterImpl *>(Painter)->_buffer);
}

void HXBufferPainterImpl::DrawPainter(HXBufferPainter *Painter, HXPoint Where, HXPoint Origin, HXPoint Size) {
	putimage(Where.X, Where.Y, Size.X, Size.Y, static_cast<HXBufferPainterImpl *>(Painter)->_buffer, Origin.X,
	         Origin.Y);
}

void HXBufferPainterImpl::SetupEasyXFont(HXFont Font, HXGUInt Height) {
	LOGFONT font;

	gettextstyle(&font);
	_tcscpy_s(font.lfFaceName, Font.Family.c_str());
	font.lfItalic  = Font.Italic;
	font.lfQuality = NONANTIALIASED_QUALITY;

	switch (Font.Style) {
	case HXFontStyle::Regular:
		font.lfWeight = FW_NORMAL;
		break;
	case HXFontStyle::Bold:
		font.lfWeight = FW_BOLD;
		break;
	case HXFontStyle::Black:
		font.lfWeight = FW_BLACK;
		break;
	case HXFontStyle::Light:
		font.lfWeight = FW_LIGHT;
		break;
	case HXFontStyle::Medium:
		font.lfWeight = FW_MEDIUM;

	// Just for fallback
	default:
		font.lfWeight = FW_NORMAL;
		break;
	}
	font.lfHeight = static_cast<LONG>(Height);

	settextstyle(&font);
}

void HXBufferPainterImpl::DrawText(const HXString &Text, HXFont Font, HXPoint Where, HXColor Color, HXGUInt Height) {
	SetupEasyXFont(Font, Height);

	settextcolor(HXColorToEasyXColor(Color));

	outtextxy(Where.X, Where.Y, Text.c_str());
}

void HXBufferPainterImpl::DrawFilledPolygon(std::vector<HXPoint> Points, HXColor Color, HXColor FillColor) {
	setfillcolor(HXColorToEasyXColor(FillColor));
	setlinecolor(HXColorToEasyXColor(Color));

	std::vector<POINT> points;
	points.reserve(Points.size());
	for (auto &point : Points) {
		points.emplace_back(POINT{static_cast<LONG>(point.X), static_cast<LONG>(point.Y)});
	}

	fillpolygon(points.data(), points.size());
}

void HXBufferPainterImpl::DrawFilledRoundedRectangle(HXRect Rect, HXColor Color, HXColor FillColor, HXGInt Radius) {
	setlinecolor(HXColorToEasyXColor(Color));
	setfillcolor(HXColorToEasyXColor(FillColor));

	fillroundrect(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom, Radius, Radius);
}

HXRect HXBufferPainterImpl::MeasureText(const HXString &Text, HXFont Font, HXGUInt Height) {
	SetupEasyXFont(Font, Height);

	return {0, 0, textwidth(Text.c_str()), textheight(Text.c_str())};
}

HXRect HXBufferPainterImpl::MeasureText(const HXChar &Text, HXFont Font, HXGUInt Height) {
	SetupEasyXFont(Font, Height);

	return {0, 0, textwidth(Text), textheight(Text)};
}

void HXBufferPainterImpl::Clear(HXColor Color) {
	setbkcolor(HXColorToEasyXColor(Color));

	cleardevice();
}

HXBufferPainter *HXBufferPainterImpl::CreateSubPainter(HXGInt Width, HXGInt Height) {
	return new HXExHostedBufferPainterImpl(Width, Height);
}

HXBufferPainter *HXBufferPainterImpl::CreateFromBuffer(void *Buffer) {
	return new HXBufferPainterImpl(static_cast<IMAGE *>(Buffer));
}

void *HXBufferPainterImpl::GetBuffer() {
	return _buffer;
}

HXPoint HXBufferPainterImpl::GetSize() {
	if (!_buffer) return {0, 0};
	return {_buffer->getwidth(), _buffer->getheight()};
}

/**
 * The node to store the painter list
 */
struct PainterNode {
	HXBufferPainterImpl *painter = nullptr;
	PainterNode *        next    = nullptr;
	PainterNode *        last    = nullptr;
};

void HXBufferPainterImpl::Begin() {
	SetWorkingImage(_buffer);

	setbkmode(TRANSPARENT);
}

void HXBufferPainterImpl::End() {
	SetWorkingImage();
}

/////////////////////////////////////////////
/// HXExHostedBufferPainterImpl

HXExHostedBufferPainterImpl::HXExHostedBufferPainterImpl(HXGInt Width, HXGInt Height)
	: HXBufferPainterImpl(new IMAGE(Width, Height)) {
}

HXExHostedBufferPainterImpl::~HXExHostedBufferPainterImpl() {
	delete _buffer;
}

/////////////////////////////////////////////
/// HXContextImpl

HXContextImpl::HXContextImpl() {
	_defaultPainter = nullptr;
}

HXContextImpl::~HXContextImpl() {
	delete _defaultPainter;
}

HXBufferPainter *HXContextImpl::DefaultPainter() {
	auto *backend = HX::GetRenderBackend();
	if (auto *d2d = dynamic_cast<HX::HXDirect2DRenderBackend *>(backend)) {
		if (d2d->GetDCRenderTarget()) {
			if (!_defaultPainter || !dynamic_cast<HX::HXDirect2DPainter *>(_defaultPainter)) {
				delete _defaultPainter;
				_defaultPainter = new HX::HXDirect2DPainter(d2d->GetDCRenderTarget(), false);
			} else {
				static_cast<HX::HXDirect2DPainter *>(_defaultPainter)->SetRenderTarget(d2d->GetDCRenderTarget(), false);
			}
			return _defaultPainter;
		}
	}
	if (!_defaultPainter || !dynamic_cast<HXBufferPainterImpl *>(_defaultPainter)) {
		delete _defaultPainter;
		_defaultPainter = new HXBufferPainterImpl(nullptr);
	}
	return _defaultPainter;
}

HXBufferPainter *HXContextImpl::BufferToPainter(void *Buffer) {
	auto *backend = HX::GetRenderBackend();
	if (dynamic_cast<HX::HXDirect2DRenderBackend *>(backend)) {
		return DefaultPainter()->CreateFromBuffer(Buffer);
	}
	return new HXBufferPainterImpl(static_cast<IMAGE *>(Buffer));
}

HXBuffer *HXContextImpl::GetDeviceBuffer() {
	auto *backend = HX::GetRenderBackend();
	if (dynamic_cast<HX::HXDirect2DRenderBackend *>(backend)) {
		return nullptr;
	}
	return reinterpret_cast<HXBuffer *>(GetImageBuffer());
}

/////////////////////////////////////////////
/// HXMessageSenderImpl

HXMessage HXMessageSenderImpl::Message(void *Message) {
	HXMessage message{};
	auto      exMessage = static_cast<ExMessage *>(Message);
	if (exMessage->message == WM_LBUTTONDOWN || exMessage->message == WM_LBUTTONUP) {
		message.MouseAction = true;
	}
	if (exMessage->message == WM_RBUTTONDOWN || exMessage->message == WM_RBUTTONUP) {
		message.MouseAction = true;
	}
	if (exMessage->message == WM_MBUTTONDOWN || exMessage->message == WM_MBUTTONUP) {
		message.MouseAction = true;
	}
	if (exMessage->message == WM_MOUSEMOVE) {
		message.MouseAction = true;
	}
	if (exMessage->message == WM_LBUTTONDOWN) {
		message.MouseLeftPressed = true;
	}
	if (exMessage->message == WM_LBUTTONUP) {
		message.MouseLeftRelease = true;
	}
	if (exMessage->message == WM_RBUTTONDOWN) {
		message.MouseRightPressed = true;
	}
	if (exMessage->message == WM_RBUTTONUP) {
		message.MouseRightRelease = true;
	}
	if (exMessage->message == WM_MBUTTONDOWN) {
		message.MouseMiddlePressed = true;
	}
	if (exMessage->message == WM_MBUTTONUP) {
		message.MouseMiddleRelease = true;
	}
	if (exMessage->message == WM_KEYDOWN) {
		message.KeyPressed = true;
		message.KeyCode    = exMessage->vkcode;
	}
	if (exMessage->message == WM_KEYUP) {
		message.KeyReleased = true;
		message.KeyCode     = exMessage->vkcode;
	}
	if (exMessage->message == WM_CHAR) {
		message.InputChar = true;
		message.Char      = exMessage->ch;
	}
	// Also handle IME messages when peekmessage happens to return them
	if (exMessage->message == WM_IME_STARTCOMPOSITION) {
		message.IMEStartComposition = true;
		HX::G_IMEPendingStartComposition = true;
	}
	if (exMessage->message == WM_IME_ENDCOMPOSITION) {
		message.IMEEndComposition = true;
	}

	// Feed global IME state into HXMessage (consumed once per frame)
	if (!HX::G_IMEStateConsumed) {
		if (HX::G_IMEPendingStartComposition) {
			message.IMEStartComposition = true;
			HX::G_IMEPendingStartComposition = false;
		}
		if (HX::G_IMECompositionPending && !HX::G_IMECompositionString.empty()) {
			message.IMEComposition = true;
			message.IMECompositionString = HX::G_IMECompositionString;
		}
		if (HX::G_IMEResultPending) {
			message.IMEResult = true;
			message.IMEResultString = HX::G_IMEResultString;
			HX::G_IMEResultPending = false;
			HX::G_IMEResultString.clear();
		}
		if (HX::G_IMEEndPending) {
			message.IMEEndComposition = true;
			HX::G_IMEEndPending = false;
		}
		HX::G_IMEStateConsumed = true;
	}
	if (exMessage->message == WM_MOUSEWHEEL) {
		message.MouseAction  = true;
		message.MouseWheel   = true;
		message.MouseWheelUp = exMessage->wheel < 0;
	}
	if (message.MouseAction) {
		message.MouseX = exMessage->x;
		message.MouseY = exMessage->y;
	}

	message.InShift = GetKeyState(VK_SHIFT) & 0x8000 ? true : false;
	message.InCtrl  = GetKeyState(VK_CONTROL) & 0x8000 ? true : false;
	message.InAlt   = GetKeyState(VK_MENU) & 0x8000 ? true : false;

	return message;
}

////////////////////////////////////////////
/// Global

namespace HX {
void *GetHXMessage(ExMessage *Message) {
	return static_cast<void *>(Message);
}
}

void HXOSOperationImpl::SetCursorStyle(HXCursorStyle Style) {
	switch (Style) {
	case HXCursorStyle::Normal: {
		LPCTSTR cursor       = IDC_ARROW;
		auto    cursorHandle = LoadCursor(NULL, cursor);
		SetClassLongPtr(GetHWnd(), GCLP_HCURSOR, reinterpret_cast<LPARAM>(cursorHandle));

		break;
	}
	case HXCursorStyle::Editing: {
		LPCTSTR cursor       = IDC_IBEAM;
		auto    cursorHandle = LoadCursor(NULL, cursor);
		SetClassLongPtr(GetHWnd(), GCLP_HCURSOR, reinterpret_cast<LPARAM>(cursorHandle));

		break;
	}
	case HXCursorStyle::ResizeNW: {
		LPCTSTR cursor       = IDC_SIZENWSE;
		auto    cursorHandle = LoadCursor(NULL, cursor);
		SetClassLongPtr(GetHWnd(), GCLP_HCURSOR, reinterpret_cast<LPARAM>(cursorHandle));

		break;
	}
	case HXCursorStyle::ResizeN: {
		LPCTSTR cursor       = IDC_SIZENS;
		auto    cursorHandle = LoadCursor(NULL, cursor);
		SetClassLongPtr(GetHWnd(), GCLP_HCURSOR, reinterpret_cast<LPARAM>(cursorHandle));

		break;
	}
	case HXCursorStyle::ResizeE: {
		LPCTSTR cursor       = IDC_SIZEWE;
		auto    cursorHandle = LoadCursor(NULL, cursor);
		SetClassLongPtr(GetHWnd(), GCLP_HCURSOR, reinterpret_cast<LPARAM>(cursorHandle));

		break;
	}
	case HXCursorStyle::ResizeES: {
		LPCTSTR cursor       = IDC_SIZENESW;
		auto    cursorHandle = LoadCursor(NULL, cursor);
		SetClassLongPtr(GetHWnd(), GCLP_HCURSOR, reinterpret_cast<LPARAM>(cursorHandle));

		break;
	}
	}
}

#ifdef UNICODE
bool HXOSOperationImpl::WriteClipboardText(const std::wstring &text)
#else
bool HXOSOperationImpl::WriteClipboardText(const std::string &text)
#endif
{
	if (!OpenClipboard(nullptr)) {
		DWORD error = GetLastError();
		return false;
	}

	EmptyClipboard();
	bool bSuccess = false;

#ifdef UNICODE
	const size_t byteSize = (text.size() + 1) * sizeof(wchar_t);
	HGLOBAL      hMem     = GlobalAlloc(GMEM_MOVEABLE, byteSize);
	if (hMem) {
		wchar_t *pMem = static_cast<wchar_t *>(GlobalLock(hMem));
		if (pMem) {
			wcscpy_s(pMem, text.size() + 1, text.c_str());
			GlobalUnlock(hMem);
			if (SetClipboardData(CF_UNICODETEXT, hMem)) {
				bSuccess = true;
			}
		}
		GlobalFree(hMem);
	}
#else
	const size_t         wsize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
	std::vector<wchar_t> wideBuffer(wsize);
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideBuffer.data(), static_cast<int>(wideBuffer.size()));

	const size_t unicodeByteSize = wsize * sizeof(wchar_t);
	HGLOBAL      hUnicodeMem     = GlobalAlloc(GMEM_MOVEABLE, unicodeByteSize);
	if (hUnicodeMem) {
		wchar_t *pUnicodeMem = static_cast<wchar_t *>(GlobalLock(hUnicodeMem));
		if (pUnicodeMem) {
			wcscpy_s(pUnicodeMem, wsize, wideBuffer.data());
			GlobalUnlock(hUnicodeMem);
			SetClipboardData(CF_UNICODETEXT, hUnicodeMem);
			bSuccess = true;
		}
		GlobalFree(hUnicodeMem);
	}
#endif

	CloseClipboard();
	return bSuccess;
}

void HXOSOperationImpl::CopyToClipboard(const HXString &String) {
	WriteClipboardText(String);
}

HXString HXOSOperationImpl::ReadFromClipboard() {
	return ReadClipboardText();
}

#ifdef UNICODE
std::wstring HXOSOperationImpl::ReadClipboardText()
#else
std::string HXOSOperationImpl::ReadClipboardText()
#endif
{
#ifdef UNICODE
	std::wstring result;
#else
	std::string result;
#endif

	if (!OpenClipboard(nullptr)) {
		return result;
	}

	UINT format    = 0;
	bool dataFound = false;

	while ((format = EnumClipboardFormats(format)) != 0 && !dataFound) {
		if (format == CF_UNICODETEXT) {
			HANDLE hData = GetClipboardData(CF_UNICODETEXT);
			if (hData) {
				if (wchar_t *wtext = static_cast<wchar_t *>(GlobalLock(hData))) {
					try {
#ifdef UNICODE
						result = wtext;
#else
						int utf8len = WideCharToMultiByte(
							CP_UTF8, 0, wtext, -1,
							nullptr, 0, nullptr, nullptr);

						if (utf8len > 0) {
							std::vector<char> buffer(utf8len);
							WideCharToMultiByte(
								CP_UTF8, 0, wtext, -1,
								buffer.data(), utf8len, nullptr, nullptr);
							result.assign(buffer.data());
						}
#endif
						dataFound = true;
					} catch (...) {
						GlobalUnlock(hData);
						CloseClipboard();
						throw;
					}
					GlobalUnlock(hData);
				}
			}
		} else if (format == CF_TEXT && !dataFound) {
#ifndef UNICODE
			HANDLE hData = GetClipboardData(CF_TEXT);
			if (hData) {
				if (char *text = static_cast<char *>(GlobalLock(hData))) {
					result    = text;
					dataFound = true;
					GlobalUnlock(hData);
				}
			}
#endif
		}
	}

	CloseClipboard();
	return result;
}

void HXOSOperationImpl::SetIMERect(const HXRect &Rect) {
	HX::G_CurrentIMERect = Rect;

	HWND hwnd = GetHWnd();
	if (!hwnd) return;
	HIMC hIMC = ImmGetContext(hwnd);
	if (!hIMC) return;

	// Coordinates are client-area relative (same as reference code's
	// BOX_X / BOX_Y).  Do NOT convert to screen.
	COMPOSITIONFORM cf{};
	cf.dwStyle = CFS_POINT;
	cf.ptCurrentPos.x = Rect.Left;
	cf.ptCurrentPos.y = Rect.Top;
	ImmSetCompositionWindow(hIMC, &cf);

	CANDIDATEFORM cdf{};
	cdf.dwIndex = 0;
	cdf.dwStyle = CFS_EXCLUDE;
	cdf.ptCurrentPos = cf.ptCurrentPos;
	cdf.rcArea = { Rect.Left, Rect.Top, Rect.Right, Rect.Bottom };
	ImmSetCandidateWindow(hIMC, &cdf);

	ImmReleaseContext(hwnd, hIMC);
}

void HXOSOperationImpl::CancelIMEComposition() {
	// Stub: IME cancellation not yet implemented for EasyX backend
	HWND hwnd = GetHWnd();
	if (hwnd) {
		HIMC himc = ImmGetContext(hwnd);
		if (himc) {
			ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
			ImmReleaseContext(hwnd, himc);
		}
	}
}

#undef line
