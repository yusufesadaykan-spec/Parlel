#include "core.hpp"

int main(int argc, char* argv[]) {
    ParlelEngine engine;

    // Register builtin utility functions
    engine.funcs.push_back({"print", 1, [](ParlelEngine* eng, auto v) {
        if (holds_alternative<float>(v[0])) cout << get<float>(v[0]) << endl;
        else cout << get<string>(v[0]) << endl;
        return 0.0f;
    }});

    // Aliases for compatibility with original names
    auto varFunc = [](ParlelEngine* eng, auto v) {
        eng->variables[get<string>(v[0])] = v[1];
        return v[1];
    };
    engine.funcs.push_back({"var", 2, varFunc});
    engine.funcs.push_back({"varible", 2, varFunc});
    engine.funcs.push_back({"define", 2, [](ParlelEngine* eng, auto v) {
        eng->defines[get<string>(v[0])] = v[1];
        return v[1];
    }});

    // Developers can register their own modules here
    // engine.registerModule(...)

    string targetFile;
    if (argc > 1) {
        targetFile = argv[1];
    } else {
        cout << "Lutfen calisitirmak istediginiz .prl dosyasinin yolunu girin: ";
        getline(cin, targetFile);
    }

    if (!targetFile.empty()) {
        try {
            cout << "--- " << targetFile << " Calistiriliyor ---" << endl;
            engine.runFile(targetFile);
            cout << "--- Islem Tamamlandi ---" << endl;
        } catch (const exception& e) {
            cout << "Hata: " << e.what() << endl;
        }
        return 0;
    }
    else {
        cout << "Lutfen calisitirmak istediginiz .prl dosyasinin yolunu girin: ";
        getline(cin, targetFile);
    }

    return 0;
}
