#include "core.hpp"
#include <map>
#include <vector>
#include <string>

using namespace std;

// Persistent storage for lists and tables
static map<float, vector<variant<float, string>>> prl_lists;
static map<float, map<string, variant<float, string>>> prl_tables;
static float next_handle = 1.0f;

extern "C" __declspec(dllexport) Module GetParlelModule() {
    Module m;
    m.name = "VanillaP";

    // --- Logical Controls ---

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
