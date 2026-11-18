// 敌人立绘比例测试器（独立工具，不参与游戏构建）
// 画一个空白战斗场景，逐个预览 BINS/assets/enemies 下的立绘，
// 用方向键实时调系数，找到合适数值后按 S 导出表，回填到 gui.cpp 的 enemyArtScale()。
//
// 操作：
//   ← / →   上一个 / 下一个敌人
//   ↑ / ↓   系数 +0.05 / -0.05     （按住 Shift 为 ±0.01）
//   1 / 2 / 3  切换显示框档位：普通 168 / 精英 200 / Boss 232
//   [ / ]   宽度上限倍数 -0.1 / +0.1（默认 1.5）
//   R       当前敌人系数复位到 1.00
//   S       把所有系数写入 scale_report.txt
//   Esc     退出
#include <graphics.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static double g_scale = 1.0;
static int S(int v) { return (int)std::lround(v * g_scale); }

struct AssetImage {
    IMAGE img;
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    int w = 0, h = 0;
    int bx = 0, by = 0, bw = 0, bh = 0;
};

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
    bi.bmiHeader.biHeight = -a.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) { if (mem) DeleteDC(mem); if (bmp) DeleteObject(bmp); return false; }
    a.oldBmp = SelectObject(mem, bmp);
    DWORD* dst = static_cast<DWORD*>(bits);
    const int total = a.w * a.h;
    int minX = a.w, minY = a.h, maxX = -1, maxY = -1;
    for (int i = 0; i < total; ++i) {
        DWORD v = src[i];
        DWORD al = (v >> 24) & 0xFF;
        if (al == 0) { dst[i] = 0; continue; }
        DWORD r = (v >> 16) & 0xFF, g = (v >> 8) & 0xFF, b = v & 0xFF;
        dst[i] = (al << 24) | (((r * al) / 255) << 16) |
                 (((g * al) / 255) << 8) | ((b * al) / 255);
        int x = i % a.w, y = i / a.w;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }
    if (maxX < minX) {
        for (int i = 0; i < total; ++i) {
            DWORD v = src[i];
            dst[i] = 0xFF000000u | (v & 0x00FFFFFFu);
        }
        a.bx = a.by = 0; a.bw = a.w; a.bh = a.h;
    } else {
        a.bx = minX; a.by = minY;
        a.bw = maxX - minX + 1;
        a.bh = maxY - minY + 1;
    }
    a.dc = mem; a.bmp = bmp;
    return true;
}

// 与 gui.cpp 的 drawImageFit 完全一致（含 v2.7 的高度贴合 + 宽度上限）
void drawImageFit(HDC dst, int bx, int by, int bw, int bh,
                  const AssetImage* a, bool alignBottom, int maxWDesign) {
    if (!a || a->bw <= 0 || a->bh <= 0 || !a->dc) return;
    int boxW = S(bw), boxH = S(bh);
    if (boxW <= 0 || boxH <= 0) return;
    int limW = maxWDesign > 0 ? S(maxWDesign) : boxW;
    float k = std::min((float)boxH / a->bh, (float)limW / a->bw);
    if (k <= 0) return;
    int dw = std::max(1, (int)std::lround(a->bw * k));
    int dh = std::max(1, (int)std::lround(a->bh * k));
    int dx = S(bx) + (boxW - dw) / 2;
    int dy = alignBottom ? (S(by) + boxH - dh) : (S(by) + (boxH - dh) / 2);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    SetStretchBltMode(dst, COLORONCOLOR);
    AlphaBlend(dst, dx, dy, dw, dh, a->dc, a->bx, a->by, a->bw, a->bh, bf);
    // 顺便把实际绘制尺寸回传给调用方（用全局变量）
    extern int g_drawW, g_drawH;
    g_drawW = (int)std::lround(dw / g_scale);
    g_drawH = (int)std::lround(dh / g_scale);
}

int g_drawW = 0, g_drawH = 0;

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

void text(int x, int y, const wchar_t* s, int size, COLORREF c, int center = 0) {
    settextcolor(c);
    settextstyle(S(size), 0, L"微软雅黑");
    int px = S(x);
    if (center) px -= textwidth(s) / 2;
    outtextxy(px, S(y), s);
}

