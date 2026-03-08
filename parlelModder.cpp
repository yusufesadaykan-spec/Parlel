#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <sstream>

using namespace std;
namespace fs = std::filesystem;

struct ModFunc {
    string name;
    int argCount;
    string code;
};

void writeString(ofstream& out, const string& s) {
    uint32_t len = (uint32_t)s.length();
    out.write((char*)&len, 4);
    out.write(s.c_str(), len);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Kullanim: parlelModder <kaynak_dosya_yolu (.cpp)>" << endl;
        return 1;
    }

    string inputPath = argv[1];
    fs::path p(inputPath);

    if (!fs::exists(p)) {
        cout << "[Hata] Dosya bulunamadi: " << inputPath << endl;
        return 1;
    }

    ifstream in(inputPath);
    stringstream buffer;
    buffer << in.rdbuf();
    string content = buffer.str();
    in.close();

    string moduleName = p.stem().string();
    vector<ModFunc> funcs;

    cout << "[Modder] C++ kaynak dosyasi isleniyor: " << p.filename() << endl;

    // 1. Find Module Name
    regex nameRegex(R"prm(m\.name\s*=\s*"([^"]+)")prm");
    smatch nameMatch;
    if (regex_search(content, nameMatch, nameRegex)) {
        moduleName = nameMatch[1];
    }

        // 2. Enhanced Lambda/String Extractor
        // Captures: m.funcs.push_back({"name", count, "code" OR [captures](args){ return body; }})
        regex funcRegex(R"prm(m\.funcs\.push_back\(\{\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*(?:"([^"]+)"|\[.*?\]\s*\((.*?)\)\s*\{\s*return\s+(.*?)\s*;\s*\})\s*\}\))prm");
        auto words_begin = sregex_iterator(content.begin(), content.end(), funcRegex);
        auto words_end = sregex_iterator();

        for (sregex_iterator i = words_begin; i != words_end; ++i) {
            smatch match = *i;
            string name = match[1];
            int argCount = stoi(match[2]);
            string code;

            if (match[3].matched) {
                code = match[3].str();
            } else {
                string params = match[4].str();
                code = match[5].str();
                
                // Map native parameter names to arg0, arg1...
                vector<string> paramNames;
                regex paramSplit(R"((?:auto\s+)?([a-zA-Z_0-9]+))");
                auto p_begin = sregex_iterator(params.begin(), params.end(), paramSplit);
                auto p_end = sregex_iterator();
                for (sregex_iterator p = p_begin; p != p_end; ++p) {
                    paramNames.push_back((*p)[1]);
                }

                for (size_t pIdx = 0; pIdx < paramNames.size(); ++pIdx) {
                    // Replace word-bound parameter with argN
                    regex r("\\b" + paramNames[pIdx] + "\\b");
                    code = regex_replace(code, r, "arg" + to_string(pIdx));
                }
            }
            
            funcs.push_back({name, argCount, code});
            cout << "   [+] Fonksiyon eklendi: " << name << " (" << argCount << " arg)" << endl;
        }

    // 3. Output .prm Binary
    string outputPath = p.replace_extension(".prm").string();
    ofstream out(outputPath, ios::binary);
    
    // Magic Number: PRM1
    uint32_t magic = 0x50524D31;
    out.write((char*)&magic, 4);

    writeString(out, moduleName);
    
    uint32_t funcSize = (uint32_t)funcs.size();
    out.write((char*)&funcSize, 4);

    for (const auto& f : funcs) {
        uint8_t type = 0; // Function
        out.write((char*)&type, 1);
        writeString(out, f.name);
        out.write((char*)&f.argCount, 4);
        writeString(out, f.code);
    }

    out.close();
    cout << "[Basari] Mod dosyasi olusturuldu: " << outputPath << endl;

    return 0;
}
