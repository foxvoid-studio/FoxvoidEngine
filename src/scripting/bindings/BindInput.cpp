#include "scripting/ScriptBindings.hpp"
#include <raylib.h>
#include <input/InputManager.hpp>

void BindInput(py::module_& m) {
    // Create a submodule for Keys. This will act like a namespace/enum in Python.
    py::module_ keys = m.def_submodule("Keys", "Raylib Keyboard Keys");
    keys.attr("KEY_NULL")       = (int)KEY_NULL;
    keys.attr("KEY_APOSTROPHE") = (int)KEY_APOSTROPHE;
    keys.attr("KEY_COMMA")      = (int)KEY_COMMA;
    keys.attr("KEY_MINUS")      = (int)KEY_MINUS;
    keys.attr("KEY_PERIOD")     = (int)KEY_PERIOD;
    keys.attr("KEY_SLASH")      = (int)KEY_SLASH;

    keys.attr("KEY_ZERO")  = (int)KEY_ZERO;
    keys.attr("KEY_ONE")   = (int)KEY_ONE;
    keys.attr("KEY_TWO")   = (int)KEY_TWO;
    keys.attr("KEY_THREE") = (int)KEY_THREE;
    keys.attr("KEY_FOUR")  = (int)KEY_FOUR;
    keys.attr("KEY_FIVE")  = (int)KEY_FIVE;
    keys.attr("KEY_SIX")   = (int)KEY_SIX;
    keys.attr("KEY_SEVEN") = (int)KEY_SEVEN;
    keys.attr("KEY_EIGHT") = (int)KEY_EIGHT;
    keys.attr("KEY_NINE")  = (int)KEY_NINE;

    keys.attr("KEY_SEMICOLON") = (int)KEY_SEMICOLON;
    keys.attr("KEY_EQUAL")     = (int)KEY_EQUAL;

    keys.attr("KEY_A") = (int)KEY_A;
    keys.attr("KEY_B") = (int)KEY_B;
    keys.attr("KEY_C") = (int)KEY_C;
    keys.attr("KEY_D") = (int)KEY_D;
    keys.attr("KEY_E") = (int)KEY_E;
    keys.attr("KEY_F") = (int)KEY_F;
    keys.attr("KEY_G") = (int)KEY_G;
    keys.attr("KEY_H") = (int)KEY_H;
    keys.attr("KEY_I") = (int)KEY_I;
    keys.attr("KEY_J") = (int)KEY_J;
    keys.attr("KEY_K") = (int)KEY_K;
    keys.attr("KEY_L") = (int)KEY_L;
    keys.attr("KEY_M") = (int)KEY_M;
    keys.attr("KEY_N") = (int)KEY_N;
    keys.attr("KEY_O") = (int)KEY_O;
    keys.attr("KEY_P") = (int)KEY_P;
    keys.attr("KEY_Q") = (int)KEY_Q;
    keys.attr("KEY_R") = (int)KEY_R;
    keys.attr("KEY_S") = (int)KEY_S;
    keys.attr("KEY_T") = (int)KEY_T;
    keys.attr("KEY_U") = (int)KEY_U;
    keys.attr("KEY_V") = (int)KEY_V;
    keys.attr("KEY_W") = (int)KEY_W;
    keys.attr("KEY_X") = (int)KEY_X;
    keys.attr("KEY_Y") = (int)KEY_Y;
    keys.attr("KEY_Z") = (int)KEY_Z;

    keys.attr("KEY_LEFT_BRACKET")  = (int)KEY_LEFT_BRACKET;
    keys.attr("KEY_BACKSLASH")     = (int)KEY_BACKSLASH;
    keys.attr("KEY_RIGHT_BRACKET") = (int)KEY_RIGHT_BRACKET;
    keys.attr("KEY_GRAVE")         = (int)KEY_GRAVE;

    keys.attr("KEY_SPACE")        = (int)KEY_SPACE;
    keys.attr("KEY_ESCAPE")       = (int)KEY_ESCAPE;
    keys.attr("KEY_ENTER")        = (int)KEY_ENTER;
    keys.attr("KEY_TAB")          = (int)KEY_TAB;
    keys.attr("KEY_BACKSPACE")    = (int)KEY_BACKSPACE;
    keys.attr("KEY_INSERT")       = (int)KEY_INSERT;
    keys.attr("KEY_DELETE")       = (int)KEY_DELETE;
    keys.attr("RIGHT")            = (int)KEY_RIGHT;
    keys.attr("LEFT")             = (int)KEY_LEFT;
    keys.attr("DOWN")             = (int)KEY_DOWN;
    keys.attr("UP")               = (int)KEY_UP;
    keys.attr("KEY_PAGE_UP")      = (int)KEY_PAGE_UP;
    keys.attr("KEY_PAGE_DOWN")    = (int)KEY_PAGE_DOWN;
    keys.attr("KEY_HOME")         = (int)KEY_HOME;
    keys.attr("KEY_END")          = (int)KEY_END;
    keys.attr("KEY_CAPS_LOCK")    = (int)KEY_CAPS_LOCK;
    keys.attr("KEY_SCROLL_LOCK")  = (int)KEY_SCROLL_LOCK;
    keys.attr("KEY_NUM_LOCK")     = (int)KEY_NUM_LOCK;
    keys.attr("KEY_PRINT_SCREEN") = (int)KEY_PRINT_SCREEN;
    keys.attr("KEY_PAUSE")        = (int)KEY_PAUSE;

    keys.attr("KEY_F1")  = (int)KEY_F1;
    keys.attr("KEY_F2")  = (int)KEY_F2;
    keys.attr("KEY_F3")  = (int)KEY_F3;
    keys.attr("KEY_F4")  = (int)KEY_F4;
    keys.attr("KEY_F5")  = (int)KEY_F5;
    keys.attr("KEY_F6")  = (int)KEY_F6;
    keys.attr("KEY_F7")  = (int)KEY_F7;
    keys.attr("KEY_F8")  = (int)KEY_F8;
    keys.attr("KEY_F9")  = (int)KEY_F9;
    keys.attr("KEY_F10") = (int)KEY_F10;
    keys.attr("KEY_F11") = (int)KEY_F11;
    keys.attr("KEY_F12") = (int)KEY_F12;

    keys.attr("KEY_LEFT_SHIFT")    = (int)KEY_LEFT_SHIFT;
    keys.attr("KEY_LEFT_CONTROL")  = (int)KEY_LEFT_CONTROL;
    keys.attr("KEY_LEFT_ALT")      = (int)KEY_LEFT_ALT;
    keys.attr("KEY_LEFT_SUPER")    = (int)KEY_LEFT_SUPER;
    keys.attr("KEY_RIGHT_SHIFT")   = (int)KEY_RIGHT_SHIFT;
    keys.attr("KEY_RIGHT_CONTROL") = (int)KEY_RIGHT_CONTROL;
    keys.attr("KEY_RIGHT_ALT")     = (int)KEY_RIGHT_ALT;
    keys.attr("KEY_RIGHT_SUPER")   = (int)KEY_RIGHT_SUPER;
    keys.attr("KEY_KB_MENU")       = (int)KEY_KB_MENU;

    keys.attr("KEY_KP_0") = (int)KEY_KP_0;
    keys.attr("KEY_KP_1") = (int)KEY_KP_1;
    keys.attr("KEY_KP_2") = (int)KEY_KP_2;
    keys.attr("KEY_KP_3") = (int)KEY_KP_3;
    keys.attr("KEY_KP_4") = (int)KEY_KP_4;
    keys.attr("KEY_KP_5") = (int)KEY_KP_5;
    keys.attr("KEY_KP_6") = (int)KEY_KP_6;
    keys.attr("KEY_KP_7") = (int)KEY_KP_7;
    keys.attr("KEY_KP_8") = (int)KEY_KP_8;
    keys.attr("KEY_KP_9") = (int)KEY_KP_9;

    keys.attr("KEY_KP_DECIMAL")  = (int)KEY_KP_DECIMAL;
    keys.attr("KEY_KP_DIVIDE")   = (int)KEY_KP_DIVIDE;
    keys.attr("KEY_KP_MULTIPLY") = (int)KEY_KP_MULTIPLY;
    keys.attr("KEY_KP_SUBTRACT") = (int)KEY_KP_SUBTRACT;
    keys.attr("KEY_KP_ADD")      = (int)KEY_KP_ADD;
    keys.attr("KEY_KP_ENTER")    = (int)KEY_KP_ENTER;
    keys.attr("KEY_KP_EQUAL")    = (int)KEY_KP_EQUAL;

    keys.attr("KEY_BACK")        = (int)KEY_BACK;
    keys.attr("KEY_MENU")        = (int)KEY_MENU;
    keys.attr("KEY_VOLUME_UP")   = (int)KEY_VOLUME_UP;
    keys.attr("KEY_VOLUME_DOWN") = (int)KEY_VOLUME_DOWN;

    keys.attr("MOUSE_LEFT")   = 1001;
    keys.attr("MOUSE_RIGHT")  = 1002;
    keys.attr("MOUSE_MIDDLE") = 1003;

    keys.attr("INPUT_TOUCH_1") = 2001;
    keys.attr("INPUT_TOUCH_2") = 2002;
    keys.attr("INPUT_TOUCH_3") = 2003;

    py::module_ input = m.def_submodule("Input", "Engine Input API");

    // Direct key mapping
    input.def("is_key_down", [](int key) { return IsKeyDown(key); });
    input.def("is_key_pressed", [](int key) { return IsKeyPressed(key); });

    // Action-based mapping
    input.def("is_action_down", [](const std::string& action) {
        return InputManager::IsActionDown(action);
    });

    input.def("is_action_pressed", [](const std::string& action) {
        return InputManager::IsActionPressed(action);
    });
}