int main() {
    // 与 gui.cpp 相同：动态取 SetProcessDPIAware，避免不同 SDK 头里的声明差异
    if (HMODULE u32 = GetModuleHandleW(L"user32.dll")) {
        if (auto fn = (BOOL(WINAPI*)())GetProcAddress(u32, "SetProcessDPIAware")) fn();
    }
    HDC sc = GetDC(0);
    g_scale = GetDeviceCaps(sc, LOGPIXELSX) / 96.0;
    ReleaseDC(0, sc);
    initgraph((int)std::lround(1280 * g_scale), (int)std::lround(720 * g_scale));

    const std::wstring dir = L"d:\\My Files\\Codes\\projects\\EasyX_homework\\BINS\\assets\\enemies";
    std::vector<std::wstring> files;
    WIN32_FIND_DATAW fd{};
    HANDLE hf = FindFirstFileW((dir + L"\\*.png").c_str(), &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                files.push_back(dir + L"\\" + fd.cFileName);
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) return 1;

    std::vector<std::wstring> names;
    std::vector<AssetImage*> arts;
    std::vector<float> factors;
    for (auto& f : files) {
        std::wstring n = f.substr(f.find_last_of(L"\\/") + 1);
        n = n.substr(0, n.size() - 4);
        auto a = new AssetImage();
        if (loadimage(&a->img, f.c_str()) == 0 && buildAssetSurface(*a)) {
            names.push_back(n);
            arts.push_back(a);
            factors.push_back(1.0f);
        } else {
            delete a;
        }
    }

    int idx = 0;
    int boxTarget = 168;       // 普通
    float capRatio = 1.5f;

    BeginBatchDraw();
    bool running = true;
    while (running) {
        ExMessage m;
        while (peekmessage(&m, EX_KEY)) {
            if (m.message != WM_KEYDOWN) continue;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            float step = shift ? 0.01f : 0.05f;
            switch (m.vkcode) {
            case VK_ESCAPE: running = false; break;
            case VK_LEFT:  idx = (idx + (int)names.size() - 1) % (int)names.size(); break;
            case VK_RIGHT: idx = (idx + 1) % (int)names.size(); break;
            case VK_UP:    factors[idx] = std::min(3.0f, factors[idx] + step); break;
            case VK_DOWN:  factors[idx] = std::max(0.2f, factors[idx] - step); break;
            case '1': boxTarget = 168; break;
            case '2': boxTarget = 200; break;
            case '3': boxTarget = 232; break;
            case VK_OEM_4: capRatio = std::max(1.0f, capRatio - 0.1f); break;  // [
            case VK_OEM_6: capRatio = std::min(3.0f, capRatio + 0.1f); break;  // ]
            case 'R': factors[idx] = 1.0f; break;
            case 'S': {
                FILE* fp = fopen("scale_report.txt", "wb");
                if (fp) {
                    for (size_t i = 0; i < names.size(); ++i) {
                        char line[256];
                        snprintf(line, sizeof(line), "{ \"%s\", %.2ff },\n",
                                 toUtf8(names[i]).c_str(), factors[i]);
                        fwrite(line, 1, strlen(line), fp);
                    }
                    fclose(fp);
                }
                break;
            }
            }
        }

        // ---------- 场景 ----------
        setbkcolor(RGB(20, 18, 26));
        cleardevice();
        setfillcolor(RGB(28, 24, 34));
        solidrectangle(S(20), S(56), S(1260), S(520));
        setlinecolor(RGB(60, 54, 72));
        setlinestyle(PS_SOLID, 1);
        rectangle(S(20), S(56), S(1260), S(520));
        line(S(640), S(56), S(640), S(520));
        line(S(920), S(56), S(920), S(520));

        // 地面线 + 各锚点参考
        setlinecolor(RGB(70, 120, 90));
        line(S(20), S(468), S(1260), S(468));
        setlinecolor(RGB(80, 70, 96));
        line(S(20), S(108), S(620), S(108));          // 效果区起点
        setlinecolor(RGB(120, 100, 60));
        rectangle(S(6), S(64), S(620), S(104));       // 意图框
        text(14, 70, L"意图框", 12, RGB(120, 104, 70));
        text(14, 112, L"效果区（从此向下流动）", 12, RGB(110, 100, 130));

        // 名字 / 血条（位置参考）
        setlinecolor(RGB(150, 60, 60));
        rectangle(S(8), S(496), S(616), S(516));
        text(312, 500, L"血条 496..516", 12, RGB(170, 90, 90), 1);
        text(312, 472, names[idx].c_str(), 18, RGB(230, 226, 240), 1);

        // 玩家框参考
        setlinecolor(RGB(70, 90, 120));
        rectangle(S(940), S(252), S(1260), S(468));
        text(1100, 440, L"玩家框 320x216", 12, RGB(90, 110, 140), 1);

        // ---------- 立绘 ----------
        int side = (int)std::lround(boxTarget * factors[idx]);
        int maxW = std::min((int)std::lround(side * capRatio), 636);
        int cx = 320;
        int artX = cx - side / 2, artY = 468 - side;

        setlinecolor(RGB(150, 60, 60));
        setlinestyle(PS_DOT, 1);
        rectangle(S(artX), S(artY), S(artX + side), S(artY + side));
        setlinestyle(PS_SOLID, 1);
        drawImageFit(GetImageHDC(), artX, artY, side, side, arts[idx], true, maxW);

        // ---------- HUD ----------
        setfillcolor(RGB(16, 14, 20));
        solidrectangle(0, S(640), S(1280), S(720));
        wchar_t buf[512];
        setlinecolor(RGB(60, 54, 72));
        line(0, S(640), S(1280), S(640));

        swprintf(buf, 512, L"%s   [%d/%d]   档位 %d  系数 %.2f  框 %d  绘制 %dx%d  占框 %d%%",
                 names[idx].c_str(), idx + 1, (int)names.size(), boxTarget,
                 factors[idx], side, g_drawW, g_drawH, side > 0 ? g_drawH * 100 / side : 0);
        text(24, 652, buf, 16, RGB(235, 230, 245));

        swprintf(buf, 512, L"宽度上限 %.1f 倍 = %d    包围盒 %dx%d",
                 capRatio, maxW, arts[idx]->bw, arts[idx]->bh);
        text(24, 678, buf, 14, RGB(150, 200, 230));

        text(24, 700, L"←/→ 换怪   ↑/↓ 系数(Shift 精调)   1/2/3 普通/精英/Boss   [ ] 宽度上限   R 复位   S 导出   Esc 退出",
             13, RGB(140, 134, 152));

        FlushBatchDraw();
        Sleep(8);
    }
    EndBatchDraw();
    closegraph();
    return 0;
}
