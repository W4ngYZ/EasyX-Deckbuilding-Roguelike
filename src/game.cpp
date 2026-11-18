// =======================================
// gui.cpp 图形界面入口（EasyX + hiEasyX）
//
// 只负责渲染、输入与回调装配；不修改逻辑层、不消耗任何随机流。
// =======================================

#define NOMINMAX
#include <windows.h>
#include <graphics.h>
#include <hex.h>
#include <include/impl/EasyX/hex_impl_easyx.h>

#include "hpps/data.hpp"
#include "hpps/battle.hpp"
#include "hpps/run.hpp"
#include "resource.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

// ==================== 常量 / 色板 ====================
constexpr int kWidth = 1280;
constexpr int kHeight = 720;

// 结算画面输入保护期：给后续结算动画留出时间，同时防止战斗最后一下的
// 鼠标松开 / 双击 / 排队的按键穿透到结算按钮。
constexpr int kGameOverInputGuardMs = 1000;

// 设计坐标系固定为 1280x720；实际窗口与绘制按 DPI 缩放。
float g_uiScale = 1.0f;
int g_winW = kWidth;
int g_winH = kHeight;

int scaled(int v) {
    return (int)std::lround(v * g_uiScale);
}

int scaledSize(int v) {
    return std::max(1, (int)std::lround(v * g_uiScale));
}

const COLORREF C_BG          = RGB(30, 26, 36);
const COLORREF C_BG_DARK     = RGB(22, 19, 27);
const COLORREF C_PANEL       = RGB(42, 36, 51);
const COLORREF C_PANEL_DARK  = RGB(34, 29, 42);
const COLORREF C_PANEL_LIGHT = RGB(55, 47, 67);
const COLORREF C_BORDER      = RGB(74, 64, 88);
const COLORREF C_TEXT        = RGB(237, 230, 214);
const COLORREF C_TEXT_DIM    = RGB(168, 158, 142);
const COLORREF C_DISABLED    = RGB(107, 100, 114);
const COLORREF C_PLAYER      = RGB(200, 64, 47);
const COLORREF C_ENERGY      = RGB(232, 179, 58);
const COLORREF C_BLOCK       = RGB(91, 143, 214);
const COLORREF C_ATTACK      = RGB(192, 57, 43);
const COLORREF C_SKILL       = RGB(46, 134, 171);
const COLORREF C_POWER       = RGB(125, 91, 166);
const COLORREF C_WARN        = RGB(224, 90, 78);
const COLORREF C_GOLD        = RGB(232, 179, 58);
const COLORREF C_GREEN       = RGB(89, 170, 110);

// ==================== 基础结构 ====================
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
};

COLORREF mixColor(COLORREF a, COLORREF b, int t) {
    t = std::clamp(t, 0, 255);
    int r = (GetRValue(a) * (255 - t) + GetRValue(b) * t) / 255;
    int g = (GetGValue(a) * (255 - t) + GetGValue(b) * t) / 255;
    int bl = (GetBValue(a) * (255 - t) + GetBValue(b) * t) / 255;
    return RGB(r, g, bl);
}

COLORREF darken(COLORREF c, int amount) {
    int r = std::max(0, GetRValue(c) - amount);
    int g = std::max(0, GetGValue(c) - amount);
    int b = std::max(0, GetBValue(c) - amount);
    return RGB(r, g, b);
}

// ==================== UTF-8 / 文字 ====================
std::wstring utf8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

// 读取剪贴板文本（UTF-8）。剪贴板被占用或无文本时返回空串。
std::string clipboardTextUtf8() {
    if (!OpenClipboard(GetHWnd())) return {};
    std::string out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* w = static_cast<const wchar_t*>(GlobalLock(h))) {
            int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (n > 1) {
                out.resize((size_t)n);
                WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
                out.pop_back();  // 去掉结尾 '\0'
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

// 字体为像素体 Minecraft AE，字号按 8 的倍数量化。
// 依据（实测本机 ttf）：1em = 1024 单位、字形坐标步长 64 单位 → 原生像素网格 16px；
//   16 / 32 / 48 为严格整格（笔画不抖），24 / 40 是 1.5x / 2.5x 折中档。
// 物理字号 = 设计字号 x kFontBoost x g_uiScale，再就近吸附到 8 的倍数，
// 避免 18/21/23 这类非整档尺寸让像素字出现粗细跳跃。
constexpr float kFontBoost = 1.15f;

std::wstring g_fontFace = L"Minecraft AE";  // 运行时探测结果；缺字体时回退雅黑
HANDLE g_fontMem = nullptr;                 // 内存加载的字体句柄（退出时移除）

// GDI 能否真正解析到该家族名：CreateFont 后回读 GetTextFace 比对。
// 不用 EnumFontFamilies，因为它拿不到私有加载的字体。
bool faceAvailableW(const wchar_t* face) {
    HDC dc = GetDC(nullptr);
    if (!dc) return false;
    LOGFONTW lf{};
    lf.lfHeight = -16;
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    HFONT f = CreateFontIndirectW(&lf);
    HGDIOBJ old = SelectObject(dc, f);
    wchar_t got[LF_FACESIZE]{};
    GetTextFaceW(dc, LF_FACESIZE, got);
    bool ok = (_wcsicmp(got, face) == 0);
    SelectObject(dc, old);
    DeleteObject(f);
    ReleaseDC(nullptr, dc);
    return ok;
}

// 启动时调用一次：直接从 exe 内嵌资源（RCDATA）把 ttf 读进进程内存。
// AddFontMemResourceEx 属进程私有字体，不写系统字体库、也不依赖工作目录；
// 资源缺失时退系统已装字体，最后回退雅黑，保证不出方框。
void loadGameFont() {
    HRSRC hr = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_FONT_MINECRAFT), RT_RCDATA);
    if (hr) {
        HGLOBAL gl = LoadResource(nullptr, hr);
        void* p = gl ? LockResource(gl) : nullptr;
        if (p) {
            DWORD n = 0;
            g_fontMem = AddFontMemResourceEx(p, SizeofResource(nullptr, hr), nullptr, &n);
        }
    }
    g_fontFace = faceAvailableW(L"Minecraft AE") ? L"Minecraft AE" : L"Microsoft YaHei";
}

void releaseGameFont() {
    if (g_fontMem) RemoveFontMemResourceEx(g_fontMem);
    g_fontMem = nullptr;
}

// 设计字号 -> 物理字号（8 的倍数，16~128）。
int fontPx(int size) {
    int px = (int)std::lround(size * kFontBoost * g_uiScale);
    px = (int)std::lround(px / 8.0) * 8;
    return std::clamp(px, 16, 128);
}

// 设计坐标下的一行高度：把物理字号换算回设计像素再 +1 留缝，
// 多行文本（意图 / 能力栏 / 弹窗正文 / 卡名）统一用它，避免 24px 文字互相压。
int fontLineH(int size) {
    return (int)std::lround(fontPx(size) / g_uiScale) + 1;
}

void setFont(int size, bool bold = false) {
    LOGFONTW lf{};
    gettextstyle(&lf);
    lf.lfHeight = -fontPx(size);          // 负值 = 字符高（本字体 ascent+descent=em，正负结果一致）
    lf.lfWidth = 0;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = false;
    lf.lfQuality = NONANTIALIASED_QUALITY;  // 像素体必须硬边，抗锯齿会把方块边糊掉
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, g_fontFace.c_str(), LF_FACESIZE);
    settextstyle(&lf);
}

// 返回“设计坐标”下的文字宽度，便于 1280x720 布局继续使用原始尺寸。
int measureW(const std::wstring& s, int size, bool bold = false) {
    if (s.empty()) return 0;
    setFont(size, bold);
    return (int)std::lround(textwidth(s.c_str()) / g_uiScale);
}

// align: 0 = 左对齐，1 = 居中，2 = 右对齐
void drawTextW(int x, int y, const std::wstring& s, int size, COLORREF color, int align = 0) {
    if (s.empty()) return;
    setFont(size);
    settextcolor(color);
    int w = textwidth(s.c_str());
    int xx = scaled(x);
    int yy = scaled(y);
    if (align == 1) xx -= w / 2;
    else if (align == 2) xx -= w;
    outtextxy(xx, yy, s.c_str());
}

void drawText(int x, int y, const std::string& s, int size, COLORREF color, int align = 0) {
    drawTextW(x, y, utf8ToW(s), size, color, align);
}

std::vector<std::wstring> wrapW(const std::wstring& s, int maxW, int size) {
    std::vector<std::wstring> lines;
    if (maxW <= 0) maxW = 1;
    setFont(size);
    const int limit = scaledSize(maxW);
    std::wstring cur;
    for (wchar_t ch : s) {
        if (ch == L'\n') {
            lines.push_back(cur);
            cur.clear();
            continue;
        }
        cur.push_back(ch);
        if (textwidth(cur.c_str()) > limit && cur.size() > 1) {
            cur.pop_back();
            lines.push_back(cur);
            cur.clear();
            cur.push_back(ch);
        }
    }
    if (!cur.empty() || lines.empty()) lines.push_back(cur);
    return lines;
}

std::wstring ellipsizeW(const std::wstring& s, int maxW, int size) {
    setFont(size);
    const int limit = scaledSize(maxW);
    if (textwidth(s.c_str()) <= limit) return s;
    std::wstring out = s;
    while (!out.empty() && textwidth((out + L"..").c_str()) > limit) out.pop_back();
    return out + L"..";
}

