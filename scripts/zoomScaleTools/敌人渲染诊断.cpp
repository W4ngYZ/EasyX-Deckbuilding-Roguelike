// 敌人立绘渲染诊断：用与 gui.cpp 完全相同的实现（预乘 alpha + alpha 包围盒 + 等比贴合）
// 把 BINS/assets/enemies 下所有图渲染成一张对照表，并输出每张图的包围盒与实际绘制尺寸。
// 只读素材、只写自己的输出目录，不改动项目代码。
#include <graphics.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

struct AssetImage {
    IMAGE img;
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
    int w = 0, h = 0;
    int bx = 0, by = 0, bw = 0, bh = 0;
};

// 与 gui.cpp buildAssetSurface 同一套逻辑
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
    a.dc = mem; a.bmp = bmp; a.bits = bits;
    return true;
}

// 与 gui.cpp drawImageFit 同一套逻辑（scale = 1）
void drawImageFit(HDC dst, int boxX, int boxY, int boxW, int boxH,
                  const AssetImage* a, bool alignBottom) {
    if (!a || a->bw <= 0 || a->bh <= 0 || !a->dc) return;
    float k = std::min((float)boxW / a->bw, (float)boxH / a->bh);
    if (k <= 0) return;
    int dw = std::max(1, (int)std::lround(a->bw * k));
    int dh = std::max(1, (int)std::lround(a->bh * k));
    int dx = boxX + (boxW - dw) / 2;
    int dy = alignBottom ? (boxY + boxH - dh) : (boxY + (boxH - dh) / 2);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    SetStretchBltMode(dst, COLORONCOLOR);
    AlphaBlend(dst, dx, dy, dw, dh, a->dc, a->bx, a->by, a->bw, a->bh, bf);
}

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring baseName(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

int main() {
    initgraph(320, 240, 0);
    SetWorkingImage(nullptr);

    const std::wstring dir = L"d:\\My Files\\Codes\\projects\\EasyX_homework\\BINS\\assets\\enemies";
    std::vector<std::wstring> files;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((dir + L"\\*.png").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                files.push_back(dir + L"\\" + fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) { printf("no files\n"); closegraph(); return 1; }

    const int kCols = 5;
    const int kCellW = 240, kCellH = 268;
    const int kArt = 168;                    // 普通怪显示框，与 enemyLayout 一致
    const int kPadX = 16, kPadY = 34;
    const int rows = ((int)files.size() + kCols - 1) / kCols;

    IMAGE sheet(kCols * kCellW + kPadX * 2, rows * kCellH + kPadY + 24);
    SetWorkingImage(&sheet);
    setbkcolor(RGB(24, 21, 30));
    cleardevice();
    settextcolor(RGB(220, 214, 230));
    settextstyle(16, 0, L"微软雅黑");
    outtextxy(kPadX, 8, L"敌人立绘 168x168 显示框渲染对照（红框 = 显示框，虚线 = 实际绘制高度）");

    std::string report;
    wchar_t buf[512];

    for (size_t i = 0; i < files.size(); ++i) {
        int col = (int)i % kCols, row = (int)i / kCols;
        int cx = kPadX + col * kCellW;
        int cy = kPadY + row * kCellH + 18;

        auto a = new AssetImage();
        if (loadimage(&a->img, files[i].c_str()) == 0 && buildAssetSurface(*a)) {
            // 显示框
            setlinecolor(RGB(150, 60, 60));
            rectangle(cx, cy, cx + kArt, cy + kArt);

            HDC dc = GetImageHDC(&sheet);
            drawImageFit(dc, cx, cy, kArt, kArt, a, true);

            float k = std::min((float)kArt / a->bw, (float)kArt / a->bh);
            int dw = (int)std::lround(a->bw * k);
            int dh = (int)std::lround(a->bh * k);

            // 实际绘制高度参考线
            setlinecolor(RGB(90, 190, 120));
            int y = cy + kArt - dh;
            for (int x = cx; x < cx + kArt; x += 6) line(x, y, x + 3, y);

            std::wstring name = baseName(files[i]);
            if (name.size() > 4) name = name.substr(0, name.size() - 4);
            settextcolor(RGB(230, 225, 240));
            settextstyle(14, 0, L"微软雅黑");
            outtextxy(cx, cy + kArt + 4, name.c_str());
            settextcolor(RGB(150, 190, 220));
            settextstyle(12, 0, L"微软雅黑");
            swprintf(buf, 512, L"画布 %dx%d  包围盒 %dx%d", a->w, a->h, a->bw, a->bh);
            outtextxy(cx, cy + kArt + 22, buf);
            swprintf(buf, 512, L"绘制 %dx%d  填充 %.0f%%", dw, dh, 100.0 * dh / kArt);
            outtextxy(cx, cy + kArt + 38, buf);

            char line1[512];
            snprintf(line1, sizeof(line1), "%-18s canvas %4dx%-4d bbox %4dx%-4d aspect %.2f  k %.4f  draw %3dx%-3d  fill %3.0f%%",
                     toUtf8(name).c_str(), a->w, a->h, a->bw, a->bh,
                     (double)a->bw / a->bh, (double)k, dw, dh, 100.0 * dh / kArt);
            report += line1;
            report += "\n";
        } else {
            settextcolor(RGB(255, 120, 120));
            outtextxy(cx, cy + 60, L"加载失败");
            report += toUtf8(baseName(files[i])) + "  LOAD FAIL\n";
        }
        delete a;
    }

    SetWorkingImage(nullptr);
    saveimage(L"sheet.png", &sheet);

    FILE* f = fopen("report.txt", "wb");
    if (f) { fwrite(report.data(), 1, report.size(), f); fclose(f); }
    printf("%s", report.c_str());
    closegraph();
    return 0;
}
