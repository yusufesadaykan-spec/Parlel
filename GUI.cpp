#include "core.hpp"
#include <windows.h>
#include <gdiplus.h>
#include <map>
#include <vector>
#include <string>

#pragma comment (lib,"Gdiplus.lib")

using namespace Gdiplus;
using namespace std;

// --- GDI+ Helpers ---
static ULONG_PTR gdiplusToken;
static HWND g_hWnd = NULL;
static HDC g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static int g_width = 800;
static int g_height = 600;

struct GUI_Color {
    BYTE r, g, b, a;
};

// --- GUI State ---
struct Component {
    string type;
    int x, y, w, h;
    string text;
    GUI_Color color;
};

static vector<Component> g_components;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            BitBlt(hdc, 0, 0, g_width, g_height, g_hdcBuffer, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DrawRoundedRect(Graphics& g, Brush* brush, int x, int y, int w, int h, int r) {
    GraphicsPath path;
    path.AddArc(x, y, r, r, 180, 90);
    path.AddArc(x + w - r, y, r, r, 270, 90);
    path.AddArc(x + w - r, y + h - r, r, r, 0, 90);
    path.AddArc(x, y + h - r, r, r, 90, 90);
    path.CloseFigure();
    g.FillPath(brush, &path);
}

extern "C" __declspec(dllexport) Module GetParlelModule() {
    Module m;
    m.name = "GUI";

    m.funcs.push_back({"gui_init", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        g_width = (int)get<float>(args[0]);
        g_height = (int)get<float>(args[1]);

        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

        WNDCLASS wc = {0};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"ParlelGUI";
        RegisterClass(&wc);

        g_hWnd = CreateWindowEx(0, L"ParlelGUI", L"Parlel GUI Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, g_width, g_height, NULL, NULL, wc.hInstance, NULL);

        HDC hdc = GetDC(g_hWnd);
        g_hdcBuffer = CreateCompatibleDC(hdc);
        g_hbmBuffer = CreateCompatibleBitmap(hdc, g_width, g_height);
        SelectObject(g_hdcBuffer, g_hbmBuffer);
        ReleaseDC(g_hWnd, hdc);

        return 1.0f;
    }});

    m.funcs.push_back({"gui_clear", 3, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        Graphics g(g_hdcBuffer);
        Color color((BYTE)get<float>(args[0]), (BYTE)get<float>(args[1]), (BYTE)get<float>(args[2]));
        g.Clear(color);
        return 1.0f;
    }});

    m.funcs.push_back({"gui_frame", 7, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        Graphics g(g_hdcBuffer);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush brush(Color(255, (BYTE)get<float>(args[4]), (BYTE)get<float>(args[5]), (BYTE)get<float>(args[6])));
        DrawRoundedRect(g, &brush, (int)get<float>(args[0]), (int)get<float>(args[1]), (int)get<float>(args[2]), (int)get<float>(args[3]), 20);
        return 1.0f;
    }});

    m.funcs.push_back({"gui_button", 8, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        Graphics g(g_hdcBuffer);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        int x = (int)get<float>(args[0]);
        int y = (int)get<float>(args[1]);
        int w = (int)get<float>(args[2]);
        int h = (int)get<float>(args[3]);
        string text = get<string>(args[4]);
        
        SolidBrush brush(Color(255, (BYTE)get<float>(args[5]), (BYTE)get<float>(args[6]), (BYTE)get<float>(args[7])));
        DrawRoundedRect(g, &brush, x, y, w, h, 10);

        FontFamily fontFamily(L"Arial");
        Font font(&fontFamily, 12, FontStyleRegular, UnitPoint);
        SolidBrush textBrush(Color(255, 255, 255, 255));
        
        wstring wtext(text.begin(), text.end());
        RectF layoutRect((REAL)x, (REAL)y, (REAL)w, (REAL)h);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(wtext.c_str(), -1, &font, layoutRect, &format, &textBrush);

        return 1.0f;
    }});

    m.funcs.push_back({"gui_text", 6, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        Graphics g(g_hdcBuffer);
        int x = (int)get<float>(args[0]);
        int y = (int)get<float>(args[1]);
        string text = get<string>(args[2]);
        
        FontFamily fontFamily(L"Arial");
        Font font(&fontFamily, 14, FontStyleRegular, UnitPoint);
        SolidBrush brush(Color(255, (BYTE)get<float>(args[3]), (BYTE)get<float>(args[4]), (BYTE)get<float>(args[5])));
        
        wstring wtext(text.begin(), text.end());
        PointF point((REAL)x, (REAL)y);
        g.DrawString(wtext.c_str(), -1, &font, point, &brush);
        return 1.0f;
    }});

    m.funcs.push_back({"gui_textbox", 8, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        Graphics g(g_hdcBuffer);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        int x = (int)get<float>(args[0]);
        int y = (int)get<float>(args[1]);
        int w = (int)get<float>(args[2]);
        int h = (int)get<float>(args[3]);
        string text = get<string>(args[4]);

        SolidBrush bgBrush(Color(255, 255, 255, 255));
        DrawRoundedRect(g, &bgBrush, x, y, w, h, 5);
        
        Pen borderPen(Color(255, (BYTE)get<float>(args[5]), (BYTE)get<float>(args[6]), (BYTE)get<float>(args[7])), 2);
        GraphicsPath path;
        int r = 5;
        path.AddArc(x, y, r, r, 180, 90);
        path.AddArc(x + w - r, y, r, r, 270, 90);
        path.AddArc(x + w - r, y + h - r, r, r, 0, 90);
        path.AddArc(x, y + h - r, r, r, 90, 90);
        path.CloseFigure();
        g.DrawPath(&borderPen, &path);

        FontFamily fontFamily(L"Arial");
        Font font(&fontFamily, 12, FontStyleRegular, UnitPoint);
        SolidBrush textBrush(Color(255, 0, 0, 0));
        wstring wtext(text.begin(), text.end());
        g.DrawString(wtext.c_str(), -1, &font, PointF((REAL)x + 5, (REAL)y + 5), &textBrush);

        return 1.0f;
    }});

    m.funcs.push_back({"gui_update", 0, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        InvalidateRect(g_hWnd, NULL, FALSE);
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return 0.0f;
    }});

    m.funcs.push_back({"gui_should_close", 0, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>(IsWindow(g_hWnd) ? 0.0f : 1.0f);
    }});

    return m;
}
