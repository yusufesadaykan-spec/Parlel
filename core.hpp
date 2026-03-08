#ifdef _WIN32
#include <windows.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;
#endif

#include <iostream>
#include <vector>
#include <map>
#include <functional>
#include <variant>
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <set>
#include <filesystem>
#include <cmath>
#include <ctime>
#include <thread>
#include <chrono>

using namespace std;
namespace fs = std::filesystem;

// --- Native GUI State (Windows Only) ---
#ifdef _WIN32
static ULONG_PTR gdiplusToken;
static HWND g_hWnd = NULL;
static HDC g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static int g_width = 800;
static int g_height = 600;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        BitBlt(hdc, 0, 0, g_width, g_height, g_hdcBuffer, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DrawRoundedRect(Graphics& g, Brush* brush, int x, int y, int w, int h, int r) {
    GraphicsPath path;
    path.AddArc(x, y, r, r, 180, 90); path.AddArc(x + w - r, y, r, r, 270, 90);
    path.AddArc(x + w - r, y + h - r, r, r, 0, 90); path.AddArc(x, y + h - r, r, r, 90, 90);
    path.CloseFigure(); g.FillPath(brush, &path);
}
#endif

#define codeLeakAccepted true
#define Host "CPU"
#define Thread "GPU"

// --- Data Structures ---

struct Op {
    char OpProfile;
    int precedence;
    function<variant<float, string>(variant<float, string>, variant<float, string>)> call;
};

class ParlelEngine;

struct Func {
    string FuncProfile;
    int inputLength; 
    function<variant<float, string>(ParlelEngine*, vector<variant<float, string>>)> call;
};

struct Module {
    string name;
    vector<Op> ops;
    vector<Func> funcs;
};

// --- Lexer ---

enum Prl_TokenType {
    T_NUMBER, T_STRING, T_IDENTIFIER, T_OPERATOR, T_LPAREN, T_RPAREN, T_COMMA, T_EOF
};

struct Prl_Token {
    Prl_TokenType type;
    string value;
};

class Lexer {
    string src;
    size_t pos = 0;
public:
    Lexer(string source) : src(source) {}
    Prl_Token nextToken() {
        while (pos < src.size()) {
            if (isspace(src[pos])) { pos++; continue; }
            if (src[pos] == '/' && pos + 1 < src.size() && src[pos+1] == '/') {
                while (pos < src.size() && src[pos] != '\n') pos++;
                continue;
            }
            break;
        }
        if (pos >= src.size()) return {T_EOF, ""};
        char curr = src[pos];
        if (isdigit(curr) || (curr == '.' && pos + 1 < src.size() && isdigit(src[pos+1]))) {
            string val; bool dot = false;
            while (pos < src.size() && (isdigit(src[pos]) || src[pos] == '.')) {
                if (src[pos] == '.') { if (dot) break; dot = true; }
                val += src[pos++];
            }
            return {T_NUMBER, val};
        }
        if (isalpha(curr) || curr == '_') {
            string val;
            while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) val += src[pos++];
            return {T_IDENTIFIER, val};
        }
        if (curr == '"' || curr == '\'') {
            char quoteType = curr;
            string val; pos++;
            while (pos < src.size()) {
                if (src[pos] == '\\' && pos + 1 < src.size() && src[pos+1] == quoteType) {
                    val += quoteType;
                    pos += 2;
                } else if (src[pos] == quoteType) {
                    pos++;
                    break;
                } else {
                    val += src[pos++];
                }
            }
            return {T_STRING, val};
        }
        if (curr == '(') { pos++; return {T_LPAREN, "("}; }
        if (curr == ')') { pos++; return {T_RPAREN, ")"}; }
        if (curr == ',') { pos++; return {T_COMMA, ","}; }
        string op(1, src[pos++]);
        return {T_OPERATOR, op};
    }
    Prl_Token peekToken() {
        size_t oldPos = pos;
        Prl_Token t = nextToken();
        pos = oldPos;
        return t;
    }
};

// --- Engine ---

class ParlelEngine {
public:
    map<string, variant<float, string>> variables;
    map<string, variant<float, string>> defines;
    vector<Op> ops;
    vector<Func> funcs;
    set<string> includedFiles;
    set<string> loadedModules;
    string currentScriptDir = ".";