std::string ellipsize(const std::string& s, int maxW, int size) {
    std::wstring w = ellipsizeW(utf8ToW(s), maxW, size);
    std::string out;
    if (w.empty()) return out;
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return out;
    out.resize((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}

int drawTextWrapped(int x, int y, const std::string& utf8, int maxW, int size,
                    COLORREF color, int lineH, int maxLines = 99) {
    auto lines = wrapW(utf8ToW(utf8), maxW, size);
    int n = std::min((int)lines.size(), maxLines);
    for (int i = 0; i < n; ++i)
        drawTextW(x, y + i * lineH, lines[i], size, color, 0);
    return n;
}

// ==================== 半透明遮罩 ====================
struct AlphaSurface {
    COLORREF color = 0;
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    void* bits = nullptr;
};

std::vector<AlphaSurface> g_alphaSurfaces;

HDC alphaSurface(COLORREF color) {
    for (auto& s : g_alphaSurfaces)
        if (s.color == color) return s.dc;

    HDC screen = GetImageHDC();
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_winW;
    bi.bmiHeader.biHeight = -g_winH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (mem) DeleteDC(mem);
        return nullptr;
    }
    SelectObject(mem, bmp);
    DWORD* px = static_cast<DWORD*>(bits);
    DWORD v = (255u << 24) |
              ((DWORD)GetRValue(color) << 16) |
              ((DWORD)GetGValue(color) << 8) |
              (DWORD)GetBValue(color);
    for (int i = 0; i < g_winW * g_winH; ++i) px[i] = v;

    g_alphaSurfaces.push_back(AlphaSurface{ color, mem, bmp, bits });
    return mem;
}

void fillAlphaRect(Rect r, COLORREF color, BYTE alpha) {
    if (alpha == 0 || r.w <= 0 || r.h <= 0) return;
    int x = std::clamp(scaled(r.x), 0, g_winW);
    int y = std::clamp(scaled(r.y), 0, g_winH);
    int w = std::min(scaledSize(r.w), g_winW - x);
    int h = std::min(scaledSize(r.h), g_winH - y);
    if (w <= 0 || h <= 0) return;
    HDC mem = alphaSurface(color);
    if (!mem) {
        setfillcolor(color);
        solidrectangle(x, y, x + w - 1, y + h - 1);
        return;
    }
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
    AlphaBlend(GetImageHDC(), x, y, w, h, mem, x, y, w, h, bf);
}

void releaseAlphaSurfaces() {
    for (auto& s : g_alphaSurfaces) {
        if (s.dc) DeleteDC(s.dc);
        if (s.bmp) DeleteObject(s.bmp);
    }
    g_alphaSurfaces.clear();
}

// ==================== 面板 / 按钮 / 血条 ====================
void drawLine(int x1, int y1, int x2, int y2, COLORREF color) {
    setlinecolor(color);
    line(scaled(x1), scaled(y1), scaled(x2), scaled(y2));
}

void drawRect(Rect r, COLORREF color, int borderW = 1) {
    int x = scaled(r.x), y = scaled(r.y);
    int w = scaledSize(r.w), h = scaledSize(r.h);
    int bw = std::max(1, scaledSize(borderW));
    setlinecolor(color);
    for (int i = 0; i < bw; ++i)
        rectangle(x + i, y + i, x + w - 1 - i, y + h - 1 - i);
}

void drawSolidCircle(int x, int y, int radius, COLORREF color) {
    setfillcolor(color);
    solidcircle(scaled(x), scaled(y), scaledSize(radius));
}

void drawCircle(int x, int y, int radius, COLORREF color) {
    setlinecolor(color);
    circle(scaled(x), scaled(y), scaledSize(radius));
}

void panel(Rect r, COLORREF fill = C_PANEL, COLORREF border = C_BORDER, int borderW = 1) {
    if (r.w <= 0 || r.h <= 0) return;
    int x = scaled(r.x), y = scaled(r.y);
    int w = scaledSize(r.w), h = scaledSize(r.h);
    setfillcolor(fill);
    solidrectangle(x, y, x + w - 1, y + h - 1);
    int bw = std::max(1, scaledSize(borderW));
    setlinecolor(border);
    for (int i = 0; i < bw; ++i)
        rectangle(x + i, y + i, x + w - 1 - i, y + h - 1 - i);
}

void outlinePanel(Rect r, COLORREF border = C_BORDER, int borderW = 1) {
    if (r.w <= 0 || r.h <= 0) return;
    drawRect(r, border, borderW);
}

void drawEasyButton(Rect r, const std::string& label, bool hover, bool enabled,
                    bool primary = false) {
    COLORREF fill = enabled ? (primary ? C_ATTACK : C_PANEL_LIGHT) : RGB(47, 42, 55);
    COLORREF border = enabled ? (primary ? mixColor(C_ATTACK, C_TEXT, 40) : C_BORDER) : C_DISABLED;
    if (hover && enabled) fill = mixColor(fill, RGB(255, 255, 255), 34);
    panel(r, fill, border, primary ? 2 : 1);
    setFont(22);
    int th = (int)std::lround(textheight(L"汉") / g_uiScale);
    drawText(r.x + r.w / 2, r.y + (r.h - th) / 2, label, 22,
             enabled ? C_TEXT : C_DISABLED, 1);
}

// 左下牌堆按钮：上行名称，下行实时张数；悬停变亮，风格与底部其他按钮一致。
void drawPileButton(Rect r, const std::string& label, int count, bool hover) {
    COLORREF fill = hover ? mixColor(RGB(44, 37, 55), RGB(255, 255, 255), 34)
                          : RGB(44, 37, 55);
    panel(r, fill, hover ? C_GOLD : C_BORDER);
    // 名称 16px 档（36 宽按钮容不下 24px 的两个汉字），张数 24px 档；用户关心的是张数。
    drawText(r.x + r.w / 2, r.y + 1, label, 11, C_TEXT_DIM, 1);
    drawText(r.x + r.w / 2, r.y + 17, std::to_string(count), 15,
             count > 0 ? C_TEXT : C_DISABLED, 1);
}

void drawHpBar(Rect r, int hp, int maxHp, int block, bool isPlayer) {
    panel(r, RGB(19, 16, 24), C_BORDER);
    int mx = std::max(1, maxHp);
    int clamped = std::clamp(hp, 0, mx);
    int pad = std::max(1, scaledSize(2));
    int px = scaled(r.x), py = scaled(r.y);
    int pw = scaledSize(r.w), ph = scaledSize(r.h);
    int fillW = (pw - pad * 2) * clamped / mx;
    if (fillW > 0) {
        setfillcolor(isPlayer ? C_PLAYER : RGB(178, 68, 54));
        solidrectangle(px + pad, py + pad, px + pad + fillW - 1, py + ph - 1 - pad);
    }
    std::string t = std::to_string(std::max(0, hp)) + "/" + std::to_string(maxHp);
    // 字号 16px 档（血条只有 20 设计高），垂直居中按实际字高计算。
    setFont(12);
    int th = (int)std::lround(textheight(L"汉") / g_uiScale);
    drawText(r.x + r.w / 2, r.y + (r.h - th) / 2, t, 12, C_TEXT, 1);
    if (block > 0)
        drawText(r.right() - 5, r.y + (r.h - th) / 2,
                 "盾" + std::to_string(block), 12, C_BLOCK, 2);
}

// 画状态药丸，返回已排到的底部 y（玩家能力栏据此定位）。
// maxY > 0 时为硬上限：超出就停止，避免状态压到名字 / 血条行。
// 传成立绘底边时，允许状态自然向下覆盖到立绘上方。
int drawStatusPills(const std::map<Status, int>& st, int x, int y, int maxW, int size = 15,
                    int maxY = 0) {
    if (st.empty()) return y;
    int cx = x, cy = y;
    int bottom = y;
    for (const auto& kv : st) {
        std::string t = std::string(statusName(kv.first)) + std::to_string(kv.second);
        int pw = measureW(utf8ToW(t), size) + 10;
        if (cx > x && cx + pw > x + maxW) {
            cx = x;
            cy += size + 11;          // 24px 字一行占 17 设计像素，行距同步放大
        }
        if (maxY > 0 && cy + size + 8 > maxY) break;
        panel(Rect{ cx, cy, pw, size + 8 }, RGB(52, 45, 64), C_BORDER);
        drawText(cx + 5, cy + 3, t, size, C_TEXT);
        cx += pw + 4;
        bottom = cy + size + 8;
    }
    return bottom;
}

// ==================== 卡牌显示辅助 ====================
const char* cardTypeName(CardType t) {
    switch (t) {
    case CardType::Attack: return "攻击";
    case CardType::Skill:  return "技能";
    case CardType::Power:  return "能力";
    }
    return "?";
}

const char* targetName(TargetMode t) {
    switch (t) {
    case TargetMode::Self:        return "自身";
    case TargetMode::SingleEnemy: return "单体";
    case TargetMode::AllEnemies:  return "全体";
    case TargetMode::RandomEnemy: return "随机";
    }
    return "?";
}

COLORREF cardTypeColor(CardType t) {
    switch (t) {
    case CardType::Attack: return C_ATTACK;
    case CardType::Skill:  return C_SKILL;
    case CardType::Power:  return C_POWER;
    }
    return C_PANEL;
}

std::string cardCostLabel(const Card& c, int overrideCost = -1) {
    if (c.xCost) return "X";
    if (overrideCost >= 0) return std::to_string(overrideCost);
    return std::to_string(c.cost);
}

std::string cardFlagText(const Card& c) {
    std::string s;
    if (c.exhaust) s += "消耗 ";
    if (c.ethereal) s += "虚无 ";
    if (c.unplayable) s += "不可打出 ";
    if (c.endTurnDamage > 0) s += "回合末-" + std::to_string(c.endTurnDamage) + " ";
    return s;
}

void drawCardFace(Rect r, const Card& c, bool highlight, bool dim, int nameSize = 18,
                  int overrideCost = -1) {
    COLORREF base = cardTypeColor(c.type);
    if (dim) base = darken(base, 70);
    panel(r, base, highlight ? C_GOLD : C_BORDER, highlight ? 3 : 1);

    Rect inner{ r.x + 4, r.y + 4, r.w - 8, r.h - 8 };
    COLORREF innerFill = dim ? mixColor(darken(base, 45), RGB(25, 22, 30), 200)
                             : mixColor(base, RGB(35, 29, 43), 140);
    panel(inner, innerFill, C_BORDER);

    int costR = (r.w >= 160) ? 20 : 14;
    int costSize = (r.w >= 160) ? 20 : 16;
    int costY = r.y + costR + 4;
    drawSolidCircle(r.x + costR + 5, costY, costR,
                    dim ? darken(C_ENERGY, 60) : C_ENERGY);
    // 费用数字按实际字高居中。
    setFont(costSize);
    int costTh = (int)std::lround(textheight(L"汉") / g_uiScale);
    drawText(r.x + costR + 5, costY - costTh / 2, cardCostLabel(c, overrideCost),
             costSize, RGB(35, 30, 42), 1);

    auto lines = wrapW(utf8ToW(c.name), r.w - 14, nameSize);
    int maxLines = (r.h >= 220) ? 4 : 3;
    int lineCount = std::min((int)lines.size(), maxLines);
    int nameY = r.y + (r.h >= 220 ? 56 : 38);
    // 行距 +7：32px 字一行占 23 设计像素，用 +4 会叠字。
    for (int i = 0; i < lineCount; ++i)
        drawTextW(r.x + r.w / 2, nameY + i * (nameSize + 7), lines[i],
                  nameSize, dim ? C_TEXT_DIM : C_TEXT, 1);

    if (r.h >= 220) {
        drawText(r.x + r.w / 2, r.y + r.h / 2 - 20, cardTypeName(c.type), 30,
                 mixColor(innerFill, C_TEXT, 40), 1);
    }

    int smallSize = (r.w >= 160) ? 16 : 13;
    // Minecraft AE 没有「·」「—」「…」这三个字形（实测渲染成方框），
    // 分隔符统一改用 ASCII 的 "/" 与空格，省略号改用 ".."。
    std::string sub = std::string(cardTypeName(c.type)) + " / " + targetName(c.target);
    // 底部两行按 24px 字排布（手牌 46/27，大卡 60/38）。
    drawText(r.x + r.w / 2, r.bottom() - (r.h >= 220 ? 60 : 46),
             sub, smallSize, C_TEXT_DIM, 1);

    std::string flags = cardFlagText(c);
    if (!flags.empty())
        drawText(r.x + r.w / 2, r.bottom() - (r.h >= 220 ? 38 : 27),
                 flags, smallSize - 2, dim ? C_DISABLED : C_WARN, 1);
}

COLORREF dim(COLORREF c) { return darken(c, 55); }

void drawCardThumb(Rect r, const Card& c) {
    panel(r, dim(cardTypeColor(c.type)), C_BORDER);
    Rect inner{ r.x + 3, r.y + 3, r.w - 6, r.h - 6 };
    panel(inner, mixColor(cardTypeColor(c.type), RGB(35, 29, 43), 150), C_BORDER);
    drawSolidCircle(r.x + 15, r.y + 15, 11, C_ENERGY);
    drawText(r.x + 15, r.y + 7, cardCostLabel(c), 14, RGB(35, 30, 42), 1);
    auto lines = wrapW(utf8ToW(c.name), r.w - 10, 15);
    int n = std::min((int)lines.size(), 2);
    for (int i = 0; i < n; ++i)
        drawTextW(r.x + r.w / 2, r.y + 34 + i * 22, lines[i], 15, C_TEXT, 1);
    drawText(r.x + r.w / 2, r.bottom() - 22, cardTypeName(c.type), 12, C_TEXT_DIM, 1);
}

// ==================== 素材加载（缺图回退占位） ====================
// 为什么不用 putimage 直接贴 PNG：
//   本机 EasyX（26.9.25）的 putimage 不混合 alpha 通道，也不按目标矩形拉伸，
//   实测「8x8 半透明 PNG 拉伸到 32x32」既没有放大、透明像素还被写成黑色。
//   因此这里把 PNG 预先转成「预乘 alpha」的 32bpp DIB 缓存，绘制统一走 AlphaBlend，
//   由 AlphaBlend 自己完成裁切与缩放（目标 DC 用 COLORONCOLOR 保持像素硬边）。
struct AssetImage {
    IMAGE img;
    HDC dc = nullptr;        // 预乘 alpha 的 DIB 内存 DC
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
    int w = 0, h = 0;
    int bx = 0, by = 0, bw = 0, bh = 0;  // alpha 包围盒（width/height 为 0 表示无可画内容）

    ~AssetImage() {
        if (dc) {
            if (oldBmp) SelectObject(dc, oldBmp);
            DeleteDC(dc);
        }
        if (bmp) DeleteObject(bmp);
    }
};

std::map<std::string, std::unique_ptr<AssetImage>> g_assets;
std::set<std::string> g_assetMissing;

// rel 路径 -> 资源 ID：素材与字体全部由 res/resource.rc 打包进 exe，运行时不再读磁盘。
// 键沿用原来的 assets/... 写法，和 rc 清单一一对应；表里没有的路径按缺图处理（走占位回退）。
const std::map<std::string, int>& assetResTable() {
    static const std::map<std::string, int> t = {
        { "assets/bg/menu.png",     IDR_BG_MENU },
        { "assets/bg/map.png",      IDR_BG_MAP },
        { "assets/bg/fail.png",     IDR_BG_FAIL },
        { "assets/bg/success.png",  IDR_BG_SUCCESS },
        { "assets/bg/battle1.png",  IDR_BG_BATTLE1 },
        { "assets/bg/battle2.png",  IDR_BG_BATTLE2 },
        { "assets/bg/battle3.png",  IDR_BG_BATTLE3 },
        { "assets/bg/battle4.png",  IDR_BG_BATTLE4 },
        { "assets/bg/battle5.png",  IDR_BG_BATTLE5 },
        { "assets/bg/battle6.png",  IDR_BG_BATTLE6 },

        { "assets/player/fighter.png", IDR_PLAYER_FIGHTER },

        { "assets/enemies/乐加维林.png",        IDR_EN_LAGAVULIN },
        { "assets/enemies/六火亡魂.png",        IDR_EN_HEXAGHOST },
        { "assets/enemies/劫掠者刺客.png",      IDR_EN_LOOTER_ASSASSIN },
        { "assets/enemies/劫掠者弩手.png",      IDR_EN_LOOTER_CROSSBOW },
        { "assets/enemies/劫掠者斧手.png",      IDR_EN_LOOTER_AXE },
        { "assets/enemies/劫掠者暴徒.png",      IDR_EN_LOOTER_MUGGER },
        { "assets/enemies/劫掠者追踪手.png",    IDR_EN_LOOTER_TRACKER },
        { "assets/enemies/史莱姆老大.png",      IDR_EN_SLIME_BOSS },
        { "assets/enemies/哨卫.png",            IDR_EN_SENTRY },
        { "assets/enemies/地精大块头.png",      IDR_EN_GREMLIN_NOB },
        { "assets/enemies/大颚虫.png",          IDR_EN_JAW_WORM },
        { "assets/enemies/守护者.png",          IDR_EN_GUARDIAN },
        { "assets/enemies/尖刺史莱姆（中）.png", IDR_EN_SPIKED_SLIME_M },
        { "assets/enemies/尖刺史莱姆（大）.png", IDR_EN_SPIKED_SLIME_L },
        { "assets/enemies/尖刺史莱姆（小）.png", IDR_EN_SPIKED_SLIME_S },
        { "assets/enemies/抢劫的.png",          IDR_EN_LOOTER },
        { "assets/enemies/暴徒.png",            IDR_EN_MUGGER },
        { "assets/enemies/真菌兽.png",          IDR_EN_FUNGI_BEAST },
        { "assets/enemies/红奴隶商人.png",      IDR_EN_SLAVER_RED },
        { "assets/enemies/红虱虫.png",          IDR_EN_RED_LOUSE },
        { "assets/enemies/绿虱虫.png",          IDR_EN_GREEN_LOUSE },
        { "assets/enemies/蓝奴隶商人.png",      IDR_EN_SLAVER_BLUE },
        { "assets/enemies/邪教徒.png",          IDR_EN_CULTIST },
        { "assets/enemies/酸液史莱姆（中）.png", IDR_EN_ACID_SLIME_M },
        { "assets/enemies/酸液史莱姆（大）.png", IDR_EN_ACID_SLIME_L },
        { "assets/enemies/酸液史莱姆（小）.png", IDR_EN_ACID_SLIME_S },
        { "assets/enemies/飞蝇菌子.png",        IDR_EN_FLY_FUNGUS },
    };
    return t;
}

int assetResId(const std::string& rel) {
    const auto& t = assetResTable();
    auto it = t.find(rel);
    return it == t.end() ? 0 : it->second;
}

// 把 EasyX 读入的 32bpp 缓冲（0xAARRGGBB）转成预乘 alpha 的 DIB，并统计 alpha 包围盒。
bool buildAssetSurface(AssetImage& a) {
    a.w = a.img.getwidth();
    a.h = a.img.getheight();
    if (a.w <= 0 || a.h <= 0) return false;
    DWORD* src = GetImageBuffer(&a.img);
    if (!src) return false;

    HDC screen = GetImageHDC();
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = a.w;
    bi.bmiHeader.biHeight = -a.h;          // 负高度 = 自上而下，和 EasyX 缓冲同序
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (mem) DeleteDC(mem);
        if (bmp) DeleteObject(bmp);
        return false;
    }
    a.oldBmp = SelectObject(mem, bmp);

    DWORD* dst = static_cast<DWORD*>(bits);
    const int total = a.w * a.h;
    int minX = a.w, minY = a.h, maxX = -1, maxY = -1;
    for (int i = 0; i < total; ++i) {
        DWORD v = src[i];
        DWORD al = (v >> 24) & 0xFF;
        if (al == 0) {
            dst[i] = 0;
            continue;
        }
        DWORD r = (v >> 16) & 0xFF;
        DWORD g = (v >> 8) & 0xFF;
        DWORD b = v & 0xFF;
        dst[i] = (al << 24) | (((r * al) / 255) << 16) |
                 (((g * al) / 255) << 8) | ((b * al) / 255);
        int x = i % a.w, y = i / a.w;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    if (maxX < minX) {
        // 整图没有 alpha（例如 24bit PNG 被读成 A=0）：按完全不透明处理，
        // 否则会被误判成「全透明」而整张图消失。
        for (int i = 0; i < total; ++i) {
            DWORD v = src[i];
            dst[i] = 0xFF000000u | (v & 0x00FFFFFFu);
        }
        a.bx = a.by = 0;
        a.bw = a.w;
        a.bh = a.h;
    } else {
        a.bx = minX;
        a.by = minY;
        a.bw = maxX - minX + 1;
        a.bh = maxY - minY + 1;
    }

    a.dc = mem;
    a.bmp = bmp;
    a.bits = bits;
    return true;
}

AssetImage* getAsset(const std::string& rel) {
    auto it = g_assets.find(rel);
    if (it != g_assets.end()) return it->second.get();
    if (g_assetMissing.count(rel)) return nullptr;

    const int id = assetResId(rel);
    if (id != 0) {
        auto a = std::make_unique<AssetImage>();
        if (loadimage(&a->img, RT_RCDATA, MAKEINTRESOURCEW(id)) == 0 &&
            a->img.getwidth() > 0 && buildAssetSurface(*a)) {
            g_assets[rel] = std::move(a);
            return g_assets[rel].get();
        }
    }
    g_assetMissing.insert(rel);
    return nullptr;
}

// 按顺序尝试多个相对路径，返回第一个命中的（用于帧名回退）。
AssetImage* getAssetFirst(const std::vector<std::string>& rels) {
    for (const auto& r : rels)
        if (AssetImage* a = getAsset(r)) return a;
    return nullptr;
}

// 等比贴合绘制：按 alpha 包围盒裁掉透明边，缩放到 box 内并在水平方向居中；
// alignBottom=true 时贴底（立绘锚点），否则垂直居中（图标 / 特效用）。
// maxW>0 时改用「先按高度贴合，宽度最多到 maxW」——给敌人立绘用：
// 原来按最长边贴合，扁平的素材会被横向撑满、高度只剩一半，导致
// 「小史莱姆比中史莱姆还高」。maxW=0 保持旧的等比贴合行为。
// 全程走 AlphaBlend，透明区域不会被写成黑色。
void drawImageFit(Rect box, const AssetImage* a, bool alignBottom = false, int maxW = 0) {
    if (!a || a->bw <= 0 || a->bh <= 0 || !a->dc) return;
    int boxW = scaledSize(box.w), boxH = scaledSize(box.h);
    if (boxW <= 0 || boxH <= 0) return;
    int limW = maxW > 0 ? scaledSize(maxW) : boxW;
    float k = std::min((float)boxH / a->bh, (float)limW / a->bw);
    if (k <= 0) return;
    int dw = std::max(1, (int)std::lround(a->bw * k));
    int dh = std::max(1, (int)std::lround(a->bh * k));
    int dx = scaled(box.x) + (boxW - dw) / 2;
    int dy = alignBottom ? (scaled(box.y) + boxH - dh)
                         : (scaled(box.y) + (boxH - dh) / 2);
    // 像素图要硬边：COLORONCOLOR 让缩放走最近邻取样，不做平均模糊。
    SetStretchBltMode(GetImageHDC(), COLORONCOLOR);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(GetImageHDC(), dx, dy, dw, dh, a->dc, a->bx, a->by, a->bw, a->bh, bf);
}

// 等比铺满绘制（cover）：背景图用。放大到覆盖整个 box 后居中裁切，四周不留黑边。
// 背景是不透明图，走 StretchBlt 比 AlphaBlend 快。
void drawImageCover(Rect box, const AssetImage* a) {
    if (!a || a->bw <= 0 || a->bh <= 0 || !a->dc) return;
    int boxW = scaledSize(box.w), boxH = scaledSize(box.h);
    if (boxW <= 0 || boxH <= 0) return;
    float k = std::max((float)boxW / a->bw, (float)boxH / a->bh);
    if (k <= 0) return;
    int sw = std::min(a->bw, std::max(1, (int)std::lround(boxW / k)));
    int sh = std::min(a->bh, std::max(1, (int)std::lround(boxH / k)));
    int sx = a->bx + (a->bw - sw) / 2;
    int sy = a->by + (a->bh - sh) / 2;
    SetStretchBltMode(GetImageHDC(), COLORONCOLOR);
    StretchBlt(GetImageHDC(), scaled(box.x), scaled(box.y), boxW, boxH,
               a->dc, sx, sy, sw, sh, SRCCOPY);
}

// 等比完整显示（contain）：整张图放进 box 居中，不裁切，空出的区域露出底层。
// 地图背景用它：羊皮纸四周有烧焦边，cover 会把上下边切掉。
void drawImageContain(Rect box, const AssetImage* a) {
    if (!a || a->bw <= 0 || a->bh <= 0 || !a->dc) return;
    int boxW = scaledSize(box.w), boxH = scaledSize(box.h);
    if (boxW <= 0 || boxH <= 0) return;
    float k = std::min((float)boxW / a->bw, (float)boxH / a->bh);
    if (k <= 0) return;
    int dw = std::max(1, (int)std::lround(a->bw * k));
    int dh = std::max(1, (int)std::lround(a->bh * k));
    int dx = scaled(box.x) + (boxW - dw) / 2;
    int dy = scaled(box.y) + (boxH - dh) / 2;
    SetStretchBltMode(GetImageHDC(), COLORONCOLOR);
    StretchBlt(GetImageHDC(), dx, dy, dw, dh, a->dc, a->bx, a->by, a->bw, a->bh, SRCCOPY);
}

// ==================== 全局状态 ====================
enum class Screen { Menu, Map, GameOver };

Screen g_screen = Screen::Menu;
std::optional<rng::RunSeed> g_run;
RunState g_rs;
RunCallbacks g_cb;
std::chrono::steady_clock::time_point g_gameOverEntered{};

std::vector<std::string> g_logs;
size_t g_nodeLogStart = 0;
int g_goldBeforeNode = 0;
bool g_pendingEnter = false;
bool g_quit = false;
std::string g_seedInput;          // 自定义种子弹窗的输入内容（空 = 随机开局）
std::string g_seedError;          // 种子短码非法时的提示
int g_mouseX = kWidth / 2;
int g_mouseY = kHeight / 2;
int g_battleTurn = 0;
BattleContext* g_activeBattle = nullptr;
// 本场战斗使用的背景名（battle1..battleN）；每场战斗开始时由 pickBattleBgName 决定。
// 缺图时保留 "battle" 作为旧命名回退。
std::string g_battleBgName = "battle";

struct FloatText {
    std::wstring text;
    int x = 0, y = 0;
    COLORREF color = C_TEXT;
    std::chrono::steady_clock::time_point start;
};
std::vector<FloatText> g_floats;

void pushLog(const std::string& s) { g_logs.push_back(s); }

std::string seedCode() {
    return g_run ? g_run->code : std::string("-");
}

void spawnFloat(int x, int y, const std::string& text, COLORREF color) {
    g_floats.push_back(FloatText{ utf8ToW(text), x, y, color,
                                  std::chrono::steady_clock::now() });
}

void updateFloats() {
    auto now = std::chrono::steady_clock::now();
    g_floats.erase(std::remove_if(g_floats.begin(), g_floats.end(),
        [&](const FloatText& f) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - f.start).count() > 900;
        }), g_floats.end());
}

