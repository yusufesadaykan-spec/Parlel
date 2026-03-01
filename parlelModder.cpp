#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <vector>

namespace fs = std::filesystem;
using namespace std;

bool checkCompiler(const string& cmd) {
#ifdef _WIN32
    string check = cmd + " > nul 2>&1";
#else
    string check = cmd + " > /dev/null 2>&1";
#endif
    return system(check.c_str()) == 0;
}

int main() {
    cout << "--- Parlel Modder Tool (Cross-Platform) ---" << endl;
    cout << "Derlemek istediginiz .cpp mod dosyasinin yolunu girin: ";
    
    string inputPath;
    getline(cin, inputPath);

    if (inputPath.empty()) {
        cout << "Hata: Yol bos olamaz!" << endl;
        return 1;
    }

    if (inputPath.front() == '"' && inputPath.back() == '"') {
        inputPath = inputPath.substr(1, inputPath.length() - 2);
    }

    fs::path cppPath(inputPath);
    if (!fs::exists(cppPath)) {
        cout << "Hata: Dosya bulunamadi: " << inputPath << endl;
        return 1;
    }

#ifdef _WIN32
    string libExt = ".dll";
#else
    string libExt = ".so";
#endif

    string libName = cppPath.stem().string() + libExt;
    fs::path libPath = cppPath.parent_path() / libName;

    cout << "[Modder] " << cppPath.filename().string() << " derleniyor..." << endl;

    string cmd;
    bool compiled = false;

#ifdef _WIN32
    // Windows: Try cl first, then g++
    if (checkCompiler("cl /?")) {
        cmd = "cl /LD /EHsc /std:c++17 \"" + cppPath.string() + "\" /Fe:\"" + libPath.string() + "\"";
    } else if (checkCompiler("g++ --version")) {
        cmd = "g++ -shared -fPIC -std=c++17 \"" + cppPath.string() + "\" -o \"" + libPath.string() + "\"";
    } else {
        cout << "[Hata] Derleyici bulunamadi! 'cl' veya 'g++' kurulu oldugundan emin olun." << endl;
        return 1;
    }
#else
    // Linux/macOS: Try g++ or clang++
    if (checkCompiler("g++ --version")) {
        cmd = "g++ -shared -fPIC -std=c++17 \"" + cppPath.string() + "\" -o \"" + libPath.string() + "\"";
    } else if (checkCompiler("clang++ --version")) {
        cmd = "clang++ -shared -fPIC -std=c++17 \"" + cppPath.string() + "\" -o \"" + libPath.string() + "\"";
    } else {
        cout << "[Hata] Derleyici bulunamadi! 'g++' veya 'clang++' kurulu oldugundan emin olun." << endl;
        return 1;
    }
#endif

    cout << "Komut calistiriliyor: " << cmd << endl;
    int result = system(cmd.c_str());

    if (result == 0) {
        cout << "\n[Basarili] Mod dosyasi hazir: " << libName << endl;
    } else {
        cout << "\n[Hata] Derleme sirasinda bir hata olustu." << endl;
    }

    cout << "\nCikmak icin Enter'a basin..." << endl;
    cin.get();

    return result;
}