    // Persistent storage for shared lists/tables (Core feature now)
    static inline map<float, vector<variant<float, string>>> prl_lists;
    static inline map<float, map<string, variant<float, string>>> prl_tables;
    static inline float next_handle = 1.0f;

    ParlelEngine() {
        // Default Operators
        ops.push_back({'+', 1, [](variant<float, string> a, variant<float, string> b) -> variant<float, string> {
            if (holds_alternative<float>(a) && holds_alternative<float>(b)) return variant<float,string>(get<float>(a) + get<float>(b));
            string sa = holds_alternative<string>(a) ? get<string>(a) : to_string(get<float>(a));
            string sb = holds_alternative<string>(b) ? get<string>(b) : to_string(get<float>(b));
            return variant<float,string>(sa + sb);
        }});
        ops.push_back({'-', 1, [](variant<float, string> a, variant<float, string> b) -> variant<float, string> { return variant<float,string>(get<float>(a) - get<float>(b)); }});
        ops.push_back({'*', 2, [](variant<float, string> a, variant<float, string> b) -> variant<float, string> { return variant<float,string>(get<float>(a) * get<float>(b)); }});
        ops.push_back({'/', 2, [](variant<float, string> a, variant<float, string> b) -> variant<float, string> { return variant<float,string>(get<float>(a) / get<float>(b)); }});

        // --- Core Functions (formerly in VanillaP) ---
        
        funcs.push_back({"inc", 1, [](ParlelEngine* eng, auto v) {
            eng->runFile(get<string>(v[0]));
            return 0.0f;
        }});

        funcs.push_back({"mod", 1, [](ParlelEngine* eng, auto v) {
            eng->loadModule(get<string>(v[0]));
            return 0.0f;
        }});

        // Logic
        funcs.push_back({"if_prl", 3, [](ParlelEngine* eng, auto v) {
            float condition = holds_alternative<float>(v[0]) ? get<float>(v[0]) : (get<string>(v[0]).empty() ? 0.0f : 1.0f);
            return eng->execute(get<string>(condition != 0.0f ? v[1] : v[2]));
        }});

        funcs.push_back({"while_prl", 2, [](ParlelEngine* eng, auto v) {
            string cond_code = get<string>(v[0]);
            string body_code = get<string>(v[1]);
            variant<float, string> last_res = 0.0f;
            while (true) {
                auto cond_res = eng->execute(cond_code);
                float cond = holds_alternative<float>(cond_res) ? get<float>(cond_res) : (get<string>(cond_res).empty() ? 0.0f : 1.0f);
                if (cond == 0.0f) break;
                last_res = eng->execute(body_code);
            }
            return last_res;
        }});

        funcs.push_back({"for_prl", 4, [](ParlelEngine* eng, auto v) {
            string var_name = get<string>(v[0]);
            float start = get<float>(v[1]), end = get<float>(v[2]);
            string body = get<string>(v[3]);
            variant<float, string> last_res = 0.0f;
            for (float i = start; i < end; ++i) {
                eng->variables[var_name] = i;
                last_res = eng->execute(body);
            }
            return last_res;
        }});

        // Comparisons
        funcs.push_back({"eq", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>(v[0] == v[1] ? 1.0f : 0.0f); }});
        funcs.push_back({"lt", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { 
            if (holds_alternative<float>(v[0]) && holds_alternative<float>(v[1])) return variant<float,string>(get<float>(v[0]) < get<float>(v[1]) ? 1.0f : 0.0f);
            return variant<float,string>(0.0f);
        }});
        funcs.push_back({"gt", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { 
            if (holds_alternative<float>(v[0]) && holds_alternative<float>(v[1])) return variant<float,string>(get<float>(v[0]) > get<float>(v[1]) ? 1.0f : 0.0f);
            return variant<float,string>(0.0f);
        }});