// ==================== 受击 / 突进动画（纯视觉状态） ====================
// 只记录开始时间，绘制时按 steady_clock 现算立绘偏移与显隐；
// 不参与战斗规则、不消耗随机数，过期后在查询时自动清理。
struct HitAnim {
    std::string name;   // 敌人名：分裂重建后同一索引会换成新敌，名字不符即视为过期
    std::chrono::steady_clock::time_point start;
};

constexpr int kHitAnimMs = 160;   // 抖动 / 闪烁总时长

// 攻击突进：55ms 去程（缓出）→ 25ms 停顿 → 80ms 回程（快速离开、缓入原位）。
constexpr int kLungeMs = 160;
constexpr int kLungeForwardMs = 55;
constexpr int kLungeHoldEndMs = 80;
constexpr int kPlayerLungeDx = -24;   // 玩家向左侧敌人区域突进（设计坐标）
constexpr int kEnemyLungeDx = 20;     // 敌人向右侧玩家区域突进（设计坐标）

struct LungeAnim {
    int idx = -1;   // 敌人下标；-1 = 玩家
    std::chrono::steady_clock::time_point start;
};

std::map<int, HitAnim> g_enemyHits;
std::optional<HitAnim> g_playerHit;
std::optional<LungeAnim> g_playerLunge;
std::optional<LungeAnim> g_enemyLunge;   // enemyTurn 同步执行，同一时刻只有一个敌人在突进

// ==================== 卡牌飞出（纯视觉状态） ====================
// 打出确认后、playCard 结算前播放：手牌副本从原位置飞向目标并缩小消失。
// 动画期间 ctx.hand 保持不动，drawHand 通过 handIdx 跳过这张牌。
struct CardFlight {
    Card card;              // 手牌副本：只用于绘制
    int handIdx = -1;       // 原手牌下标（drawHand 跳过）
    Rect from{};            // 起始 Rect（原手牌位置与尺寸）
    int tx = 0, ty = 0;     // 目标中心（设计坐标）
    int toW = 0, toH = 0;   // 结束尺寸（约 65%）
    int cost = -1;          // 卡面显示费用（X 费用牌仍显示 X）
    int nameSize = 18;      // 手牌卡面字号（>9 张时为 13），随缩放等比缩小
    int durationMs = 200;
    std::chrono::steady_clock::time_point start;
};

std::optional<CardFlight> g_cardFlight;

int elapsedMs(std::chrono::steady_clock::time_point start) {
    return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
}

void triggerEnemyHit(int idx, const std::string& name) {
    g_enemyHits[idx] = HitAnim{ name, std::chrono::steady_clock::now() };
}

void triggerPlayerHit() {
    // 已在播放时不重启：突进回调与回合末 diff 会对同一次伤害各触发一次，
    // 重启会让受击反馈变长、看起来像挨了两下。
    if (g_playerHit && elapsedMs(g_playerHit->start) < kHitAnimMs) return;
    g_playerHit = HitAnim{ "", std::chrono::steady_clock::now() };
}

// 每场战斗 / 新开一局时复位，避免跨战斗残留。
void clearBattleAnims() {
    g_enemyHits.clear();
    g_playerHit.reset();
    g_playerLunge.reset();
    g_enemyLunge.reset();
    g_cardFlight.reset();
}

struct HitPose {
    int dx = 0;        // 立绘水平偏移（设计坐标）
    bool draw = true;  // false = 本帧隐藏立绘（闪烁的暗帧）
};

// 位移序列 0 → -5 → +5 → -4 → +3：每 32ms 一档，160ms 时自然回到 0；
// 显隐每 40ms 交替一次（160ms 内闪 2 次），过期后必然恢复显示。
HitPose makeHitPose(int elapsedMs_) {
    HitPose p;
    if (elapsedMs_ < 0 || elapsedMs_ >= kHitAnimMs) return p;
    static const int kDx[] = { 0, -5, 5, -4, 3 };
    p.dx = kDx[elapsedMs_ / (kHitAnimMs / 5)];
    p.draw = (elapsedMs_ / 40) % 2 == 0;
    return p;
}

HitPose enemyHitPose(int idx, const std::string& name) {
    auto it = g_enemyHits.find(idx);
    if (it == g_enemyHits.end()) return {};
    int ms = elapsedMs(it->second.start);
    if (ms >= kHitAnimMs || it->second.name != name) {
        g_enemyHits.erase(it);
        return {};
    }
    return makeHitPose(ms);
}

HitPose playerHitPose() {
    if (!g_playerHit) return {};
    int ms = elapsedMs(g_playerHit->start);
    if (ms >= kHitAnimMs) {
        g_playerHit.reset();
        return {};
    }
    return makeHitPose(ms);
}

// 突进位移：去程缓出（快起慢停），回程按剩余量平方（快速离开、缓入原位），
// 两端都不会是匀速直线。maxDx 为负表示向左。
int lungeDxAt(int ms, int maxDx) {
    if (ms < 0 || ms >= kLungeMs) return 0;
    if (ms < kLungeForwardMs) {
        float p = (float)ms / kLungeForwardMs;
        float ease = 1.0f - (1.0f - p) * (1.0f - p);
        return (int)std::lround(maxDx * ease);
    }
    if (ms < kLungeHoldEndMs) return maxDx;   // 命中停顿
    float p = (float)(ms - kLungeHoldEndMs) / (kLungeMs - kLungeHoldEndMs);
    float rest = 1.0f - p;
    return (int)std::lround(maxDx * rest * rest);
}

int playerLungeDx() {
    if (!g_playerLunge) return 0;
    int ms = elapsedMs(g_playerLunge->start);
    if (ms >= kLungeMs) {
        g_playerLunge.reset();
        return 0;
    }
    return lungeDxAt(ms, kPlayerLungeDx);
}

int enemyLungeDx(int idx) {
    if (!g_enemyLunge) return 0;
    int ms = elapsedMs(g_enemyLunge->start);
    if (ms >= kLungeMs) {
        g_enemyLunge.reset();
        return 0;
    }
    if (g_enemyLunge->idx != idx) return 0;   // 只移动正在行动的这只敌人
    return lungeDxAt(ms, kEnemyLungeDx);
}

// ==================== 输入 ====================
struct InputFrame {
    int mx = 0, my = 0;
    bool click = false;
    bool rightClick = false;
    bool esc = false;
    bool enter = false;
    bool backspace = false;
    bool paste = false;
    bool wheelUp = false;
    bool wheelDown = false;
    std::string chars;  // 本帧 WM_CHAR 输入（种子输入框用）
};

// 鼠标事件来自物理像素窗口，布局与命中判定使用 1280x720 设计坐标。
int toDesignX(int px) { return (int)std::lround(px / g_uiScale); }
int toDesignY(int py) { return (int)std::lround(py / g_uiScale); }

InputFrame pollInput(bool pushToHx) {
    InputFrame f;
    f.mx = g_mouseX;
    f.my = g_mouseY;
    // 只有完整转发过“按下”的松开才交给 HX。战斗等非 HX 界面不转发鼠标，
    // 若最后一下的松开晚一帧到达菜单/结算，HX 按钮会仅凭松开就触发。
    static bool hxLeftDown = false;
    ExMessage msg;
    while (peekmessage(&msg, EX_MOUSE | EX_KEY | EX_CHAR | EX_WINDOW, true)) {
        if (pushToHx) {
            bool forward = true;
            if (msg.message == WM_LBUTTONUP) {
                forward = hxLeftDown;
                hxLeftDown = false;
            } else if (msg.message == WM_LBUTTONDOWN) {
                hxLeftDown = true;
            }
            if (forward) HX::PushMessage(&msg);  // HX 控件按物理坐标工作
        } else if (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP) {
            hxLeftDown = false;  // 非 HX 界面的点击不进入 HX，避免留下孤立松开
        }
        switch (msg.message) {
        case WM_MOUSEMOVE:
            g_mouseX = toDesignX(msg.x);
            g_mouseY = toDesignY(msg.y);
            f.mx = g_mouseX;
            f.my = g_mouseY;
            break;
        case WM_LBUTTONDOWN:
            f.click = true;
            g_mouseX = toDesignX(msg.x);
            g_mouseY = toDesignY(msg.y);
            f.mx = g_mouseX;
            f.my = g_mouseY;
            break;
        case WM_RBUTTONDOWN:
            f.rightClick = true;
            break;
        case WM_KEYDOWN:
            if (msg.vkcode == VK_ESCAPE) f.esc = true;
            if (msg.vkcode == VK_RETURN) f.enter = true;
            if (msg.vkcode == VK_BACK) f.backspace = true;
            if (msg.vkcode == 'V' && (GetKeyState(VK_CONTROL) & 0x8000))
                f.paste = true;  // Ctrl+V
            break;
        case WM_CHAR:
            // 只收 ASCII 可打印字符；base62 过滤在菜单输入框里做。
            if (msg.ch >= 32 && msg.ch < 127)
                f.chars.push_back(static_cast<char>(msg.ch));
            break;
        case WM_MOUSEWHEEL:
            if (msg.wheel > 0) f.wheelUp = true;
            else if (msg.wheel < 0) f.wheelDown = true;
            break;
        case WM_CLOSE:
            g_quit = true;
            break;
        default:
            break;
        }
    }
    return f;
}

void waitFrame(std::chrono::steady_clock::time_point start) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (elapsed < 16) Sleep((DWORD)(16 - elapsed));
}

// ==================== hiEasyX 外壳 ====================
HXColor hxColor(COLORREF c) {
    HXColor col{};
    col.R = (HXColInt)GetRValue(c);
    col.G = (HXColInt)GetGValue(c);
    col.B = (HXColInt)GetBValue(c);
    col.A = 255;
    return col;
}

void applyHxTheme() {
    HX::ApplyModernTheme(HX::HXThemeMode::ModernDark);
    HXTheme& t = HX::GetTheme();
    t.WindowBackground = hxColor(C_PANEL);
    t.WindowTitleBackground = hxColor(C_PANEL);
    t.WindowTitleText = hxColor(C_PANEL);
    t.TextSecondary = hxColor(C_PANEL);      // 隐藏 TitleBarHeight=0 时的折叠图标
    t.TextPrimary = hxColor(C_TEXT);
    t.TextTertiary = hxColor(C_TEXT_DIM);
    t.ButtonBackground = hxColor(C_ATTACK);
    t.ButtonBorder = hxColor(C_ATTACK);
    t.ButtonText = hxColor(C_TEXT);
    t.ButtonOnHoverBackground = hxColor(mixColor(C_ATTACK, RGB(255, 255, 255), 40));
    t.ButtonOnHoverBorder = hxColor(mixColor(C_ATTACK, RGB(255, 255, 255), 40));
    t.ButtonOnHoverText = hxColor(C_TEXT);
    t.ButtonPressedBackground = hxColor(darken(C_ATTACK, 45));
    t.ButtonPressedBorder = hxColor(darken(C_ATTACK, 45));
    t.ButtonPressedText = hxColor(C_TEXT);
    t.DisabledText = hxColor(C_DISABLED);
    t.FontSizeLabel = fontPx(15);   // 弹窗按钮文字走同一套量化（24px 档）
    t.FontSizeTitle = fontPx(15);
}

void hxPanel(Rect r) {
    HX::WindowProfile wp;
    wp.Size = { scaledSize(r.w), scaledSize(r.h) };
    wp.TitleBarHeight = 0;
    wp.Movable = false;
    wp.Sizable = false;
    wp.Folded = false;
    wp.Position = { scaled(r.x), scaled(r.y) };
    HX::Window(L"", wp);
}

bool hxButton(int localX, int localY, int w, int h, const std::string& label,
              HX::ButtonProfile& profile) {
    profile.Size = { scaledSize(w), scaledSize(h) };
    HX::SetLeftMargin(scaled(localX));
    HX::SetBaseLine(scaled(localY));
    return HX::Button(utf8ToW(label), profile);
}

void drawPanelBorder(Rect r) {
    drawRect(r, C_BORDER, 1);
    Rect inner{ r.x + 1, r.y + 1, r.w - 2, r.h - 2 };
    drawRect(inner, mixColor(C_BORDER, C_TEXT, 25), 1);
}

// ==================== 战斗背景挑图 ====================
// 只查资源表与 exe 内是否真有该资源，不解码图片：避免启动时一次性解码全部背景。
bool bgResExists(const std::string& baseName) {
    const int id = assetResId("assets/bg/" + baseName + ".png");
    return id != 0 && FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA) != nullptr;
}

// 候选表：首次需要时探一次 battle1..battle16，缺号自动跳过（增删素材后重编 rc 即可）。
const std::vector<std::string>& battleBgList() {
    static const std::vector<std::string> list = [] {
        std::vector<std::string> out;
        for (int i = 1; i <= 16; ++i) {
            std::string name = "battle" + std::to_string(i);
            if (bgResExists(name)) out.push_back(name);
        }
        return out;
    }();
    return list;
}

