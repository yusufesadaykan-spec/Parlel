#include "core.hpp"
#include "math_module.hpp"

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
    engine.registerModule(CreateMathModule());

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

    cout << "--- Parlel Modular Engine Test ---" << endl;
    try {
        // Test nested math and precedence
        cout << "10 + 5 * 2 = "; engine.execute("print(10 + 5 * 2)");
        cout << "(10 + 5) * 2 = "; engine.execute("print((10 + 5) * 2)");

        // Test custom module function (pow)
        cout << "pow(2, 10) = "; engine.execute("print(pow(2, 10))");

        // Test variables
        engine.execute("var(\"x\", 5)");
        engine.execute("var(\"y\", sqrt(x + 11))"); // Nested function and addition
        cout << "Variable y (sqrt(5 + 11)) = "; engine.execute("print(y)");

    } catch (const exception& e) {
        cout << "Test Error: " << e.what() << endl;
    }

    return 0;
}