        // Math
        funcs.push_back({"math_sin", 1, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>(sinf(get<float>(v[0]))); }});
        funcs.push_back({"math_cos", 1, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>(cosf(get<float>(v[0]))); }});
        funcs.push_back({"math_tan", 1, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>(tanf(get<float>(v[0]))); }});
        funcs.push_back({"math_pow", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>(powf(get<float>(v[0]), get<float>(v[1]))); }});
        funcs.push_back({"math_sqrt", 1, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>(sqrtf(get<float>(v[0]))); }});
        funcs.push_back({"math_abs", 1, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>(fabsf(get<float>(v[0]))); }});
        funcs.push_back({"math_rand", 0, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return variant<float,string>((float)rand()/(float)RAND_MAX); }});
        funcs.push_back({"math_min", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { float a=get<float>(v[0]), b=get<float>(v[1]); return a < b ? a : b; }});
        funcs.push_back({"math_max", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { float a=get<float>(v[0]), b=get<float>(v[1]); return a > b ? a : b; }});
        
        // System
        funcs.push_back({"sys_sleep", 1, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { std::this_thread::sleep_for(std::chrono::milliseconds((long long)get<float>(v[0]))); return 0.0f; }});
        funcs.push_back({"sys_time", 0, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return (float)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }});

        // GUI (Native Windows GDI+)
#ifdef _WIN32
        funcs.push_back({"gui_init", 2, [](ParlelEngine* eng, vector<variant<float, string>> v) -> variant<float, string> {
            if (g_hWnd) return 1.0f;
            g_width = (int)get<float>(v[0]); g_height = (int)get<float>(v[1]);
            GdiplusStartupInput si; GdiplusStartup(&gdiplusToken, &si, NULL);
            WNDCLASSW wc = {0}; wc.lpfnWndProc = WindowProc; wc.hInstance = GetModuleHandle(NULL);
            wc.lpszClassName = L"ParlelGUI"; RegisterClassW(&wc);
            g_hWnd = CreateWindowExW(0, L"ParlelGUI", L"Parlel GUI", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, g_width, g_height, NULL, NULL, wc.hInstance, NULL);
            HDC hdc = GetDC(g_hWnd); g_hdcBuffer = CreateCompatibleDC(hdc); g_hbmBuffer = CreateCompatibleBitmap(hdc, g_width, g_height);
            SelectObject(g_hdcBuffer, g_hbmBuffer); ReleaseDC(g_hWnd, hdc);
            return 1.0f;
        }});
        funcs.push_back({"gui_clear", 3, [](ParlelEngine* eng, vector<variant<float, string>> v) -> variant<float, string> { Graphics g(g_hdcBuffer); g.Clear(Color(255, (BYTE)get<float>(v[0]), (BYTE)get<float>(v[1]), (BYTE)get<float>(v[2]))); return 1.0f; }});
        funcs.push_back({"gui_rect", 7, [](ParlelEngine* eng, vector<variant<float, string>> v) -> variant<float, string> {
            Graphics g(g_hdcBuffer); g.SetSmoothingMode(SmoothingModeAntiAlias);
            SolidBrush b(Color(255, (BYTE)get<float>(v[4]), (BYTE)get<float>(v[5]), (BYTE)get<float>(v[6])));
            g.FillRectangle(&b, (REAL)get<float>(v[0]), (REAL)get<float>(v[1]), (REAL)get<float>(v[2]), (REAL)get<float>(v[3]));
            return 1.0f;
        }});
        funcs.push_back({"gui_text", 6, [](ParlelEngine* eng, vector<variant<float, string>> v) -> variant<float, string> {
            Graphics g(g_hdcBuffer); FontFamily ff(L"Arial"); Font f(&ff, 14, FontStyleRegular, UnitPoint);
            SolidBrush b(Color(255, (BYTE)get<float>(v[3]), (BYTE)get<float>(v[4]), (BYTE)get<float>(v[5])));
            string txt = get<string>(v[2]); wstring wtxt(txt.begin(), txt.end());
            g.DrawString(wtxt.c_str(), -1, &f, PointF((REAL)get<float>(v[0]), (REAL)get<float>(v[1])), &b);
            return 1.0f;
        }});
        funcs.push_back({"gui_update", 0, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { InvalidateRect(g_hWnd, NULL, FALSE); MSG msg; while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); } return 0.0f; }});
        funcs.push_back({"gui_should_close", 0, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { return (float)!IsWindow(g_hWnd); }});
