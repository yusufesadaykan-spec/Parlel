// Parlel Vanilla Module (Updated & Working)
#include "core.hpp"

Module GetParlelModule() {
    Module m;
    m.name = "VanillaP";

    // Mathematical Utilities
    m.funcs.push_back({"abs", 1, [](auto x) { return math_abs(x); }});
    m.funcs.push_back({"sin", 1, [](auto degrees) { return math_sin(degrees); }});
    m.funcs.push_back({"cos", 1, [](auto degrees) { return math_cos(degrees); }});
    m.funcs.push_back({"tan", 1, [](auto degrees) { return math_tan(degrees); }});
    m.funcs.push_back({"sqrt", 1, [](auto x) { return math_sqrt(x); }});
    m.funcs.push_back({"pow", 2, [](auto x, auto y) { return math_pow(x, y); }});
    m.funcs.push_back({"min", 2, [](auto a, auto b) { return math_min(a, b); }});
    m.funcs.push_back({"max", 2, [](auto a, auto b) { return math_max(a, b); }});
    m.funcs.push_back({"random", 0, []() { return math_rand(); }});

    // System Utilities
    m.funcs.push_back({"sleep", 1, [](auto ms) { return sys_sleep(ms); }});
    m.funcs.push_back({"time", 0, []() { return sys_time(); }});

    return m;
}