// 背景挑图专用的简易随机：不接入 rng.hpp、不消耗任何逻辑随机域，
// 因此同一固定种子的战斗过程与结果完全不受影响（纯视觉选择）。
uint32_t nextBgRand() {
    static uint32_t s =
        (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count() | 1u;
    s = s * 1664525u + 1013904223u;   // 标准 LCG
    return s >> 8;                     // 低 8 位质量差，右移后再取模
}

// 相邻战斗必不同：只在 n-1 个候选里均匀取，再跳过上一次用的下标。
std::string pickBattleBgName() {
    const auto& list = battleBgList();
    if (list.empty()) return "battle";   // 回退到旧命名（缺图时再由渐变兜底）
    static int last = -1;
    const int n = (int)list.size();
    if (n == 1) {
        last = 0;
        return list[0];
    }
    int k = (int)(nextBgRand() % (uint32_t)(n - 1));
    if (last >= 0 && k >= last) ++k;
    last = k;
    return list[k];
}

// 背景：优先用 assets/bg/<屏名>.png；缺图回退为程序渐变。
// name 传 nullptr 时按当前状态自动选：
//   战斗（本场挑好的 battleN）> 地图 > 结算（胜 success / 负 fail）> 菜单。
void drawBackdrop(const char* name = nullptr) {
    if (!name) {
        if (g_activeBattle)              name = g_battleBgName.c_str();
        else if (g_screen == Screen::Map)      name = "map";
        else if (g_screen == Screen::GameOver) name = g_rs.won ? "success" : "fail";
        else                                   name = "menu";
    }
    const std::string bgName = name;
    AssetImage* bg = getAsset("assets/bg/" + bgName + ".png");
    // 结算图可能只做了一半：success / fail 缺图时退回封面，再缺才走渐变。
    if (!bg && (bgName == "success" || bgName == "fail"))
        bg = getAsset("assets/bg/menu.png");
    if (bg) {
        // v3.4/v3.5：地图背景改为「整张完整显示」。
        // 原 cover 会把 1080x720 的羊皮纸按 16:9 铺满，上下各裁掉约 66 设计像素，
        // 顶部烧焦边还会压在 56 高的不透明顶栏下。改为先铺深色底，
        // 再把图 contain 到「上方顶栏中线、下方留 12 设计像素」的区域：
        // 1080x720 → 1020x680，落在 x 130..1150 / y 28..708，四边都完整。
        const bool mapScreen = (g_screen == Screen::Map && !g_activeBattle);
        if (mapScreen) {
            setfillcolor(C_BG_DARK);
            solidrectangle(0, 0, g_winW - 1, g_winH - 1);
            drawImageContain(Rect{ 8, 28, kWidth - 16, kHeight - 28 - 12 }, bg);
        } else {
            drawImageCover(Rect{ 0, 0, kWidth, kHeight }, bg);
        }
        // 按屏压暗：主菜单保留封面原色（0）；战斗 / 地图压暗保证文字对比度；
        // 结算要压在图上，压得更重。
        int dimAlpha = g_activeBattle ? 90
                     : (g_screen == Screen::Map ? 60
                     : (g_screen == Screen::GameOver ? 120 : 0));
        if (dimAlpha > 0)
            fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(16, 14, 22), dimAlpha);
    } else {
        // 必须逐“物理像素行”填充：按设计坐标逐行绘制时，非整数缩放会漏掉部分物理行，
        // 造成上一帧残影穿透（表现为扫描线状色块）。
        for (int y = 0; y < g_winH; ++y) {
            COLORREF c = mixColor(C_BG_DARK, C_BG, y * 255 / g_winH);
            setlinecolor(c);
            line(0, y, g_winW - 1, y);
        }
    }
}

// ==================== 前向声明 ====================
bool guiBattle(BattleContext& ctx);
int guiChooseCard(const ChoiceRequest& req);
int guiChooseReward(const std::vector<Card>& options);
int guiChooseDeck(const std::vector<Card>& deck);
int guiChooseEvent(const std::vector<std::string>& options);
int pickCardModal(const std::string& title, const std::vector<Card>& cards,
                  bool allowCancel, const std::string& footer, bool big);
void cardListModal(const std::string& title, const std::vector<Card>& cards);
void deckModal(const std::vector<Card>& deck);
int eventModal(const std::vector<std::string>& options);
void eventResultModal();
bool confirmModal(const std::string& title, const std::string& text);

// ==================== 战斗布局 ====================
// 分区：
//   怪物区 0..640 / 中央特效区 640..920 / 玩家区 920..1280，纵向 56..520。
constexpr int kFieldTop = 56;
constexpr int kFieldBottom = 520;
constexpr int kEnemyZoneW = 640;
constexpr int kCenterX0 = 640;
constexpr int kCenterX1 = 920;
constexpr int kPlayerX0 = 920;
constexpr int kPlayerZoneW = 360;

const Rect kDeckButton{ 1030, 10, 100, 36 };
// 卡牌大全按钮：战斗 / 地图两处顶栏各一个，绘制与命中判定共用同一矩形。
const Rect kCardLibButton{ 893, 10, 130, 36 };
const Rect kMapCardLibButton{ 866, 10, 130, 36 };
const Rect kEndTurnButton{ 1145, 640, 130, 56 };
// 左下三个牌堆按钮：36x42 并排塞进 124 宽的能量面板；
// 9 张手牌时最左卡在 x=138，不会与按钮命中区重叠。
const Rect kDrawPileButton{ 12, 640, 36, 42 };
const Rect kDiscardPileButton{ 52, 640, 36, 42 };
const Rect kExhaustPileButton{ 92, 640, 36, 42 };

bool anyEnemyInPlay(const BattleContext& ctx) {
    for (const auto& e : ctx.enemies)
        if (e.inPlay()) return true;
    return false;
}

int aliveEnemyCount(const BattleContext& ctx) {
    int n = 0;
    for (const auto& e : ctx.enemies)
        if (e.inPlay()) ++n;
    return n;
}

int firstAliveEnemy(const BattleContext& ctx) {
    for (int i = 0; i < (int)ctx.enemies.size(); ++i)
        if (ctx.enemies[i].inPlay()) return i;
    return -1;
}

struct EnemyLayout {
    Rect intent;
    Rect portrait;
    Rect name;
    Rect hp;
    Rect status;
    int columnX = 0;
    int columnW = 0;
};

// 体型档：普通 168 / 精英 200 / Boss 232（比例 1 : 1.19 : 1.38）。
// 只能按名字判定——core.hpp 的 Enemy 没有类别字段，往逻辑层加字段会改动 hpps 语义。
int enemyClassTarget(const std::string& name) {
    static const char* kElite[] = { "哨卫", "地精大块头", "乐加维林" };
    static const char* kBoss[]  = { "六火亡魂", "守护者", "史莱姆老大" };
    for (const char* s : kElite) if (name == s) return 200;
    for (const char* s : kBoss)  if (name == s) return 232;
    return 168;
}

// 每张图的微调系数（乘在显示框边长上，1.0 = 不调）。
// 原因：AI 出图的长宽比 / 构图差异很大，按 alpha 包围盒贴合后本体失真：
//   扁平素材被横向撑满、高度只剩 45%（中号史莱姆比小号还矮）；
//   邪教徒的包围盒含背后两尊石像和长法杖，本体只占约 60%。
// 数值由 tmp/enemy_scale_test.exe 逐张试出来后导出回填。
float enemyArtScale(const std::string& name) {
    static const std::pair<const char*, float> kTbl[] = {
        { "乐加维林",           1.60f },
        { "六火亡魂",           1.90f },
        { "劫掠者刺客",         1.55f },
        { "劫掠者弩手",         1.50f },
        { "劫掠者斧手",         1.50f },
        { "劫掠者暴徒",         1.55f },
        { "劫掠者追踪手",       1.55f },
        { "史莱姆老大",         1.95f },
        { "哨卫",               1.65f },
        { "地精大块头",         1.80f },
        { "大颚虫",             0.95f },
        { "守护者",             1.95f },
        { "尖刺史莱姆（中）",   1.05f },
        { "尖刺史莱姆（大）",   1.35f },
        { "尖刺史莱姆（小）",   0.75f },
        { "抢劫的",             1.35f },
        { "暴徒",               1.45f },
        { "真菌兽",             1.00f },
        { "红奴隶商人",         1.70f },
        { "红虱虫",             1.00f },
        { "绿虱虫",             1.00f },
        { "蓝奴隶商人",         1.55f },
        { "邪教徒",             1.60f },
        { "酸液史莱姆（中）",   1.00f },
        { "酸液史莱姆（大）",   1.45f },
        { "酸液史莱姆（小）",   0.65f },
        { "飞蝇菌子",           1.40f },
    };
    for (const auto& kv : kTbl)
        if (name == kv.first) return kv.second;
    return 1.0f;
}

// 立绘最高只能到 y=120（意图框 44 高，下沿 108），否则会盖住意图框和顶栏。
constexpr int kEnemyArtTopLimit = 120;

EnemyLayout enemyLayout(int i, int n, int target) {
    if (n <= 0) n = 1;
    // 目标边长按类别给，再被列宽压制：4 只同场时三档都塌到 148。
    int colW = kEnemyZoneW / n;
    int x = i * colW;
    int ps = std::clamp(std::min(target, colW - 12), 96, 232);
    int cx = x + colW / 2;

    // 纵向顺序：意图 → 效果 → 立绘 → 名字 → 血条。
    // 血条贴住底部控制条（496..516，留 4px），立绘脚底统一 468，与玩家同一条线。
    // 效果从 108 起向下自由流动，最多压到立绘底边（468），不会碰名字和血条。
    const int kArtBottom = 468;
    const int kNameY = 470;
    const int kHpY = 496, kHpH = 20;

    EnemyLayout L;
    L.columnX = x;
    L.columnW = colW;
    L.intent = Rect{ x + 6, 64, colW - 12, 44 };                 // 高 44，容纳 24px 两行意图
    L.status = Rect{ x + 8, 112, colW - 16, kArtBottom - 112 };  // 高度仅作参考，实际向下流动
    L.portrait = Rect{ cx - ps / 2, kArtBottom - ps, ps, ps };
    L.name = Rect{ x + 2, kNameY, colW - 4, 22 };
    L.hp = Rect{ x + 8, kHpY, colW - 16, kHpH };
    return L;
}

Rect handCardRect(int i, int n) {
    if (n <= 0) n = 1;
    // 卡面 140x195，y 522，pitch 上限 108。
    // 9 张时总跨度 138..1142，左不压能量面板（≤132）、右不压回合结束按钮（≥1148）。
    const int A = 1000;
    const int W = 140, H = 195, kPitchMax = 108;
    int w = W;
    int pitch = 0;
    if (n <= 9) {
        pitch = std::min(kPitchMax, A / n);
    } else {
        pitch = A / n;
        w = std::clamp(pitch - 6, 96, W);
    }
    int cx = 640 + (int)std::lround((i - (n - 1) / 2.0) * pitch);
    return Rect{ cx - w / 2, 522, w, H };
}

int displayCostFor(const BattleContext& ctx, const Card& c) {
    if (ctx.corruption && c.type == CardType::Skill) return 0;  // 腐化：技能 0 费（X 费用技能 X=0）
    if (c.xCost) return -1;  // 显示 X
    if (c.attackCostDiscount) return std::max(0, c.cost - ctx.attacksPlayed);
    return c.cost;
}

bool guiPlayable(BattleContext& ctx, const Card& c) {
    if (c.unplayable) return false;
    if (c.type == CardType::Attack && ctx.player.getStatus(Status::Entangle) > 0) return false;
    if (c.canPlay && !c.canPlay(ctx)) return false;
    int cost = c.xCost ? ctx.energy : displayCostFor(ctx, c);
    return ctx.energy >= cost;
}

int handIndexAt(BattleContext& ctx, int mx, int my) {
    int n = (int)ctx.hand.size();
    for (int i = n - 1; i >= 0; --i) {
        Rect r = handCardRect(i, n);
        if (r.contains(mx, my)) return i;
    }
    return -1;
}

int enemyIndexAt(BattleContext& ctx, int mx, int my) {
    int n = (int)ctx.enemies.size();
    for (int i = 0; i < n; ++i) {
        if (!ctx.enemies[i].inPlay()) continue;
        EnemyLayout L = enemyLayout(i, n, enemyClassTarget(ctx.enemies[i].name));
        Rect col{ L.columnX, kFieldTop, L.columnW, kFieldBottom - kFieldTop };
        if (col.contains(mx, my)) return i;
    }
    return -1;
}

// ==================== 战斗绘制 ====================
// 立绘直接画在分区底板上（不再套一层内框），透明区域会透出分区底色。
void drawPlayerArt(Rect r) {
    AssetImage* img = getAssetFirst({ "assets/player/fighter_idle_00.png",
                                      "assets/player/fighter.png" });
    if (img) {
        drawImageFit(r, img, true);   // 贴底对齐：脚踩在框底
        return;
    }
    panel(r, darken(C_PLAYER, 85), C_BORDER);
    drawText(r.x + r.w / 2, r.y + r.h / 2 - 30, "铁甲", 22, C_TEXT, 1);
    drawText(r.x + r.w / 2, r.y + r.h / 2 + 6, "战士", 22, C_TEXT, 1);
}

void drawEnemyArt(const Enemy& e, Rect r, int maxW = 0) {
    AssetImage* img = getAssetFirst({ "assets/enemies/" + e.name + "_idle_00.png",
                                      "assets/enemies/" + e.name + ".png" });
    if (img) {
        drawImageFit(r, img, true, maxW);   // 高度贴合 + 贴底，宽度上限见 maxW
    } else {
        // 缺图时只画一块底板：名字由下方独立的名字行显示，这里不再重复。
        panel(r, RGB(62, 52, 78), C_BORDER);
    }
}

struct BattleView {
    int hoverHand = -1;
    int aimHand = -1;
    int hoverEnemy = -1;
    bool aiming = false;
};

void drawBattleTopBar(BattleContext& ctx) {
    panel(Rect{ 0, 0, kWidth, 56 }, C_PANEL_DARK, C_BORDER);
    drawText(16, 6, "种子 " + seedCode(), 12, C_TEXT_DIM);
    std::string node = "节点 " + std::to_string(g_rs.pos + 1) + "/" +
                       std::to_string(g_rs.map.size());
    drawText(16, 26, node + "    回合 " + std::to_string(g_battleTurn),
             12, C_TEXT);
    // 金币 32px、牌组 24px；两行按行高排布，避免压到 56 高的顶栏下沿。
    drawText(1264, 4, "金币 " + std::to_string(ctx.gold ? *ctx.gold : g_rs.gold),
             20, C_ENERGY, 2);
    drawText(1264, 32, "牌组 " + std::to_string(ctx.player.deck.size()),
             15, C_TEXT_DIM, 2);
    bool hov = kDeckButton.contains(g_mouseX, g_mouseY);
    drawEasyButton(kDeckButton, "牌组", hov, true);
    bool libHov = kCardLibButton.contains(g_mouseX, g_mouseY);
    drawEasyButton(kCardLibButton, "卡牌大全", libHov, true);
}

// 系数放大后的实际边长：顶部不得高于 kEnemyArtTopLimit（否则会盖住意图框）。
int enemyArtSide(const Enemy& e, const EnemyLayout& L) {
    int side = (int)std::lround(L.portrait.h * enemyArtScale(e.name));
    return std::min(side, L.portrait.bottom() - kEnemyArtTopLimit);
}

// 按每张图的系数放大/缩小绘制框，底边锚点（468）保持不变。
Rect makeEnemyArtRect(const Enemy& e, const EnemyLayout& L) {
    int side = enemyArtSide(e, L);
    return Rect{ L.portrait.x + (L.portrait.w - side) / 2,
                 L.portrait.bottom() - side, side, side };
}

// 高度贴合的宽度上限：1.5 倍框宽，但不越过本列，避免压住相邻怪物。
int enemyArtMaxW(const Enemy& e, const EnemyLayout& L) {
    int side = enemyArtSide(e, L);
    return std::min((int)std::lround(side * 1.5f), L.columnW - 4);
}

void drawEnemyArea(BattleContext& ctx, const BattleView& view) {
    int n = (int)ctx.enemies.size();
    if (n == 0) return;
    for (int i = 0; i < n; ++i) {
        const Enemy& e = ctx.enemies[i];
        if (!e.inPlay()) {           // 死亡 / 逃走的敌人整块消失，不留任何标记
            g_enemyHits.erase(i);
            continue;
        }
        EnemyLayout L = enemyLayout(i, n, enemyClassTarget(e.name));

        panel(L.intent, RGB(44, 36, 52), C_BORDER);
        std::string desc = e.current.desc.empty() ? "-" : e.current.desc;
        const int intentSize = 13;   // 统一 24px 档（按人数分档量化后会掉到 16px）
        drawTextWrapped(L.intent.x + 4, L.intent.y + 3, desc, L.intent.w - 8,
                        intentSize, C_TEXT, fontLineH(intentSize), 2);

        if (view.aiming) {
            COLORREF c = view.hoverEnemy == i ? C_GOLD : mixColor(C_GOLD, C_BORDER, 120);
            int pad = view.hoverEnemy == i ? 5 : 3;
            Rect glow{ L.portrait.x - pad, L.portrait.y - pad,
                       L.portrait.w + pad * 2, L.portrait.h + pad * 2 };
            drawRect(glow, c, view.hoverEnemy == i ? 2 : 1);
        }
        // 受击 / 突进动画只偏移 / 隐藏立绘本身，意图框、名字、血条、状态栏保持原位。
        HitPose hit = enemyHitPose(i, e.name);
        if (hit.draw) {
            Rect art = makeEnemyArtRect(e, L);
            art.x += hit.dx + enemyLungeDx(i);
            drawEnemyArt(e, art, enemyArtMaxW(e, L));
        }

        int nameSize = (n >= 4) ? 12 : (n >= 3 ? 15 : 19);   // 16 / 24 / 32px 三档
        drawText(L.name.x + L.name.w / 2, L.name.y,
                 ellipsize(e.name, L.name.w, nameSize), nameSize,
                 C_TEXT, 1);
        drawHpBar(L.hp, e.hp, e.maxHp, e.block, false);
        drawStatusPills(e.status, L.status.x, L.status.y, L.status.w, 15,
                        L.portrait.bottom());
    }
}

// 玩家立绘区域（设计坐标）：脚底与怪物统一 468（252+216）。
const Rect kPlayerPortrait{ 940, 252, 320, 216 };