#endif

        // Data structures
        funcs.push_back({"list_new", 0, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { float h = next_handle++; prl_lists[h] = {}; return h; }});
        funcs.push_back({"list_add", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { prl_lists[get<float>(v[0])].push_back(v[1]); return v[1]; }});
        funcs.push_back({"list_set", 3, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { 
            float h = get<float>(v[0]); int i = (int)get<float>(v[1]);
            if(prl_lists.count(h) && i>=0 && i<(int)prl_lists[h].size()) prl_lists[h][i] = v[2];
            return v[2];
        }});
        funcs.push_back({"list_get", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { auto& l = prl_lists[get<float>(v[0])]; int i = (int)get<float>(v[1]); return (i>=0 && i<(int)l.size())?l[i]:0.0f; }});
        
        funcs.push_back({"table_new", 0, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { float h = next_handle++; prl_tables[h] = {}; return h; }});
        funcs.push_back({"table_set", 3, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { prl_tables[get<float>(v[0])][get<string>(v[1])] = v[2]; return v[2]; }});
        funcs.push_back({"table_get", 2, [](ParlelEngine* e, vector<variant<float, string>> v) -> variant<float, string> { auto& t = prl_tables[get<float>(v[0])]; string k = get<string>(v[1]); return t.count(k)?t[k]:0.0f; }});

        // PRM Custom Function Definer
        funcs.push_back({"def_prl", 3, [](ParlelEngine* eng, auto v) {
            string name = get<string>(v[0]), params_raw = get<string>(v[1]), body = get<string>(v[2]);
            vector<string> params; stringstream ss(params_raw); string p;
            while(getline(ss, p, ',')) { p.erase(0, p.find_first_not_of(" ")); p.erase(p.find_last_not_of(" ") + 1); if(!p.empty()) params.push_back(p); }
            eng->funcs.push_back({name, (int)params.size(), [params, body](ParlelEngine* e, auto vals) {
                for (size_t i = 0; i < params.size(); ++i) e->variables[params[i]] = vals[i];
                return e->execute(body);
            }});
            return 1.0f;
        }});
    }

    void registerModule(const Module& m) {
        for (auto& o : m.ops) ops.push_back(o);
        for (auto& f : m.funcs) funcs.push_back(f);
    }

    void loadModule(string modName) {
        if (loadedModules.count(modName)) return;

        std::filesystem::path scriptDir(currentScriptDir);
        vector<std::filesystem::path> searchPaths = {
            scriptDir, 
            scriptDir / "Mods",
            ".",
            "./Mods",
            "./compiled"
        };
        
        std::filesystem::path prmPath; bool found = false;
        for (const auto& dir : searchPaths) {
            std::filesystem::path p = dir / (modName + ".prm");
            if (std::filesystem::exists(p)) { prmPath = p; found = true; break; }
        }

        if (!found) throw runtime_error("Module not found: " + modName);

        ifstream f(prmPath, ios::binary);
        uint32_t magic; f.read((char*)&magic, 4);
        if (magic != 0x50524D31) throw runtime_error("Invalid .prm format");

        uint32_t nameLen; f.read((char*)&nameLen, 4);
        string mName(nameLen, '\0'); f.read(&mName[0], nameLen);

        uint32_t itemCount; f.read((char*)&itemCount, 4);
        for (uint32_t i = 0; i < itemCount; ++i) {
            uint8_t type; f.read((char*)&type, 1);
            uint32_t nLen; f.read((char*)&nLen, 4);
            string name(nLen, '\0'); f.read(&name[0], nLen);
            
            if (type == 0) { // Function
                uint32_t argCount; f.read((char*)&argCount, 4);
                uint32_t codeLen; f.read((char*)&codeLen, 4);
                string code(codeLen, '\0'); f.read(&code[0], codeLen);
                
                funcs.push_back({name, (int)argCount, [code, argCount](ParlelEngine* eng, auto args) {
                    // Map args to arg0, arg1, etc.
                    for (int j = 0; j < (int)argCount && j < (int)args.size(); ++j) {
                        string argName = "arg" + to_string(j);
                        eng->variables[argName] = args[j];
                    }
                    return eng->execute(code);
                }});
            }
        }

        loadedModules.insert(modName);
        cout << "[Engine] Loaded .prm module: " << mName << endl;
    }

    variant<float, string> execute(const string& input);

    void runFile(const string& filename) {
        std::filesystem::path filePath(filename);
        if (filePath.is_relative()) {
            filePath = std::filesystem::path(currentScriptDir) / filePath;
        }

        if (!std::filesystem::exists(filePath)) {
            throw runtime_error("Dosya bulunamadı: " + filePath.string());
        }

        string absPath = std::filesystem::absolute(filePath).string();
        if (includedFiles.count(absPath)) return;
        includedFiles.insert(absPath);

        string oldDir = currentScriptDir;
        currentScriptDir = filePath.parent_path().string();
        if (currentScriptDir.empty()) currentScriptDir = ".";

        ifstream file(absPath);
        stringstream buffer;
        buffer << file.rdbuf();
        string content = buffer.str();
        execute(content);
        
        currentScriptDir = oldDir;
    }

