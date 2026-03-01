#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif
#include "core.hpp"
#include <map>
#include <vector>
#include <string>
#include <cmath>
#include <ctime>
#include <thread>
#include <chrono>

using namespace std;

// Persistent storage for lists and tables
static map<float, vector<variant<float, string>>> prl_lists;
static map<float, map<string, variant<float, string>>> prl_tables;
static float next_handle = 1.0f;

#ifdef _WIN32
#define PRL_EXPORT __declspec(dllexport)
#else
#define PRL_EXPORT __attribute__((visibility("default")))
#endif

extern "C" PRL_EXPORT Module GetParlelModule() {
    Module m;
    m.name = "VanillaP";

    // --- Logical Controls ---
    // ... existing logic ...

    m.funcs.push_back({"if_prl", 3, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float condition = holds_alternative<float>(args[0]) ? get<float>(args[0]) : (get<string>(args[0]).empty() ? 0.0f : 1.0f);
        if (condition != 0.0f) {
            return eng->execute(get<string>(args[1]));
        } else {
            return eng->execute(get<string>(args[2]));
        }
    }});

    m.funcs.push_back({"while_prl", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        string cond_code = get<string>(args[0]);
        string body_code = get<string>(args[1]);
        variant<float, string> last_res = 0.0f;

        while (true) {
            auto cond_res = eng->execute(cond_code);
            float cond = holds_alternative<float>(cond_res) ? get<float>(cond_res) : (get<string>(cond_res).empty() ? 0.0f : 1.0f);
            if (cond == 0.0f) break;
            last_res = eng->execute(body_code);
        }
        return last_res;
    }});

    // --- Comparisons ---

    m.funcs.push_back({"eq", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>(args[0] == args[1] ? 1.0f : 0.0f);
    }});

    m.funcs.push_back({"lt", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        if (holds_alternative<float>(args[0]) && holds_alternative<float>(args[1]))
            return variant<float, string>(get<float>(args[0]) < get<float>(args[1]) ? 1.0f : 0.0f);
        return variant<float, string>(0.0f);
    }});

    m.funcs.push_back({"gt", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        if (holds_alternative<float>(args[0]) && holds_alternative<float>(args[1]))
            return variant<float, string>(get<float>(args[0]) > get<float>(args[1]) ? 1.0f : 0.0f);
        return variant<float, string>(0.0f);
    }});

    // --- Math Utilities ---

    m.funcs.push_back({"math_sin", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>(sinf(get<float>(args[0])));
    }});

    m.funcs.push_back({"math_cos", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>(cosf(get<float>(args[0])));
    }});

    m.funcs.push_back({"math_pow", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>(powf(get<float>(args[0]), get<float>(args[1])));
    }});

    m.funcs.push_back({"math_sqrt", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>(sqrtf(get<float>(args[0])));
    }});

    m.funcs.push_back({"math_abs", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>(fabsf(get<float>(args[0])));
    }});

    m.funcs.push_back({"math_rand", 0, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>((float)rand() / (float)RAND_MAX);
    }});

    // --- String Utilities ---

    m.funcs.push_back({"str_upper", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        string s = get<string>(args[0]);
        transform(s.begin(), s.end(), s.begin(), ::toupper);
        return variant<float, string>(s);
    }});

    m.funcs.push_back({"str_lower", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        string s = get<string>(args[0]);
        transform(s.begin(), s.end(), s.begin(), ::tolower);
        return variant<float, string>(s);
    }});

    m.funcs.push_back({"str_len", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        return variant<float, string>((float)get<string>(args[0]).length());
    }});

    // --- System Utilities ---

    m.funcs.push_back({"sys_sleep", 1, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        std::this_thread::sleep_for(std::chrono::milliseconds((long long)get<float>(args[0])));
        return 0.0f;
    }});

    m.funcs.push_back({"sys_time", 0, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return variant<float, string>((float)duration.count());
    }});

    // --- Loops ---

    m.funcs.push_back({"for_prl", 4, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        string var_name = get<string>(args[0]);
        float start = get<float>(args[1]);
        float end = get<float>(args[2]);
        string body = get<string>(args[3]);
        variant<float, string> last_res = 0.0f;

        for (float i = start; i < end; ++i) {
            eng->variables[var_name] = i;
            last_res = eng->execute(body);
        }
        return last_res;
    }});

    // --- Data Structures ---

    m.funcs.push_back({"list_new", 0, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float h = next_handle++;
        prl_lists[h] = {};
        return variant<float, string>(h);
    }});

    m.funcs.push_back({"list_add", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float h = get<float>(args[0]);
        prl_lists[h].push_back(args[1]);
        return args[1];
    }});

    m.funcs.push_back({"list_set", 3, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float h = get<float>(args[0]);
        int idx = (int)get<float>(args[1]);
        if (prl_lists.count(h) && idx >= 0 && idx < prl_lists[h].size()) {
            prl_lists[h][idx] = args[2];
            return args[2];
        }
        return variant<float, string>(0.0f);
    }});

    m.funcs.push_back({"list_get", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float h = get<float>(args[0]);
        int idx = (int)get<float>(args[1]);
        if (prl_lists.count(h) && idx >= 0 && idx < prl_lists[h].size())
            return prl_lists[h][idx];
        return variant<float, string>(0.0f);
    }});

    m.funcs.push_back({"table_new", 0, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float h = next_handle++;
        prl_tables[h] = {};
        return variant<float, string>(h);
    }});

    m.funcs.push_back({"table_set", 3, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float h = get<float>(args[0]);
        string key = get<string>(args[1]);
        prl_tables[h][key] = args[2];
        return args[2];
    }});

    m.funcs.push_back({"table_get", 2, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        float h = get<float>(args[0]);
        string key = get<string>(args[1]);
        if (prl_tables.count(h) && prl_tables[h].count(key))
            return prl_tables[h][key];
        return variant<float, string>(0.0f);
    }});

    // --- Function Definitions ---

    m.funcs.push_back({"def_prl", 3, [](ParlelEngine* eng, vector<variant<float, string>> args) {
        string name = get<string>(args[0]);
        string params_raw = get<string>(args[1]);
        string body = get<string>(args[2]);

        // Simple param parser
        vector<string> params;
        stringstream ss(params_raw);
        string p;
        while (getline(ss, p, ',')) {
            p.erase(0, p.find_first_not_of(" "));
            p.erase(p.find_last_not_of(" ") + 1);
            if (!p.empty()) params.push_back(p);
        }

        eng->funcs.push_back({name, (int)params.size(), [params, body](ParlelEngine* e, vector<variant<float, string>> vals) {
            // Push params as variables
            for (size_t i = 0; i < params.size(); ++i) {
                e->variables[params[i]] = vals[i];
            }
            return e->execute(body);
        }});
        return 1.0f;
    }});

    return m;
}
