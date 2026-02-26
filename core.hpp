#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
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

namespace fs = std::filesystem;
using namespace std;

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
        while (pos < src.size() && isspace(src[pos])) pos++;
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
        if (curr == '"') {
            string val; pos++;
            while (pos < src.size()) {
                if (src[pos] == '\\' && pos + 1 < src.size() && src[pos+1] == '"') {
                    val += '"';
                    pos += 2;
                } else if (src[pos] == '"') {
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

    ParlelEngine() {
        // Default Operators
        ops.push_back({'+', 1, [](auto a, auto b) {
            if (holds_alternative<float>(a) && holds_alternative<float>(b)) return variant<float,string>(get<float>(a) + get<float>(b));
            string sa = holds_alternative<string>(a) ? get<string>(a) : to_string(get<float>(a));
            string sb = holds_alternative<string>(b) ? get<string>(b) : to_string(get<float>(b));
            return variant<float,string>(sa + sb);
        }});
        ops.push_back({'-', 1, [](auto a, auto b) { return variant<float,string>(get<float>(a) - get<float>(b)); }});
        ops.push_back({'*', 2, [](auto a, auto b) { return variant<float,string>(get<float>(a) * get<float>(b)); }});
        ops.push_back({'/', 2, [](auto a, auto b) { return variant<float,string>(get<float>(a) / get<float>(b)); }});

        // Default Functions
        funcs.push_back({"inc", 1, [](ParlelEngine* eng, auto v) {
            string path = get<string>(v[0]);
            eng->runFile(path);
            return 0.0f;
        }});

        funcs.push_back({"mod", 1, [](ParlelEngine* eng, auto v) {
            string modName = get<string>(v[0]);
            eng->loadDynamicModule(modName);
            return 0.0f;
        }});
    }

    void registerModule(const Module& m) {
        for (auto& o : m.ops) ops.push_back(o);
        for (auto& f : m.funcs) funcs.push_back(f);
    }

    void loadDynamicModule(string modName) {
        if (loadedModules.count(modName)) return;

        fs::path baseDir(currentScriptDir);
        vector<fs::path> searchPaths = {
            baseDir,
            baseDir / modName,
            baseDir / "Mods" / modName,
        };

#ifdef _WIN32
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        fs::path exeDir = fs::path(exePath).parent_path();
        searchPaths.push_back(exeDir);
        searchPaths.push_back(exeDir / "Mods" / modName);
#endif

        // Add parent directory searches for development environments
        searchPaths.push_back(baseDir / ".." / "Mods" / modName);

        fs::path cppPath, dllPath;
        bool found = false;

        string modNameLower = modName;
        transform(modNameLower.begin(), modNameLower.end(), modNameLower.begin(), ::tolower);

        for (const auto& dir : searchPaths) {
            vector<string> names = {modName, modNameLower};
            for (const auto& name : names) {
                fs::path p_cpp = dir / (name + ".cpp");
                fs::path p_dll = dir / (name + ".dll");

                if (fs::exists(p_cpp)) {
                    cppPath = p_cpp;
                    dllPath = p_cpp.parent_path() / (name + ".dll");
                    found = true;
                    break;
                }
                if (fs::exists(p_dll)) {
                    dllPath = p_dll;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }

        if (!found) {
            string cwd = fs::current_path().string();
            stringstream ss;
            ss << "Modül bulunamadı: '" << modName << "'\nAranan yollar:";
            for (const auto& p : searchPaths) ss << "\n - " << p.string();
            ss << "\nÇalışma dizini: " << cwd;
            throw runtime_error(ss.str());
        }

        if (fs::exists(cppPath)) {
            cout << "[Engine] " << modName << ".cpp derleniyor..." << endl;
            string cmd = "cl /LD /EHsc /std:c++17 \"" + cppPath.string() + "\" /Fe:\"" + dllPath.string() + "\" > nul 2>&1";
            int res = system(cmd.c_str());
            if (res != 0) {
                throw runtime_error("Derleme hatası! '" + cppPath.string() + "' derlenemedi. 'cl' komutunun açık olduğundan emin olun.");
            }
        }

#ifdef _WIN32
        HMODULE hMod = LoadLibraryA(dllPath.string().c_str());
        if (!hMod) {
            DWORD err = GetLastError();
            throw runtime_error("Modül yüklenemedi: " + dllPath.string() + " (Sistem Hatası: " + to_string(err) + ")");
        }

        typedef Module (*GetModFunc)();
        GetModFunc getMod = (GetModFunc)GetProcAddress(hMod, "GetParlelModule");
        if (!getMod) {
            FreeLibrary(hMod);
            throw runtime_error("Modül 'GetParlelModule' fonksiyonunu bulamadı: " + dllPath.string());
        }
#else
        void* hMod = dlopen(dllPath.string().c_str(), RTLD_LAZY);
        if (!hMod) throw runtime_error("Modül yüklenemedi: " + dllPath.string());
        
        typedef Module (*GetModFunc)();
        GetModFunc getMod = (GetModFunc)dlsym(hMod, "GetParlelModule");
        if (!getMod) {
            dlclose(hMod);
            throw runtime_error("Modül 'GetParlelModule' fonksiyonunu bulamadı: " + dllPath.string());
        }
#endif

        registerModule(getMod());
        loadedModules.insert(modName);
        cout << "[Engine] Modül '" << modName << "' yüklendi." << endl;
    }

    variant<float, string> execute(const string& input);

    void runFile(const string& filename) {
        fs::path filePath(filename);
        if (filePath.is_relative()) {
            filePath = fs::path(currentScriptDir) / filePath;
        }

        if (!fs::exists(filePath)) {
            throw runtime_error("Dosya bulunamadı: " + filePath.string());
        }

        string absPath = fs::absolute(filePath).string();
        if (includedFiles.count(absPath)) return;
        includedFiles.insert(absPath);

        string oldDir = currentScriptDir;
        currentScriptDir = filePath.parent_path().string();
        if (currentScriptDir.empty()) currentScriptDir = ".";

        ifstream file(absPath);
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            try {
                execute(line);
            } catch (const exception& e) {
                cout << "Hata [" << absPath << "]: " << e.what() << endl;
            }
        }
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
    if (curr.type == T_EOF) return 0.0f;
    return expression(lexer, curr);
}