void drawPlayerArea(BattleContext& ctx) {
    // 纵向顺序：效果 → 能力 → 立绘 → 名字 → 血条。
    Rect portrait = kPlayerPortrait;
    // 受击 / 突进动画只偏移 / 隐藏立绘本身，名字、血条、状态栏保持原位。
    HitPose hit = playerHitPose();
    if (hit.draw) {
        Rect art = portrait;
        art.x += hit.dx + playerLungeDx();
        drawPlayerArt(art);
    }
    drawText(portrait.x + portrait.w / 2, 470, ctx.player.name, 19, C_TEXT, 1);
    drawHpBar(Rect{ 928, 496, 336, 20 }, ctx.player.hp, ctx.player.maxHp,
              ctx.player.block, true);

    // 效果从 64 起向下流动，可以压到立绘上（252 以后）；
    // 上限 392 是为了给下面的能力栏留出 4 行（398 起，到 462 结束，不碰名字行）。
    int statusBottom = drawStatusPills(ctx.player.status, 928, 64, 336, 15, 392);

    std::string powers;
    auto addPower = [&](const char* name, bool on) {
        if (on) powers += std::string(name) + " ";
    };
    addPower("壁垒", ctx.barricade);
    addPower("腐化", ctx.corruption);
    addPower("巨像", ctx.colossus);
    addPower("火焰屏障", ctx.flameBarrier);
    addPower("狂怒", ctx.rage);
    addPower("狱火", ctx.infernoFire);
    addPower("地狱狂徒", ctx.hellbound);
    addPower("连环拳", ctx.doubleNextAttack);
    addPower("坚定不移", ctx.steadfast);
    addPower("势不可当", ctx.relentless > 0);
    addPower("惊逃", ctx.flee);
    if (ctx.feelNoPain > 0) powers += "无惧疼痛" + std::to_string(ctx.feelNoPain) + " ";
    if (ctx.darkEmbrace > 0) powers += "黑暗之拥" + std::to_string(ctx.darkEmbrace) + " ";
    if (ctx.demonForm > 0) powers += "恶魔形态" + std::to_string(ctx.demonForm) + " ";
    if (ctx.metallize > 0) powers += "金属化" + std::to_string(ctx.metallize) + " ";
    if (ctx.pyreSource > 0) powers += "薪火" + std::to_string(ctx.pyreSource) + " ";
    if (ctx.juggle > 0) powers += "杂耍" + std::to_string(ctx.juggle) + " ";
    if (ctx.cruelty > 0) powers += "残酷" + std::to_string(ctx.cruelty) + " ";
    if (!powers.empty())
        drawTextWrapped(928, statusBottom + 4, powers, 336, 15, C_TEXT_DIM,
                        fontLineH(15), 4);
}

void drawLogPanel() {
    // 日志放在中央特效区 640..920，宽 264。
    Rect r{ 648, 372, 264, 140 };
    fillAlphaRect(r, RGB(12, 10, 16), 190);
    outlinePanel(r, C_BORDER);
    // 日志面板只有 264 宽，24px 字每行只能显示 13 个字（长句全被截断），
    // 因此正文保持 16px 档、行距 15，能多显示两行；标题同步降到 16px。
    drawText(r.x + 10, r.y + 5, "战斗日志", 11, C_TEXT_DIM);
    int start = std::max(0, (int)g_logs.size() - 7);
    for (int i = start; i < (int)g_logs.size(); ++i) {
        int line = i - start;
        drawText(r.x + 10, r.y + 26 + line * 15,
                 ellipsize(g_logs[i], r.w - 20, 12), 12, C_TEXT, 0);
    }
}

void drawFloats() {
    auto now = std::chrono::steady_clock::now();
    for (const auto& f : g_floats) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - f.start).count();
        int y = f.y - (int)(ms / 30);
        drawTextW(f.x, y, f.text, 20, f.color, 1);
    }
}

void drawBattleBottom(BattleContext& ctx) {
    panel(Rect{ 0, 520, kWidth, 200 }, C_PANEL_DARK, C_BORDER);

    // 左控制：能量 + 抽牌 / 弃牌 / 消耗三个牌堆按钮（点击弹出只读列表）
    panel(Rect{ 8, 530, 124, 180 }, RGB(33, 28, 41), C_BORDER);
    int orbs = std::max(1, ctx.player.maxEnergy);
    int shown = std::min(orbs, 4);
    for (int i = 0; i < shown; ++i) {
        int cx = 30 + i * 30;
        bool full = i < ctx.energy;
        drawSolidCircle(cx, 562, 13, full ? C_ENERGY : RGB(63, 55, 72));
        drawCircle(cx, 562, 13, full ? mixColor(C_ENERGY, C_TEXT, 40) : C_BORDER);
    }
    drawText(70, 588, "能量 " + std::to_string(ctx.energy) + "/" +
             std::to_string(ctx.player.maxEnergy), 15, C_TEXT, 1);
    if (ctx.energy > ctx.player.maxEnergy)
        drawText(70, 609, "+" + std::to_string(ctx.energy - ctx.player.maxEnergy),
                 12, C_ENERGY, 1);
    drawPileButton(kDrawPileButton, "抽牌", (int)ctx.drawPile.size(),
                   kDrawPileButton.contains(g_mouseX, g_mouseY));
    drawPileButton(kDiscardPileButton, "弃牌", (int)ctx.discardPile.size(),
                   kDiscardPileButton.contains(g_mouseX, g_mouseY));
    drawPileButton(kExhaustPileButton, "消耗", (int)ctx.exhaustPile.size(),
                   kExhaustPileButton.contains(g_mouseX, g_mouseY));

    // 右控制：牌堆张数摘要 + 结束回合（可点击入口在左下三个按钮）
    panel(Rect{ 1148, 530, 124, 92 }, RGB(33, 28, 41), C_BORDER);
    drawText(1158, 540, "弃牌 " + std::to_string(ctx.discardPile.size()), 15, C_TEXT);
    drawText(1158, 562, "消耗 " + std::to_string(ctx.exhaustPile.size()), 15, C_TEXT);
    drawText(1158, 584, "抽牌 " + std::to_string(ctx.drawPile.size()), 15, C_TEXT_DIM);
    bool hov = kEndTurnButton.contains(g_mouseX, g_mouseY);
    drawEasyButton(kEndTurnButton, "回合结束", hov, true, true);
}

void drawHand(BattleContext& ctx, const BattleView& view) {
    int n = (int)ctx.hand.size();
    if (n == 0) {
        drawText(640, 620, "手牌为空", 15, C_DISABLED, 1);
        return;
    }
    int hover = view.hoverHand;
    // 正在飞出的牌仍留在 ctx.hand 里，这里临时跳过，避免出现两张相同的卡。
    const int flying = g_cardFlight ? g_cardFlight->handIdx : -1;
    for (int i = 0; i < n; ++i) {
        if (i == hover || i == flying) continue;
        Rect r = handCardRect(i, n);
        bool playable = guiPlayable(ctx, ctx.hand[i]);
        bool highlight = (view.aimHand == i);
        drawCardFace(r, ctx.hand[i], highlight, !playable, n > 9 ? 13 : 18,
                     displayCostFor(ctx, ctx.hand[i]));
    }
    if (hover >= 0 && hover < n && hover != flying) {
        Rect r = handCardRect(hover, n);
        r.y -= 20;
        bool playable = guiPlayable(ctx, ctx.hand[hover]);
        drawCardFace(r, ctx.hand[hover], true, !playable, n > 9 ? 13 : 18,
                     displayCostFor(ctx, ctx.hand[hover]));
        if (!playable)
            drawText(r.x + r.w / 2, r.bottom() - 42, "无法打出", 12, C_DISABLED, 1);
    }
}

// 抽牌堆的只读展示副本：按「类型 → 名称 → 费用」稳定排序。
// drawPile.back() 是下一张要抽的牌，直接按原顺序展示会剧透；
// 这里只重排展示副本，不修改 ctx.drawPile，也不使用任何随机数。
std::vector<Card> drawPileDisplay(const BattleContext& ctx) {
    std::vector<Card> view = ctx.drawPile;
    std::stable_sort(view.begin(), view.end(), [](const Card& a, const Card& b) {
        if (a.type != b.type) return (int)a.type < (int)b.type;
        if (a.name != b.name) return a.name < b.name;
        return a.cost < b.cost;
    });
    return view;
}

// 飞出的手牌副本：从原手牌位置平滑移向目标中心，同时缩到约 65%，结束后自动清理。
// 只读取 g_cardFlight，不碰 ctx.hand / 能量 / 任何游戏数据。
void drawCardFlight() {
    if (!g_cardFlight) return;
    int ms = elapsedMs(g_cardFlight->start);
    if (ms >= g_cardFlight->durationMs) {
        g_cardFlight.reset();
        return;
    }
    const CardFlight& f = *g_cardFlight;
    float p = (float)ms / f.durationMs;
    float e = p * p * (3.0f - 2.0f * p);   // smoothstep：两端慢、中间快
    float cx = f.from.x + f.from.w / 2.0f;
    float cy = f.from.y + f.from.h / 2.0f;
    cx += (f.tx - cx) * e;
    cy += (f.ty - cy) * e;
    int w = (int)std::lround(f.from.w + (f.toW - f.from.w) * e);
    int h = (int)std::lround(f.from.h + (f.toH - f.from.h) * e);
    Rect r{ (int)std::lround(cx - w / 2.0f), (int)std::lround(cy - h / 2.0f), w, h };
    // 名字字号随卡面宽度等比缩小，避免 65% 尺寸下文字挤出卡框。
    int nameSize = std::max(9, (int)std::lround(f.nameSize * (float)w / f.from.w));
    drawCardFace(r, f.card, false, false, nameSize, f.cost);
}

void drawBattle(BattleContext& ctx, const BattleView& view) {
    drawBackdrop();
    drawBattleTopBar(ctx);
    // 分区底板半透明，让战斗背景透出来；描边保留以维持分区感。
    Rect enemyZone{ 0, kFieldTop, kEnemyZoneW, kFieldBottom - kFieldTop };
    Rect playerZone{ kPlayerX0, kFieldTop, kPlayerZoneW, kFieldBottom - kFieldTop };
    fillAlphaRect(enemyZone, RGB(32, 27, 38), 165);
    outlinePanel(enemyZone, C_BORDER);
    fillAlphaRect(playerZone, RGB(32, 27, 38), 165);
    outlinePanel(playerZone, C_BORDER);
    drawEnemyArea(ctx, view);
    drawPlayerArea(ctx);
    drawLogPanel();
    drawFloats();
    drawBattleBottom(ctx);
    drawHand(ctx, view);
    if (view.aiming) {
        Rect banner{ 420, 62, 440, 30 };
        fillAlphaRect(banner, C_ATTACK, 190);
        drawText(banner.x + banner.w / 2, banner.y + 5,
                 "选择目标  右键 / Esc 取消", 13, C_TEXT, 1);
    }
    drawCardFlight();   // 飞出的卡牌副本压在所有战斗 UI 之上
}

// ==================== 数值快照 / 伤害数字 ====================
struct EntSnap { int hp = 0; int block = 0; };

std::vector<EntSnap> snapEnemies(const BattleContext& ctx) {
    std::vector<EntSnap> snaps;
    for (const auto& e : ctx.enemies) snaps.push_back({ e.hp, e.block });
    return snaps;
}

EntSnap snapPlayer(const BattleContext& ctx) {
    return { ctx.player.hp, ctx.player.block };
}

// blockTriggersHit=false 用于敌人回合结束后的对比：敌人行动前会把自己的格挡清零，
// 那不是受击，不能让立绘误闪。
void diffEnemies(const BattleContext& ctx, const std::vector<EntSnap>& before,
                 bool blockTriggersHit = true) {
    int n = (int)ctx.enemies.size();
    for (int i = 0; i < n && i < (int)before.size(); ++i) {
        if (!ctx.enemies[i].inPlay()) continue;
        EnemyLayout L = enemyLayout(i, n, enemyClassTarget(ctx.enemies[i].name));
        int cx = L.portrait.x + L.portrait.w / 2;
        int cy = L.portrait.y + 20;
        bool hpDown = ctx.enemies[i].hp < before[i].hp;
        bool blockDown = ctx.enemies[i].block < before[i].block;
        if (hpDown || (blockTriggersHit && blockDown))
            triggerEnemyHit(i, ctx.enemies[i].name);
        if (hpDown)
            spawnFloat(cx, cy, std::to_string(ctx.enemies[i].hp - before[i].hp), C_WARN);
        else if (ctx.enemies[i].block > before[i].block)
            spawnFloat(cx, cy, "+" + std::to_string(ctx.enemies[i].block - before[i].block), C_BLOCK);
    }
}

void diffPlayer(const BattleContext& ctx, const EntSnap& before) {
    // HP 减少或格挡被消耗（完全挡下伤害）都算受击；格挡增加不算。
    if (ctx.player.hp < before.hp || ctx.player.block < before.block)
        triggerPlayerHit();
    if (ctx.player.hp < before.hp)
        spawnFloat(1100, 100, std::to_string(ctx.player.hp - before.hp), C_WARN);
    else if (ctx.player.block > before.block)
        spawnFloat(1100, 100, "+" + std::to_string(ctx.player.block - before.block), C_BLOCK);
}

// ==================== 通用界面部件 ====================
void drawModalHeader(Rect box, const std::string& title, const std::string& right = "",
                     const std::string& page = "") {
    // 标题 40px，表头相应加高；右上角信息与页码合成一行，避免两行叠在一起。
    drawText(box.x + 24, box.y + 12, title, 22, C_TEXT);
    std::string info = right;
    if (!page.empty()) info += (info.empty() ? "" : "    ") + page;
    if (!info.empty())
        drawText(box.right() - 24, box.y + 18, info, 13, C_TEXT_DIM, 2);
    drawLine(box.x + 20, box.y + 52, box.right() - 20, box.y + 52, C_BORDER);
}

void drawLogBody(Rect body, size_t start, int size, int lineH, int maxLines) {
    std::vector<std::wstring> lines;
    const std::string eventMarker = "【事件】";
    for (size_t i = start; i < g_logs.size(); ++i) {
        if (g_logs[i].rfind(eventMarker, 0) == 0) continue;  // 标题已单独显示
        auto part = wrapW(utf8ToW(g_logs[i]), body.w - 8, size);
        for (auto& p : part) lines.push_back(p);
    }
    if ((int)lines.size() > maxLines)
        lines.erase(lines.begin(), lines.begin() + ((int)lines.size() - maxLines));
    for (int i = 0; i < (int)lines.size(); ++i)
        drawTextW(body.x + 4, body.y + i * lineH, lines[i], size, C_TEXT, 0);
}

// ==================== 通用选卡器 ====================
int pickCardModal(const std::string& title, const std::vector<Card>& cards,
                  bool allowCancel, const std::string& footer, bool big) {
    if (cards.empty()) return -1;
    // 大卡 260x362（弹窗 960x560）；小卡 130x180，每行 6 张、每页 12 张
    // （3 行会超出弹窗底部按钮区）。
    int perPage = big ? 3 : 12;
    int pages = ((int)cards.size() + perPage - 1) / perPage;
    int page = 0;
    Rect box = big ? Rect{ 160, 80, 960, 560 } : Rect{ 90, 80, 1100, 580 };

    HX::ButtonProfile prevBtn, nextBtn, cancelBtn;
    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        if (g_activeBattle) drawBattle(*g_activeBattle, {});
        else drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 175);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        bool prevHit = false, nextHit = false, cancelHit = false;
        if (page > 0)
            prevHit = hxButton(40, box.h - 58, 120, 40, "上一页", prevBtn);
        if (page + 1 < pages)
            nextHit = hxButton(176, box.h - 58, 120, 40, "下一页", nextBtn);
        if (allowCancel)
            cancelHit = hxButton(box.w - 164, box.h - 58, 124, 40,
                                 big ? "跳过" : "取消", cancelBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        drawModalHeader(box, title, footer,
                        std::to_string(page + 1) + "/" + std::to_string(pages));

        int nPage = std::min(perPage, (int)cards.size() - page * perPage);
        std::vector<Rect> cardRects;
        if (big) {
            int w = 260, h = 362, gap = 30;
            int total = nPage * w + (nPage - 1) * gap;
            int x0 = box.x + (box.w - total) / 2;
            int y0 = box.y + 88;
            for (int k = 0; k < nPage; ++k) {
                Rect r{ x0 + k * (w + gap), y0, w, h };
                cardRects.push_back(r);
                bool hov = r.contains(g_mouseX, g_mouseY);
                drawCardFace(r, cards[page * perPage + k], hov, false, 22);
            }
        } else {
            int w = 130, h = 180, gap = 12, perRow = 6;
            int rows = (nPage + perRow - 1) / perRow;
            int rowCount = std::min(perRow, nPage);
            int total = rowCount * w + (rowCount - 1) * gap;
            int x0 = box.x + (box.w - total) / 2;
            int y0 = box.y + 70;
            for (int k = 0; k < nPage; ++k) {
                int row = k / perRow, col = k % perRow;
                Rect r{ x0 + col * (w + gap), y0 + row * (h + gap), w, h };
                cardRects.push_back(r);
                bool hov = r.contains(g_mouseX, g_mouseY);
                drawCardThumb(r, cards[page * perPage + k]);
                if (hov) outlinePanel(r, C_GOLD, 2);
            }
            (void)rows;
        }

        FlushBatchDraw();
        waitFrame(frameStart);

        if (in.esc || in.rightClick) {
            if (allowCancel) return -1;
        }
        if (cancelHit) return -1;
        if (prevHit) page = std::max(0, page - 1);
        if (nextHit) page = std::min(pages - 1, page + 1);
        if (in.click) {
            for (int k = 0; k < (int)cardRects.size(); ++k)
                if (cardRects[k].contains(in.mx, in.my))
                    return page * perPage + k;
        }
    }
    return -1;
}

