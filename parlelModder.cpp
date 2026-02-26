#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace std;

int main() {
    cout << "--- Parlel Modder Tool ---" << endl;
    cout << "Derlemek istediginiz .cpp mod dosyasinin yolunu girin: ";
    
    string inputPath;
    getline(cin, inputPath);

    if (inputPath.empty()) {
        cout << "Hata: Yol bos olamaz!" << endl;
        return 1;
    }

    // Tırnak işaretlerini temizle (sürükle bırak yapıldığında gelebilir)
    if (inputPath.front() == '"' && inputPath.back() == '"') {
        inputPath = inputPath.substr(1, inputPath.length() - 2);
    }

    fs::path cppPath(inputPath);
    if (!fs::exists(cppPath)) {
        cout << "Hata: Dosya bulunamadi: " << inputPath << endl;
        return 1;
    }

    if (cppPath.extension() != ".cpp") {
        cout << "Hata: Bu bir .cpp dosyasi degil!" << endl;
        return 1;
    }

    string dllName = cppPath.stem().string() + ".dll";
    fs::path targetDir = cppPath.parent_path();
    fs::path dllPath = targetDir / dllName;

    cout << "[Modder] " << cppPath.filename().string() << " derleniyor..." << endl;

    // Komut oluşturma: /LD (DLL oluştur), /EHsc (Exception handling), /std:c++17
    string cmd = "cl /LD /EHsc /std:c++17 \"" + cppPath.string() + "\" /Fe:\"" + dllPath.string() + "\"";
    
    cout << "Komut calistiriliyor: " << cmd << endl;
    
    int result = system(cmd.c_str());

    if (result == 0) {
        cout << "\n[Basarili] Mod dosyasi hazir!" << endl;
        cout << "Olusturulan dosyalar:" << endl;
        cout << "- " << dllPath.filename().string() << " (Calisma dosyasi)" << endl;
        cout << "- " << cppPath.stem().string() << ".lib (Import kutuphanesi)" << endl;
    } else {
        cout << "\n[Hata] Derleme sirasinda bir hata olustu." << endl;
        cout << "Lutfen Visual Studio Developer Command Prompt (veya vcvars64.bat) acik oldugundan emin olun." << endl;
    }

    cout << "\nCikmak icin bir tusa basin..." << endl;
    cin.get();

    return result;
}
