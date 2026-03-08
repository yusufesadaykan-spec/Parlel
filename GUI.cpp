// Parlel GUI Module (Updated & Working)
#include "core.hpp"

Module GetParlelModule() {
    Module m;
    m.name = "GUI";

    // Window Management
    m.funcs.push_back({"init", 2, [](auto w, auto h) { return gui_init(w, h); }});
    m.funcs.push_back({"update", 0, []() { return gui_update(); }});
    m.funcs.push_back({"is_open", 0, []() { return 1 - gui_should_close(); }});

    // Drawing Operations
    m.funcs.push_back({"clear", 3, [](auto r, auto g, auto b) { return gui_clear(r, g, b); }});
    m.funcs.push_back({"rect", 7, [](auto x, auto y, auto w, auto h, auto r, auto g, auto b) { return gui_rect(x, y, w, h, r, g, b); }});
    m.funcs.push_back({"text", 6, [](auto x, auto y, auto content, auto r, auto g, auto b) { return gui_text(x, y, content, r, g, b); }});

    return m;
}