// ==================== 只读卡牌列表（牌组 / 牌堆查看） ====================
// 展示顺序由调用方决定：牌组按原顺序，抽牌堆传入已排序的显示副本；
// 本函数只渲染与翻页，不修改任何牌堆，也不调用随机数。
void cardListModal(const std::string& title, const std::vector<Card>& cards) {
    int perPage = 12;   // 网格卡 130x180、6 列 2 行（3 行会压到底部按钮）
    int pages = std::max(1, ((int)cards.size() + perPage - 1) / perPage);
    int page = 0;
    Rect box{ 90, 80, 1100, 580 };
    HX::ButtonProfile prevBtn, nextBtn, closeBtn;

    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        if (g_activeBattle) drawBattle(*g_activeBattle, {});
        else drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 175);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        bool prevHit = false, nextHit = false, closeHit = false;
        if (page > 0)
            prevHit = hxButton(40, box.h - 58, 120, 40, "上一页", prevBtn);
        if (page + 1 < pages)
            nextHit = hxButton(176, box.h - 58, 120, 40, "下一页", nextBtn);
        closeHit = hxButton(box.w - 164, box.h - 58, 124, 40, "关闭", closeBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        drawModalHeader(box, title, std::to_string(cards.size()) + " 张",
                        std::to_string(page + 1) + "/" + std::to_string(pages));

        int nPage = std::min(perPage, (int)cards.size() - page * perPage);
        if (nPage <= 0) {
            drawText(box.x + box.w / 2, box.y + 220, "牌堆为空", 20, C_TEXT_DIM, 1);
        } else {
            int w = 130, h = 180, gap = 12, perRow = 6;
            int rowCount = std::min(perRow, nPage);
            int total = rowCount * w + (rowCount - 1) * gap;
            int x0 = box.x + (box.w - total) / 2;
            int y0 = box.y + 70;
            for (int k = 0; k < nPage; ++k) {
                int row = k / perRow, col = k % perRow;
                Rect r{ x0 + col * (w + gap), y0 + row * (h + gap), w, h };
                drawCardThumb(r, cards[page * perPage + k]);
                if (r.contains(g_mouseX, g_mouseY)) outlinePanel(r, C_GOLD, 2);
            }
        }

        FlushBatchDraw();
        waitFrame(frameStart);

        if (in.esc || in.rightClick || closeHit) return;
        if (prevHit) page = std::max(0, page - 1);
        if (nextHit) page = std::min(pages - 1, page + 1);
        if (in.wheelUp) page = std::max(0, page - 1);
        if (in.wheelDown) page = std::min(pages - 1, page + 1);
    }
}

void deckModal(const std::vector<Card>& deck) {
    cardListModal("牌组", deck);
}

// ==================== 卡牌大全 ====================
// 效果文本只用于界面展示：底稿为 data.hpp 末尾「全卡池清单」，
// 并按当前实现修正了个别数值（绯红披风 10 格挡、群情激愤单机版只复制到自己的弃牌堆）。
// 逻辑层不引入描述字段，新增 / 修改卡牌时需要同步维护这里。
std::string cardDescription(const std::string& name) {
    static const std::map<std::string, std::string> kDesc = {
        // —— 基础牌 ——
        { "打击", "造成6点伤害。" },
        { "防御", "获得5点格挡。" },
        { "痛击", "造成8点伤害，给予2层易伤。" },
        // —— 攻击 ——
        { "铁斩波", "获得5点格挡，造成5点伤害。" },
        { "剑柄打击", "造成9点伤害，抽1张牌。" },
        { "闪电霹雳", "对所有敌人造成4点伤害，给予1层易伤。" },
        { "全身撞击", "造成等同于你当前格挡值的伤害。" },
        { "御血术", "失去2点生命，造成15点伤害。" },
        { "头槌", "造成9点伤害，将你弃牌堆中的一张牌放到抽牌堆顶部。" },
        { "双重打击", "造成5点伤害两次。" },
        { "飞剑回旋镖", "随机对敌人造成3点伤害3次。" },
        { "突破", "失去1点生命，对所有敌人造成9点伤害。" },
        { "欺凌", "造成4点伤害，该敌人身上每有一层易伤就额外造成2点伤害。" },
        { "余烬", "造成18点伤害，随机消耗1张牌。" },
        { "重锤", "造成32点伤害。" },
        { "破击", "造成20点伤害，给予5层易伤。" },
        { "焚烧", "对所有敌人造成2点伤害4次。" },
        { "完美打击", "造成6点伤害，你每有一张名字中含有\"打击\"的牌，伤害+2。" },
        { "灰烬打击", "造成6点伤害，你的消耗牌堆中每有一张牌，伤害增加3。" },
        { "拆卸", "造成8点伤害，如果该敌人有易伤状态，则攻击两次。" },
        { "预备打击", "造成7点伤害，在本回合内获得2点力量。" },
        { "旋风斩", "对所有敌人造成6点伤害X次。" },
        { "踩踏", "对所有敌人造成12点伤害，你在本回合中每打出过一张攻击牌，其耗能减少1。" },
        { "交锋", "只有在手牌中每一张牌都是攻击牌时才能被打出，造成14点伤害。" },
        { "契约终结", "如果你的消耗牌堆拥有大于等于3张牌，则对所有敌人造成17点伤害。" },
        { "狂宴", "造成10点伤害，斩杀时永久获得3点最大生命值，消耗。" },
        { "怨恨", "造成5点伤害，如果你在本回合失去过生命值，则攻击2次。" },
        { "熔融之拳", "造成10点伤害，将该敌人身上的易伤层数翻倍，消耗。" },
        { "愤怒", "造成6点伤害，将一张此牌的复制品加入弃牌堆。" },
        { "群情激愤", "造成9点伤害，在弃牌堆中加入一张此牌的复制品。" },
        // —— 技能 ——
        { "邪眼", "获得8点格挡，如果你在本回合消耗过卡牌，则额外获得8点格挡。" },
        { "耸肩无视", "获得8点格挡，抽1张牌。" },
        { "放血", "失去3点生命，获得2点能量。" },
        { "坚毅", "获得7点格挡，随机消耗1张牌。" },
        { "挑衅", "获得6点格挡，给予1层易伤。" },
        { "战栗", "给予3层易伤，消耗。" },
        { "燃烧契约", "消耗1张牌，抽2张牌。" },
        { "血墙", "失去2点生命，获得16点格挡。" },
        { "祭品", "失去6点生命，获得2点能量，抽3张牌，消耗。" },
        { "烙印", "失去1点生命，消耗1张牌，获得1点力量。" },
        { "绯红披风", "失去1点生命，获得10点格挡。" },
        { "巩固", "将你当前的格挡翻倍。" },
        { "重振精神", "消耗手牌中所有非攻击牌，每张获得5点格挡。" },
        { "倾泻", "打出你抽牌堆顶部的X张牌。" },
        { "破灭", "打出你抽牌堆顶部的牌，并将其消耗。" },
        { "巨像", "获得4点格挡，在本回合中，有易伤状态的敌人对你造成的伤害降低50%。" },
        { "战斗专注", "抽3张牌，你在本回合内不能再抽任何牌。" },
        { "战鼓", "抽2张牌，这张牌被消耗时，获得1点能量。" },
        { "被遗忘的仪式", "如果你在本回合消耗过卡牌，则获得2点能量，消耗。" },
        { "狂怒", "打出此牌后，你在这个回合内每打出一张攻击牌，获得3点格挡。" },
        { "火焰屏障", "获得12点格挡，你在这个回合每受到一次攻击，都会对攻击者造成4点伤害。" },
        { "添柴", "消耗所有手牌，每消耗一张牌，将1张随机牌加入你的手牌。" },
        { "主宰", "给予1层易伤，敌人身上每有一层易伤，就获得1点力量，消耗。" },
        { "双持", "选择一张攻击牌或能力牌，将该牌的一张复制品加入你的手牌。" },
        { "连环拳", "在这个回合，你打出的下1张攻击牌会被额外打出一次。" },
        { "岩石铠甲", "获得6层覆甲。" },
        // —— 能力 ——
        { "惊逃", "在你的回合结束时，随机打出你手牌中的1张攻击牌攻击随机敌人。" },
        { "燃烧", "获得2点力量。" },
        { "残酷", "有易伤状态的敌人额外受到25%的伤害。" },
        { "无惧疼痛", "每当有一张牌被消耗时，获得3点格挡。" },
        { "黑暗之拥", "每当有一张牌被消耗时，抽1张牌。" },
        { "坚定不移", "翻倍你每回合第一次从卡牌中获得的格挡。" },
        { "势不可当", "每当你获得格挡时，对随机敌人造成6点伤害。" },
        { "杂耍", "将你在每回合打出的第三张攻击牌的复制品加入你的手牌。" },
        { "壁垒", "格挡不再在你的回合开始时消失。" },
        { "腐化", "技能牌费用变为0，每当你打出一张技能牌时，将其消耗。" },
        { "狱火", "在你的回合开始时，失去1点生命，每当你在你的回合内失去生命时，对所有敌人造成6点伤害。" },
        { "地狱狂徒", "每当你抽到名字中有\"打击\"的牌时，对一名随机敌人打出这张牌。" },
        { "薪火之源", "在回合开始时，获得1点能量。" },
        { "恶魔形态", "在你的回合开始时，获得2点力量。" },
        { "金属化", "在你的回合结束时，获得3点格挡。" },
    };
    auto it = kDesc.find(name);
    return it == kDesc.end() ? "暂无描述。" : it->second;
}

// 卡牌大全清单：基础牌固定在前，其余按实际 Card::type 分组
// （allCardFactories 里邪眼 / 岩石铠甲登记在错误分组，这里以卡牌自身类型为准）。
std::vector<Card> compendiumCards() {
    std::vector<Card> attacks, skills, powers;
    for (auto& factory : allCardFactories()) {
        Card c = factory();
        switch (c.type) {
        case CardType::Attack: attacks.push_back(std::move(c)); break;
        case CardType::Skill:  skills.push_back(std::move(c)); break;
        case CardType::Power:  powers.push_back(std::move(c)); break;
        }
    }
    std::vector<Card> out;
    out.reserve(3 + attacks.size() + skills.size() + powers.size());
    out.push_back(makeStrike());
    out.push_back(makeDefend());
    out.push_back(makeBash());
    for (auto* group : { &attacks, &skills, &powers })
        for (auto& c : *group)
            out.push_back(std::move(c));
    return out;
}

// 卡牌大全弹窗：左侧网格列出全部卡牌，右侧详情面板显示选中卡的大图与效果。
// 只读卡牌数据，不修改战斗状态、不消耗任何随机流。
void cardCompendiumModal() {
    std::vector<Card> cards = compendiumCards();
    const int perPage = 12;   // 130x180 缩略卡，6 列 x 2 行
    const int pages = std::max(1, ((int)cards.size() + perPage - 1) / perPage);
    int page = 0;
    int selected = cards.empty() ? -1 : 0;
    const Rect box{ 50, 60, 1180, 610 };
    const Rect detail{ 930, 130, 276, 455 };
    HX::ButtonProfile prevBtn, nextBtn, closeBtn;

    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        if (g_activeBattle) drawBattle(*g_activeBattle, {});
        else drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 175);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        bool prevHit = false, nextHit = false, closeHit = false;
        if (page > 0)
            prevHit = hxButton(40, box.h - 58, 120, 40, "上一页", prevBtn);
        if (page + 1 < pages)
            nextHit = hxButton(176, box.h - 58, 120, 40, "下一页", nextBtn);
        closeHit = hxButton(box.w - 164, box.h - 58, 124, 40, "关闭", closeBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        drawModalHeader(box, "卡牌大全", "共 " + std::to_string(cards.size()) + " 张",
                        std::to_string(page + 1) + "/" + std::to_string(pages));

        const int w = 130, h = 180, gap = 12, perRow = 6;
        const int x0 = box.x + 24;
        const int y0 = box.y + 70;
        int nPage = std::min(perPage, (int)cards.size() - page * perPage);
        std::vector<Rect> cardRects;
        for (int k = 0; k < nPage; ++k) {
            int row = k / perRow, col = k % perRow;
            Rect r{ x0 + col * (w + gap), y0 + row * (h + gap), w, h };
            cardRects.push_back(r);
            int idx = page * perPage + k;
            drawCardThumb(r, cards[idx]);
            if (r.contains(g_mouseX, g_mouseY)) outlinePanel(r, C_GOLD, 2);
            if (idx == selected) outlinePanel(r, C_GOLD, 3);
        }
        drawText(x0, y0 + 2 * (h + gap) + 2, "点击卡牌查看效果，滚轮翻页",
                 12, C_DISABLED);

        panel(detail, C_PANEL_DARK, C_BORDER);
        if (selected >= 0 && selected < (int)cards.size()) {
            const Card& c = cards[selected];
            Rect face{ detail.x + (detail.w - 220) / 2, detail.y + 12, 220, 306 };
            drawCardFace(face, c, true, false, 18);
            drawTextWrapped(detail.x + 14, face.bottom() + 8,
                            cardDescription(c.name), detail.w - 28, 13, C_TEXT,
                            fontLineH(13), 7);
        } else {
            drawText(detail.x + detail.w / 2, detail.y + 180,
                     "点击左侧卡牌查看效果", 13, C_TEXT_DIM, 1);
        }

        FlushBatchDraw();
        waitFrame(frameStart);

        if (in.esc || in.rightClick || closeHit) return;
        if (prevHit) page = std::max(0, page - 1);
        if (nextHit) page = std::min(pages - 1, page + 1);
        if (in.wheelUp) page = std::max(0, page - 1);
        if (in.wheelDown) page = std::min(pages - 1, page + 1);
        if (in.click) {
            for (int k = 0; k < (int)cardRects.size(); ++k) {
                if (cardRects[k].contains(in.mx, in.my)) {
                    selected = page * perPage + k;
                    break;
                }
            }
        }
    }
}

// ==================== chooseCard 模态 ====================
int guiChooseCard(const ChoiceRequest& req) {
    if (!req.source || req.candidates.empty()) return -1;
    // 候选卡 130x195；1040 宽放不下 8 张，每页 7 张。
    const int perPage = 7;
    int pages = ((int)req.candidates.size() + perPage - 1) / perPage;
    int page = 0;
    Rect box{ 120, 150, 1040, 420 };
    HX::ButtonProfile prevBtn, nextBtn, cancelBtn;

    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        if (g_activeBattle) drawBattle(*g_activeBattle, {});
        else drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 175);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        bool prevHit = false, nextHit = false, cancelHit = false;
        if (page > 0)
            prevHit = hxButton(40, box.h - 58, 120, 40, "上一页", prevBtn);
        if (page + 1 < pages)
            nextHit = hxButton(176, box.h - 58, 120, 40, "下一页", nextBtn);
        cancelHit = hxButton(box.w - 164, box.h - 58, 124, 40, "取消", cancelBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        std::string label = req.label ? req.label : "选择一张牌";
        drawModalHeader(box, "选择一张牌", label,
                        std::to_string(page + 1) + "/" + std::to_string(pages));

        int nPage = std::min(perPage, (int)req.candidates.size() - page * perPage);
        int w = 130, h = 195, gap = 12;
        int total = nPage * w + (nPage - 1) * gap;
        int x0 = box.x + (box.w - total) / 2;
        int y0 = box.y + 82;
        std::vector<Rect> cardRects;
        for (int k = 0; k < nPage; ++k) {
            Rect r{ x0 + k * (w + gap), y0, w, h };
            cardRects.push_back(r);
            const Card& c = req.source->at(req.candidates[page * perPage + k]);
            bool hov = r.contains(g_mouseX, g_mouseY);
            drawCardFace(r, c, hov, false, 16);
        }

        FlushBatchDraw();
        waitFrame(frameStart);

        if (in.esc || in.rightClick || cancelHit) return -1;
        if (prevHit) page = std::max(0, page - 1);
        if (nextHit) page = std::min(pages - 1, page + 1);
        if (in.click) {
            for (int k = 0; k < (int)cardRects.size(); ++k)
                if (cardRects[k].contains(in.mx, in.my))
                    return req.candidates[page * perPage + k];
        }
    }
    return -1;
}

// ==================== 奖励 / 事件回调 ====================
int guiChooseReward(const std::vector<Card>& options) {
    std::string footer = "金币 " + std::to_string(g_rs.gold);
    int delta = g_rs.gold - g_goldBeforeNode;
    if (delta != 0) footer += "（本次 " + std::string(delta > 0 ? "+" : "") + std::to_string(delta) + "）";
    return pickCardModal("战利品", options, true, footer, true);
}

int guiChooseDeck(const std::vector<Card>& deck) {
    return pickCardModal("选择一张牌", deck, true, "", false);
}