private:
    variant<float, string> expression(Lexer& lex, Prl_Token& curr);
    variant<float, string> term(Lexer& lex, Prl_Token& curr);
    variant<float, string> factor(Lexer& lex, Prl_Token& curr);
    void eat(Lexer& lex, Prl_Token& curr, Prl_TokenType t) {
        if (curr.type == t) curr = lex.nextToken();
        else throw runtime_error("Beklenmedik token: " + curr.value);
    }
};

// --- Parser Implementation ---

variant<float, string> ParlelEngine::factor(Lexer& lex, Prl_Token& curr) {
    if (curr.type == T_NUMBER) {
        float val = stof(curr.value);
        eat(lex, curr, T_NUMBER);
        return val;
    } else if (curr.type == T_STRING) {
        string val = curr.value;
        eat(lex, curr, T_STRING);
        return val;
    } else if (curr.type == T_LPAREN) {
        eat(lex, curr, T_LPAREN);
        auto node = expression(lex, curr);
        eat(lex, curr, T_RPAREN);
        return node;
    } else if (curr.type == T_IDENTIFIER) {
        string name = curr.value;
        eat(lex, curr, T_IDENTIFIER);
        if (curr.type == T_LPAREN) {
            eat(lex, curr, T_LPAREN);
            vector<variant<float, string>> args;
            if (curr.type != T_RPAREN) {
                args.push_back(expression(lex, curr));
                while (curr.type == T_COMMA) {
                    eat(lex, curr, T_COMMA);
                    args.push_back(expression(lex, curr));
                }
            }
            eat(lex, curr, T_RPAREN);
            for (auto& f : funcs) if (f.FuncProfile == name) return f.call(this, args);
            throw runtime_error("Fonksiyon bulunamadı: " + name);
        } else {
            if (variables.count(name)) return variables[name];
            if (defines.count(name)) return defines[name];
            return name;
        }
    }
    throw runtime_error("Beklenmedik ifade (factor): " + curr.value);
}

variant<float, string> ParlelEngine::term(Lexer& lex, Prl_Token& curr) {
    auto node = factor(lex, curr);
    while (curr.type == T_OPERATOR && (curr.value == "*" || curr.value == "/")) {
        char opChar = curr.value[0];
        eat(lex, curr, T_OPERATOR);
        auto right = factor(lex, curr);
        for (auto& op : ops) if (op.OpProfile == opChar) { node = op.call(node, right); break; }
    }
    return node;
}

variant<float, string> ParlelEngine::expression(Lexer& lex, Prl_Token& curr) {
    if (curr.type == T_IDENTIFIER) {
        if (lex.peekToken().value == "=") {
            string name = curr.value;
            eat(lex, curr, T_IDENTIFIER);
            eat(lex, curr, T_OPERATOR);
            auto val = expression(lex, curr);
            variables[name] = val;
            return val;
        }
    }

    auto node = term(lex, curr);
    while (curr.type == T_OPERATOR && (curr.value == "+" || curr.value == "-")) {
        char opChar = curr.value[0];
        eat(lex, curr, T_OPERATOR);
        auto right = term(lex, curr);
        for (auto& op : ops) if (op.OpProfile == opChar) { node = op.call(node, right); break; }
    }
    return node;
}

variant<float, string> ParlelEngine::execute(const string& input) {
    Lexer lexer(input);
    Prl_Token curr = lexer.nextToken();
    variant<float, string> last_res = 0.0f;
    while (curr.type != T_EOF) {
        last_res = expression(lexer, curr);
    }
    return last_res;
}