int eventModal(const std::vector<std::string>& options) {
    if (options.empty()) return -1;
    // 事件有多个选项时必须择一：额外提供「取消 / 离开」等于允许跳过事件。
    const bool allowCancel = options.size() <= 1;
    Rect box{ 160, 80, 960, 560 };
    std::vector<HX::ButtonProfile> profiles(options.size());
    HX::ButtonProfile cancelBtn;

    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 175);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        int chosen = -1;
        for (int i = 0; i < (int)options.size(); ++i) {
            if (hxButton(80, 330 + i * 56, 800, 46, options[i], profiles[i]))
                chosen = i;
        }
        bool cancelHit = false;
        if (allowCancel)
            cancelHit = hxButton(80, 330 + (int)options.size() * 56, 800, 42,
                                 "取消 / 离开", cancelBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        std::string title = "事件";
        const std::string marker = "【事件】";
        for (size_t i = g_nodeLogStart; i < g_logs.size(); ++i) {
            if (g_logs[i].rfind(marker, 0) == 0) {
                title = g_logs[i].substr(marker.size());
                break;
            }
        }
        drawModalHeader(box, title);
        drawLogBody(Rect{ box.x + 24, box.y + 60, box.w - 48, 250 },
                    g_nodeLogStart, 15, fontLineH(15), 8);

        FlushBatchDraw();
        waitFrame(frameStart);

        if (allowCancel && (in.esc || in.rightClick || cancelHit)) return -1;
        if (chosen >= 0) return chosen;
    }
    return -1;
}

int guiChooseEvent(const std::vector<std::string>& options) {
    return eventModal(options);
}

void eventResultModal() {
    Rect box{ 220, 120, 840, 480 };
    HX::ButtonProfile closeBtn;
    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 175);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        bool closeHit = hxButton(box.w / 2 - 80, box.h - 66, 160, 44, "继续", closeBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        drawModalHeader(box, "事件结果");
        drawLogBody(Rect{ box.x + 24, box.y + 60, box.w - 48, 330 },
                    g_nodeLogStart, 15, fontLineH(15), 14);

        FlushBatchDraw();
        waitFrame(frameStart);

        if (in.esc || in.rightClick || closeHit) return;
    }
}

bool confirmModal(const std::string& title, const std::string& text) {
    Rect box{ 340, 240, 600, 240 };
    HX::ButtonProfile yesBtn, noBtn;
    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 180);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        bool yes = hxButton(90, 170, 180, 44, "是", yesBtn);
        bool no = hxButton(330, 170, 180, 44, "否", noBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        drawText(box.x + 30, box.y + 24, title, 22, C_TEXT);
        drawTextWrapped(box.x + 30, box.y + 72, text, box.w - 60, 15, C_TEXT_DIM,
                        fontLineH(15), 3);

        FlushBatchDraw();
        waitFrame(frameStart);

        if (yes) return true;
        if (no || in.esc || in.rightClick) return false;
    }
    return false;
}

// ==================== 战斗阶段 / 回合驱动 ====================
// 计算飞行动画的起点 / 终点与尺寸；只在确认能打出后调用，不消耗随机数。
CardFlight makeCardFlight(BattleContext& ctx, int handIdx, int targetIdx) {
    CardFlight f;
    f.card = ctx.hand[handIdx];                 // 副本：动画期间不动 ctx.hand
    f.handIdx = handIdx;
    f.from = handCardRect(handIdx, (int)ctx.hand.size());
    // 直接点击手牌打出时，手牌是抬起 20px 绘制的；飞牌从抬起后的实际位置起步，避免瞬移。
    if (handIndexAt(ctx, g_mouseX, g_mouseY) == handIdx) f.from.y -= 20;
    f.cost = displayCostFor(ctx, ctx.hand[handIdx]);
    f.nameSize = (int)ctx.hand.size() > 9 ? 13 : 18;   // 与 drawHand 的字号保持一致
    switch (f.card.target) {
    case TargetMode::SingleEnemy: {
        int n = (int)ctx.enemies.size();
        EnemyLayout L = enemyLayout(targetIdx, n, enemyClassTarget(ctx.enemies[targetIdx].name));
        Rect art = makeEnemyArtRect(ctx.enemies[targetIdx], L);
        f.tx = art.x + art.w / 2;
        f.ty = art.y + art.h / 2;
        break;
    }
    case TargetMode::AllEnemies:
    case TargetMode::RandomEnemy:
        // 群体 / 随机目标统一飞向敌人区域中央，不额外调用随机数。
        f.tx = kEnemyZoneW / 2;
        f.ty = (kFieldTop + kFieldBottom) / 2;
        break;
    case TargetMode::Self:
        f.tx = kPlayerPortrait.x + kPlayerPortrait.w / 2;
        f.ty = kPlayerPortrait.y + kPlayerPortrait.h / 2;
        break;
    }
    f.toW = std::max(1, (int)std::lround(f.from.w * 0.65f));
    f.toH = std::max(1, (int)std::lround(f.from.h * 0.65f));
    f.durationMs = 200;
    return f;
}

// 出牌动画：卡牌飞出 +（攻击牌）玩家突进同时开始，循环到两者都结束。
// 期间只重绘，不修改手牌 / 能量 / 任何游戏数据；结束后才由调用方 playCard。
void playCardAnimation(BattleContext& ctx, const CardFlight& flight, bool withLunge) {
    auto start = std::chrono::steady_clock::now();
    g_cardFlight = flight;
    g_cardFlight->start = start;
    if (withLunge) g_playerLunge = LungeAnim{ -1, start };
    const int totalMs = withLunge ? std::max(flight.durationMs, kLungeMs)
                                  : flight.durationMs;
    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        pollInput(false);   // 丢弃动画期间的输入，避免连点穿透到下一张牌
        if (elapsedMs(start) >= totalMs) break;
        updateFloats();
        drawBattle(ctx, {});
        FlushBatchDraw();
        waitFrame(frameStart);
    }
    g_cardFlight.reset();
    g_playerLunge.reset();
}

// 敌人攻击突进：由 onEnemyActionResolved 回调同步驱动，多个敌人依次播放；
// 到位瞬间触发玩家受击反馈（复用受击动画），不改动任何战斗数据。
void playEnemyAttackLunge(BattleContext& ctx, int idx) {
    g_enemyLunge = LungeAnim{ idx, std::chrono::steady_clock::now() };
    bool playerHitStarted = false;
    while (!g_quit && g_enemyLunge) {
        auto frameStart = std::chrono::steady_clock::now();
        pollInput(false);
        int ms = elapsedMs(g_enemyLunge->start);
        if (ms >= kLungeMs) break;
        if (!playerHitStarted && ms >= kLungeForwardMs) {
            triggerPlayerHit();
            playerHitStarted = true;
        }
        updateFloats();
        drawBattle(ctx, {});
        FlushBatchDraw();
        waitFrame(frameStart);
    }
    g_enemyLunge.reset();
}

bool playFromGUI(BattleContext& ctx, int handIdx, int targetIdx) {
    // 先确认能打出：只有 GUI 校验通过才播放飞牌动画；失败路径保持原样（仍由 playCard 记日志）。
    const Card* card = (handIdx >= 0 && handIdx < (int)ctx.hand.size())
                           ? &ctx.hand[handIdx] : nullptr;
    const bool targetOk = !card || card->target != TargetMode::SingleEnemy ||
                          (targetIdx >= 0 && targetIdx < (int)ctx.enemies.size() &&
                           ctx.enemies[targetIdx].inPlay());
    if (card && targetOk && guiPlayable(ctx, *card))
        playCardAnimation(ctx, makeCardFlight(ctx, handIdx, targetIdx),
                          card->type == CardType::Attack);   // 纯视觉先行，归位后才结算

    auto pe = snapEnemies(ctx);
    auto pp = snapPlayer(ctx);
    bool ok = playCard(ctx, handIdx, targetIdx);
    if (ok) {
        diffEnemies(ctx, pe);
        diffPlayer(ctx, pp);
    }
    return ok;
}

// 返回 true 表示玩家主动结束回合；false 表示战斗结束或退出。
bool playerPhaseGUI(BattleContext& ctx) {
    int aimHand = -1;
    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        InputFrame in = pollInput(false);
        updateFloats();

        if (in.esc || in.rightClick) aimHand = -1;

        if (in.click) {
            // 牌堆查看：只读弹窗，打开前取消瞄准；关闭后仍停留在当前玩家回合。
            if (kDrawPileButton.contains(in.mx, in.my)) {
                aimHand = -1;
                std::vector<Card> view = drawPileDisplay(ctx);
                cardListModal("抽牌堆", view);
                continue;
            }
            if (kDiscardPileButton.contains(in.mx, in.my)) {
                aimHand = -1;
                cardListModal("弃牌堆", ctx.discardPile);
                continue;
            }
            if (kExhaustPileButton.contains(in.mx, in.my)) {
                aimHand = -1;
                cardListModal("消耗堆", ctx.exhaustPile);
                continue;
            }
            if (kDeckButton.contains(in.mx, in.my)) {
                aimHand = -1;
                deckModal(ctx.player.deck);
                continue;
            }
            if (kCardLibButton.contains(in.mx, in.my)) {
                aimHand = -1;
                cardCompendiumModal();
                continue;
            }
            if (kEndTurnButton.contains(in.mx, in.my)) return true;

            if (aimHand >= 0) {
                int ei = enemyIndexAt(ctx, in.mx, in.my);
                if (ei >= 0 && aimHand < (int)ctx.hand.size())
                    playFromGUI(ctx, aimHand, ei);
                aimHand = -1;
            } else {
                int hi = handIndexAt(ctx, in.mx, in.my);
                if (hi >= 0) {
                    Card& c = ctx.hand[hi];
                    int alive = aliveEnemyCount(ctx);
                    if (!guiPlayable(ctx, c)) {
                        int t = firstAliveEnemy(ctx);
                        if (t >= 0 || c.target != TargetMode::SingleEnemy)
                            playFromGUI(ctx, hi, t < 0 ? 0 : t);
                    } else if (c.target == TargetMode::SingleEnemy && alive > 1) {
                        aimHand = hi;
                    } else {
                        int t = (c.target == TargetMode::SingleEnemy) ? firstAliveEnemy(ctx) : 0;
                        playFromGUI(ctx, hi, t < 0 ? 0 : t);
                    }
                }
            }
        }

        if (!ctx.player.alive() || !anyEnemyInPlay(ctx)) break;

        BattleView view;
        view.aiming = aimHand >= 0;
        view.aimHand = aimHand;
        view.hoverHand = view.aiming ? -1 : handIndexAt(ctx, in.mx, in.my);
        view.hoverEnemy = enemyIndexAt(ctx, in.mx, in.my);
        drawBattle(ctx, view);
        FlushBatchDraw();
        waitFrame(frameStart);
    }
    return false;
}

bool guiBattle(BattleContext& ctx) {
    g_activeBattle = &ctx;
    g_battleTurn = 0;
    g_floats.clear();
    clearBattleAnims();
    // 每场战斗换一张背景，相邻两场必不相同（不消耗任何逻辑随机域）。
    g_battleBgName = pickBattleBgName();

    for (auto& e : ctx.enemies) planNextIntent(ctx, e);
    ctx.chooseCard = [](const ChoiceRequest& req) { return guiChooseCard(req); };
    // 敌人行动完成回调：只做表现层突进，CLI 不设置时 enemyTurn 行为与原来一致。
    ctx.onEnemyActionResolved = [](int idx, bool hurtPlayer) {
        if (hurtPlayer && g_activeBattle)
            playEnemyAttackLunge(*g_activeBattle, idx);
    };

    while (ctx.player.alive() && anyEnemyInPlay(ctx) && !g_quit) {
        ++g_battleTurn;
        startPlayerTurn(ctx);
        updateFloats();
        if (!ctx.player.alive() || !anyEnemyInPlay(ctx)) break;

        bool wantEnd = playerPhaseGUI(ctx);
        if (g_quit || !wantEnd) break;
        if (!ctx.player.alive() || !anyEnemyInPlay(ctx)) break;

        auto pp = snapPlayer(ctx);
        auto pe = snapEnemies(ctx);
        endPlayerTurn(ctx);
        diffPlayer(ctx, pp);
        diffEnemies(ctx, pe);
        if (!ctx.player.alive() || !anyEnemyInPlay(ctx)) break;

        enemyTurn(ctx);
        diffPlayer(ctx, pp);
        diffEnemies(ctx, pe, false);   // 敌人行动前自清的格挡不算受击
        updateFloats();

        // 敌人回合结束后短暂展示伤害数字与受击动画：逐帧重绘（仍由 waitFrame 控帧），
        // 不能用 Sleep 阻塞，否则 160ms 的受击动画只会显示第一帧。
        auto showStart = std::chrono::steady_clock::now();
        while (!g_quit &&
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - showStart).count() < 180) {
            auto frameStart = std::chrono::steady_clock::now();
            updateFloats();
            drawBattle(ctx, {});
            FlushBatchDraw();
            waitFrame(frameStart);
        }
    }

    // 离开战斗必须清掉 GUI 回调：ctx 可能被后续流程复用，也不能影响 CLI 行为。
    ctx.onEnemyActionResolved = nullptr;
    clearBattleAnims();
    if (ctx.player.alive()) pushLog("战斗胜利！");
    g_activeBattle = nullptr;
    return ctx.player.alive();
}

// ==================== 地图 / 菜单 / 结算 ====================
COLORREF nodeColor(NodeType t) {
    switch (t) {
    case NodeType::Battle: return RGB(150, 66, 58);
    case NodeType::Elite:  return RGB(128, 76, 156);
    case NodeType::Event:  return RGB(58, 112, 150);
    case NodeType::Rest:   return RGB(70, 130, 88);
    case NodeType::Boss:   return RGB(190, 76, 48);
    }
    return C_PANEL;
}

const char* nodeLetter(NodeType t) {
    switch (t) {
    case NodeType::Battle: return "B";
    case NodeType::Elite:  return "E";
    case NodeType::Event:  return "?";
    case NodeType::Rest:   return "R";
    case NodeType::Boss:   return "X";
    }
    return "?";
}

std::string nodeDescription(NodeType t) {
    switch (t) {
    case NodeType::Battle: return "普通战斗：胜利后获得金币与三选一卡牌。";
    case NodeType::Elite:  return "精英战斗：敌人更强，金币奖励更多。";
    case NodeType::Event:  return "未知事件：可能获得卡牌、金币，也可能失去生命。";
    case NodeType::Rest:   return "篝火：恢复最大生命的 35%。";
    case NodeType::Boss:   return "首领战：胜利即通关。";
    }
    return "";
}

// 开局：useCode=true 时按短码复现，短码非法返回 false（不静默改随机）。
bool startNewRun(const std::string& code, bool useCode) {
    if (useCode) {
        auto opt = rng::startRunFromCode(code);
        if (!opt) {
            g_seedError = "短码超出种子范围：请使用游戏显示的短码或不超过 10 位";
            return false;
        }
        g_run.emplace(std::move(*opt));
    } else {
        g_run.emplace(rng::startNewRun());
    }
    g_seedError.clear();
    g_rs = beginRun(g_run->rng.map);
    g_logs.clear();
    g_nodeLogStart = 0;
    g_goldBeforeNode = 0;
    g_pendingEnter = false;
    g_battleTurn = 0;
    g_floats.clear();
    clearBattleAnims();
    g_activeBattle = nullptr;
    g_screen = Screen::Map;
    return true;
}

void drawMapScreen() {
    drawBackdrop();
    // v3.5：地图放大到顶栏中线（y=28）以下，顶栏不能再整块不透明盖住纸边。
    // 改成上深下浅的三段遮罩：上半仍压住文字，下半让地图上边缘透出来。
    fillAlphaRect(Rect{ 0, 0, kWidth, 24 }, RGB(22, 19, 27), 228);
    fillAlphaRect(Rect{ 0, 24, kWidth, 16 }, RGB(22, 19, 27), 190);
    fillAlphaRect(Rect{ 0, 40, kWidth, 16 }, RGB(22, 19, 27), 140);
    drawLine(0, 56, kWidth, 56, C_BORDER);
    drawText(16, 6, "种子 " + seedCode(), 12, C_TEXT_DIM);
    drawText(16, 26, "金币 " + std::to_string(g_rs.gold) +
             "    生命 " + std::to_string(g_rs.player.hp) + "/" +
             std::to_string(g_rs.player.maxHp) +
             "    牌组 " + std::to_string(g_rs.player.deck.size()), 13, C_TEXT);
    drawText(1264, 14, "冒险路线", 15, C_TEXT_DIM, 2);
    Rect deckBtn{ 1010, 10, 110, 36 };
    bool deckHover = deckBtn.contains(g_mouseX, g_mouseY);
    drawEasyButton(deckBtn, "牌组", deckHover, true);
    bool libHover = kMapCardLibButton.contains(g_mouseX, g_mouseY);
    drawEasyButton(kMapCardLibButton, "卡牌大全", libHover, true);

    int n = g_rs.map.size();
    if (n <= 0) return;
    // v3.4：羊皮纸改成 contain 后只剩 972 设计像素宽（x 154..1126），
    // 节点行与下方两个面板同步收窄，保证全部落在纸面内。
    int nodeW = 52, gap = 9;
    int total = n * nodeW + (n - 1) * gap;
    int x0 = (kWidth - total) / 2;
    int y0 = 276;
    for (int i = 0; i < n - 1; ++i) {
        int cx1 = x0 + i * (nodeW + gap) + nodeW / 2;
        int cx2 = x0 + (i + 1) * (nodeW + gap) + nodeW / 2;
        drawLine(cx1, y0 + nodeW / 2, cx2, y0 + nodeW / 2, C_BORDER);
    }
    for (int i = 0; i < n; ++i) {
        NodeType t = g_rs.map.at(i);
        Rect r{ x0 + i * (nodeW + gap), y0, nodeW, nodeW };
        bool current = (i == g_rs.pos);
        bool past = (i < g_rs.pos);
        COLORREF fill = nodeColor(t);
        if (past) fill = mixColor(fill, C_BG, 150);
        panel(r, fill, current ? C_GOLD : C_BORDER, current ? 4 : 1);
        drawText(r.x + r.w / 2, r.y + 12, nodeLetter(t), 26,
                 past ? C_DISABLED : C_TEXT, 1);
        // v3.3：地图背景换成羊皮纸（亮色）后，节点名原来直接压在底图上几乎看不清，
        // 这里给文字加一块半透明深色底片；底片宽度按实际文字宽度计算。
        const std::string label = nodeTypeName(t);
        const int labelW = measureW(utf8ToW(label), 13);
        fillAlphaRect(Rect{ r.x + r.w / 2 - labelW / 2 - 6, r.bottom() + 2,
                            labelW + 12, 20 },
                      RGB(12, 10, 16), 170);
        drawText(r.x + r.w / 2, r.bottom() + 5, label, 13,
                 current ? C_GOLD : C_TEXT_DIM, 1);
    }

    Rect info{ 187, 392, 906, 104 };
    panel(info, RGB(32, 27, 39), C_BORDER);
    if (g_rs.pos < n) {
        NodeType t = g_rs.map.at(g_rs.pos);
        drawText(info.x + 20, info.y + 14, "第 " + std::to_string(g_rs.pos + 1) +
                 " 站 / " + nodeTypeName(t), 20, C_TEXT);
        drawText(info.x + 20, info.y + 54, nodeDescription(t), 15, C_TEXT_DIM);
    } else {
        drawText(info.x + 20, info.y + 30, "路线已完成", 20, C_TEXT_DIM);
    }

    bool enterHover = Rect{ 560, 620, 160, 52 }.contains(g_mouseX, g_mouseY);
    bool canEnter = g_rs.pos < n;
    drawEasyButton(Rect{ 560, 620, 160, 52 }, "进入", enterHover, canEnter, true);
    bool menuHover = Rect{ 187, 620, 150, 44 }.contains(g_mouseX, g_mouseY);
    drawEasyButton(Rect{ 187, 620, 150, 44 }, "主菜单", menuHover, true);
}

void mapFrame() {
    auto frameStart = std::chrono::steady_clock::now();
    InputFrame in = pollInput(false);
    drawMapScreen();
    FlushBatchDraw();
    waitFrame(frameStart);

    if (g_quit) return;
    if (in.esc) {
        if (confirmModal("返回主菜单", "当前冒险进度将丢失，确定返回主菜单吗？")) {
            g_run.reset();
            g_rs = RunState{};
            g_screen = Screen::Menu;
        }
        return;
    }
    if (in.click) {
        if (Rect{ 1010, 10, 110, 36 }.contains(in.mx, in.my)) {
            deckModal(g_rs.player.deck);
            return;
        }
        if (kMapCardLibButton.contains(in.mx, in.my)) {
            cardCompendiumModal();
            return;
        }
        if (Rect{ 187, 620, 150, 44 }.contains(in.mx, in.my)) {
            if (confirmModal("返回主菜单", "当前冒险进度将丢失，确定返回主菜单吗？")) {
                g_run.reset();
                g_rs = RunState{};
                g_screen = Screen::Menu;
            }
            return;
        }
        if (g_rs.pos < g_rs.map.size() &&
            Rect{ 560, 620, 160, 52 }.contains(in.mx, in.my)) {
            g_pendingEnter = true;
        }
    }
}

// 存档：目前没有存档系统，只有 exe 同目录存在 save.dat 时才认为「可继续」。
// 后续接入存档时，把这个判断换成真正的存档校验即可。
bool hasSaveGame() {
    const std::string paths[] = { "save.dat", "../save.dat" };
    for (const auto& p : paths) {
        DWORD attr = GetFileAttributesW(utf8ToW(p).c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            return true;
    }
    return false;
}

// 主菜单按钮：半透明底 + 细描边，不铺大色块，保留封面原色。
// 不可用时压成近乎纯黑，明确「点不动」。
void drawMenuButton(Rect r, const std::string& label, bool hover, bool enabled,
                    bool primary) {
    if (enabled) {
        // 透明度压低一些（alpha 越大越不透明），保证压在封面上也读得清。
        fillAlphaRect(r, RGB(12, 10, 16), hover ? 228 : 200);
        outlinePanel(r, hover ? C_GOLD
                              : (primary ? mixColor(C_ATTACK, C_TEXT, 40) : C_BORDER),
                     (hover || primary) ? 2 : 1);
    } else {
        fillAlphaRect(r, RGB(6, 5, 8), 220);
        outlinePanel(r, RGB(38, 34, 44), 1);
    }
    setFont(20);
    int th = (int)std::lround(textheight(L"汉") / g_uiScale);
    drawText(r.x + r.w / 2, r.y + (r.h - th) / 2, label, 20,
             enabled ? C_TEXT : C_DISABLED, 1);
}

// 自定义种子弹窗：返回 true 表示要用 g_seedInput 开局（空串 = 随机）。
bool seedModal() {
    Rect box{ 390, 250, 500, 230 };
    Rect field{ 414, 306, 452, 44 };
    HX::ButtonProfile startBtn, cancelBtn;

    while (!g_quit) {
        auto frameStart = std::chrono::steady_clock::now();
        drawBackdrop();
        fillAlphaRect(Rect{ 0, 0, kWidth, kHeight }, RGB(8, 6, 10), 175);

        HX::HXBegin();
        HX::SetBuffer(GetImageBuffer());
        InputFrame in = pollInput(true);
        hxPanel(box);
        bool startHit = hxButton(40, box.h - 58, 160, 44, "开始新局", startBtn);
        bool cancelHit = hxButton(box.w - 164, box.h - 58, 124, 44, "取消", cancelBtn);
        HX::End();
        HX::Render();

        drawPanelBorder(box);
        drawModalHeader(box, "自定义种子");

        bool edited = false;
        if (in.paste) {
            // Ctrl+V：只提取 base62 字符，自动忽略空格 / 换行 / 中文。
            std::string pasted;
            for (char c : clipboardTextUtf8()) {
                if (pasted.size() >= 11) break;
                if (rng::base62Value(c) < 0) continue;
                pasted.push_back(c);
            }
            if (!pasted.empty()) {
                g_seedInput = pasted;
                edited = true;
            }
        }
        for (char c : in.chars) {
            if (g_seedInput.size() >= 11) break;
            if (rng::base62Value(c) < 0) continue;  // 只接受 0-9 / A-Z / a-z
            g_seedInput.push_back(c);
            edited = true;
        }
        if (in.backspace && !g_seedInput.empty()) {
            g_seedInput.pop_back();
            edited = true;
        }
        if (edited) g_seedError.clear();

        panel(field, C_PANEL_DARK, C_GOLD, 2);
        if (g_seedInput.empty()) {
            drawText(field.x + 12, field.y + 13, "0-9 / A-Z / a-z", 12, C_DISABLED);
        } else {
            drawText(field.x + 12, field.y + 10,
                     ellipsize(g_seedInput, field.w - 24, 15), 15, C_TEXT);
        }
        // 光标闪烁只用 std::chrono，禁止引入随机源。
        static const auto blinkStart = std::chrono::steady_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - blinkStart).count();
        if ((ms / 500) % 2 == 0) {
            int cx = field.x + 12 +
                (g_seedInput.empty() ? 0 : measureW(utf8ToW(g_seedInput), 15));
            cx = std::min(cx, field.right() - 8);
            drawLine(cx, field.y + 11, cx, field.y + 32, C_TEXT);
        }

        if (!g_seedError.empty())
            drawText(box.x + 24, 356, g_seedError, 12, C_WARN);
        else
            drawText(box.x + 24, 356, "留空 = 随机；最多 11 位", 12, C_DISABLED);

        FlushBatchDraw();
        waitFrame(frameStart);

        if (startHit || in.enter) return true;
        if (cancelHit || in.esc || in.rightClick) {
            // 关闭弹窗即清空，下次打开不残留上一次的种子。
            g_seedInput.clear();
            g_seedError.clear();
            return false;
        }
    }
    return false;
}

void menuFrame() {
    auto frameStart = std::chrono::steady_clock::now();
    drawBackdrop();                    // 主菜单不压暗，保留封面原色
    InputFrame in = pollInput(false);  // 主菜单没有 hiEasyX 控件

    // 左下竖排按钮：避开封面标题与右侧高塔，主要落在水面区域。
    const Rect kNew{ 152, 454, 286, 54 };
    const Rect kContinue{ 152, 518, 286, 54 };
    const Rect kSeed{ 152, 582, 286, 54 };
    const Rect kQuit{ 152, 644, 150, 38 };

    const bool canContinue = hasSaveGame();
    struct Item { Rect r; const char* label; bool enabled; bool primary; };
    const Item items[] = {
        { kNew,      "新游戏",     true,        true  },
        { kContinue, "继续",       canContinue, false },
        { kSeed,     "自定义种子", true,        false },
        { kQuit,     "退出",       true,        false },
    };
    bool hover[4] = {};
    for (int i = 0; i < 4; ++i)
        hover[i] = items[i].r.contains(g_mouseX, g_mouseY);

    if (in.click) {
        if (hover[0]) {
            startNewRun("", false);
        } else if (hover[1]) {
            // 存档系统接入后在这里读档；当前 hasSaveGame() 恒为 false，按钮是黑的。
            if (canContinue) { /* TODO: 读取存档并恢复局内进度 */ }
        } else if (hover[2]) {
            // 短码非法时 startNewRun 会写 g_seedError，重新弹窗让玩家继续改。
            while (seedModal()) {
                if (g_seedInput.empty()) {
                    startNewRun("", false);
                    break;
                }
                if (startNewRun(g_seedInput, true)) {
                    g_seedInput.clear();   // 已用于开局，清空避免下次残留
                    break;
                }
            }
        } else if (hover[3]) {
            g_quit = true;
        }
    }
    if (in.esc) g_quit = true;

    for (int i = 0; i < 4; ++i)
        drawMenuButton(items[i].r, items[i].label, hover[i], items[i].enabled,
                       items[i].primary);

    FlushBatchDraw();
    waitFrame(frameStart);
}

void gameOverFrame() {
    auto frameStart = std::chrono::steady_clock::now();
    // 结算输入保护：期间只消费消息、不推给 HX，也不响应 Esc/按钮。
    long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        frameStart - g_gameOverEntered).count();
    bool inputReady = elapsedMs >= kGameOverInputGuardMs;

    drawBackdrop();
    bool win = g_rs.won;
    drawText(640, 110, win ? "胜利" : "失败", 64, win ? C_GOLD : C_WARN, 1);
    drawText(640, 196, win ? "你击败了首领，冒险结束。" : "你的冒险在此结束。",
             20, C_TEXT, 1);

    Rect stats{ 340, 250, 600, 200 };
    panel(stats, C_PANEL, C_BORDER);
    int y = stats.y + 20;
    auto row = [&](const std::string& k, const std::string& v) {
        drawText(stats.x + 30, y, k, 15, C_TEXT_DIM);
        drawText(stats.right() - 30, y, v, 15, C_TEXT, 2);
        y += 30;
    };
    row("种子", seedCode());
    row("生命", std::to_string(std::max(0, g_rs.player.hp)) + "/" +
        std::to_string(g_rs.player.maxHp));
    row("金币", std::to_string(g_rs.gold));
    row("牌组", std::to_string(g_rs.player.deck.size()) + " 张");
    row("到达节点", std::to_string(std::min(g_rs.pos + 1, g_rs.map.size())) +
        "/" + std::to_string(g_rs.map.size()));

    Rect panelR{ 430, 490, 420, 150 };
    HX::HXBegin();
    HX::SetBuffer(GetImageBuffer());
    InputFrame in = pollInput(inputReady);
    hxPanel(panelR);
    static HX::ButtonProfile retryBtn, menuBtn;
    bool retry = hxButton(50, 30, 320, 50, "再来一局", retryBtn);
    bool toMenu = hxButton(50, 92, 320, 44, "返回主菜单", menuBtn);
    HX::End();
    HX::Render();

    drawPanelBorder(panelR);
    FlushBatchDraw();
    waitFrame(frameStart);

    if (!inputReady) return;
    if (retry) startNewRun("", false);
    if (toMenu || in.esc) {
        g_run.reset();
        g_rs = RunState{};
        g_screen = Screen::Menu;
    }
}

}  // namespace

// ==================== DPI 适配 ====================
// 必须在 initgraph 之前调用，否则 Windows 会把窗口当作位图放大，导致文字模糊。
void enableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetCtxFn = BOOL(WINAPI*)(HANDLE);
        auto setCtx = reinterpret_cast<SetCtxFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
        if (setCtx && setCtx((HANDLE)(LONG_PTR)-4)) return;  // PER_MONITOR_AWARE_V2
        using SetAwareFn = BOOL(WINAPI*)();
        auto setAware = reinterpret_cast<SetAwareFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDPIAware")));
        if (setAware && setAware()) return;
        SetProcessDPIAware();
    }
}

// 按当前 DPI 与桌面工作区计算缩放：DPI 决定文字清晰度，工作区决定窗口能否放下。
void setupDpiScale() {
    int dpi = 96;
    HDC screen = GetDC(nullptr);
    if (screen) {
        dpi = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(nullptr, screen);
    }
    if (dpi <= 0) dpi = 96;
    float s = dpi / 96.0f;

    RECT wa{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
        // 预留标题栏与窗口边框，避免算出的客户区贴死后被裁到屏幕外。
        int ww = (wa.right - wa.left) - 16;
        int wh = (wa.bottom - wa.top) - 48;
        if (ww > 0 && wh > 0) {
            float fit = std::min((float)ww / kWidth, (float)wh / kHeight);
            s = std::min(s, fit);
        }
    }
    g_uiScale = std::clamp(s, 0.75f, 3.0f);
    g_winW = scaled(kWidth);
    g_winH = scaled(kHeight);
}

// ==================== 入口 ====================
int main(int argc, char** argv) {
    enableDpiAwareness();
    setupDpiScale();
    loadGameFont();          // 必须在任何 setFont 之前：私有加载 assets/fonts 并探测家族名
    initgraph(g_winW, g_winH);
    SetWindowTextW(GetHWnd(), L"尖塔奇兵");
    setbkmode(TRANSPARENT);
    HX::HXInitForEasyX();
    applyHxTheme();
    BeginBatchDraw();

    g_cb.battle = guiBattle;
    g_cb.chooseRewardCard = guiChooseReward;
    g_cb.chooseDeckCard = guiChooseDeck;
    g_cb.chooseEvent = guiChooseEvent;
    g_cb.log = [](const std::string& s) { pushLog(s); };

    if (argc >= 2) {
        auto opt = rng::startRunFromCode(argv[1]);
        if (opt) {
            g_run.emplace(std::move(*opt));
            g_rs = beginRun(g_run->rng.map);
            g_screen = Screen::Map;
        } else {
            // 非法短码不静默忽略：回填输入框并提示，方便修正后重开。
            if (std::string(argv[1]).size() <= 11) g_seedInput = argv[1];
            g_seedError = "短码无效或超出范围：请使用游戏显示的短码";
        }
    }

    while (!g_quit && IsWindow(GetHWnd())) {
        if (g_screen == Screen::Map && g_pendingEnter) {
            g_pendingEnter = false;
            if (!g_run) {
                g_screen = Screen::Menu;
                continue;
            }
            if (g_rs.pos >= g_rs.map.size()) {
                g_gameOverEntered = std::chrono::steady_clock::now();
                g_screen = Screen::GameOver;
                continue;
            }
            NodeType t = currentType(g_rs);
            g_nodeLogStart = g_logs.size();
            g_goldBeforeNode = g_rs.gold;
            playNode(g_rs, *g_run, g_cb);
            if (g_rs.won || g_rs.dead) {
                g_gameOverEntered = std::chrono::steady_clock::now();
                g_screen = Screen::GameOver;
            } else {
                if (t == NodeType::Event) eventResultModal();
                if (!g_quit && g_screen == Screen::Map) {
                    // 保持在地图；下一帧重新绘制。
                }
            }
            continue;
        }

        switch (g_screen) {
        case Screen::Menu:     menuFrame(); break;
        case Screen::Map:      mapFrame(); break;
        case Screen::GameOver: gameOverFrame(); break;
        }
    }

    releaseAlphaSurfaces();
    EndBatchDraw();
    closegraph();
    releaseGameFont();       // 与 loadGameFont 成对，避免私有字体句柄泄漏
    return 0;
}

