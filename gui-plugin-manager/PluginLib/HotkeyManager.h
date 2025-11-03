#pragma once
#include <Windows.h>

namespace HotkeyManager {
    // Hotkey entry with modifiers support
    struct Hotkey {
        int mainKey = 0;
        bool requireCtrl = false;
        bool requireShift = false;
        bool requireAlt = false;

        Hotkey() = default;
        Hotkey(int key, bool ctrl = false, bool shift = false, bool alt = false)
            : mainKey(key), requireCtrl(ctrl), requireShift(shift), requireAlt(alt) {}
    };

    // Check if a hotkey is currently pressed with its modifiers
    inline bool IsHotkeyPressed(const Hotkey& hotkey) {
        if (hotkey.mainKey == 0) return false;

        // Check main key is pressed (0x8000 = key is currently held)
        if (!(GetAsyncKeyState(hotkey.mainKey) & 0x8000)) {
            return false;
        }

        // Check CTRL modifier - if required, CTRL MUST be pressed
        bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (hotkey.requireCtrl && !ctrlPressed) {
            return false;
        }

        // Check SHIFT modifier - if required, SHIFT MUST be pressed
        bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (hotkey.requireShift && !shiftPressed) {
            return false;
        }

        // Check ALT modifier - if required, ALT MUST be pressed
        bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        if (hotkey.requireAlt && !altPressed) {
            return false;
        }

        return true;
    }

    // Get human-readable hotkey string
    inline const char* GetHotkeyString(int key, bool ctrl, bool shift, bool alt) {
        static char buffer[128];
        memset(buffer, 0, sizeof(buffer));

        if (key == 0) {
            strcpy_s(buffer, sizeof(buffer), "[Not Set]");
            return buffer;
        }

        std::string result;
        if (ctrl) result += "CTRL+";
        if (shift) result += "SHIFT+";
        if (alt) result += "ALT+";

        // Convert virtual key code to string
        switch (key) {
            // Function keys
            case VK_F1: result += "F1"; break;
            case VK_F2: result += "F2"; break;
            case VK_F3: result += "F3"; break;
            case VK_F4: result += "F4"; break;
            case VK_F5: result += "F5"; break;
            case VK_F6: result += "F6"; break;
            case VK_F7: result += "F7"; break;
            case VK_F8: result += "F8"; break;
            case VK_F9: result += "F9"; break;
            case VK_F10: result += "F10"; break;
            case VK_F11: result += "F11"; break;
            case VK_F12: result += "F12"; break;

            // Number keys (top row)
            case '0': result += "0"; break;
            case '1': result += "1"; break;
            case '2': result += "2"; break;
            case '3': result += "3"; break;
            case '4': result += "4"; break;
            case '5': result += "5"; break;
            case '6': result += "6"; break;
            case '7': result += "7"; break;
            case '8': result += "8"; break;
            case '9': result += "9"; break;

            // Numpad
            case VK_NUMPAD0: result += "NUMPAD0"; break;
            case VK_NUMPAD1: result += "NUMPAD1"; break;
            case VK_NUMPAD2: result += "NUMPAD2"; break;
            case VK_NUMPAD3: result += "NUMPAD3"; break;
            case VK_NUMPAD4: result += "NUMPAD4"; break;
            case VK_NUMPAD5: result += "NUMPAD5"; break;
            case VK_NUMPAD6: result += "NUMPAD6"; break;
            case VK_NUMPAD7: result += "NUMPAD7"; break;
            case VK_NUMPAD8: result += "NUMPAD8"; break;
            case VK_NUMPAD9: result += "NUMPAD9"; break;
            case VK_MULTIPLY: result += "NUMPAD*"; break;
            case VK_ADD: result += "NUMPAD+"; break;
            case VK_SUBTRACT: result += "NUMPAD-"; break;
            case VK_DIVIDE: result += "NUMPAD/"; break;
            case VK_DECIMAL: result += "NUMPAD."; break;

            // Letters
            case 'A': result += "A"; break;
            case 'B': result += "B"; break;
            case 'C': result += "C"; break;
            case 'D': result += "D"; break;
            case 'E': result += "E"; break;
            case 'F': result += "F"; break;
            case 'G': result += "G"; break;
            case 'H': result += "H"; break;
            case 'I': result += "I"; break;
            case 'J': result += "J"; break;
            case 'K': result += "K"; break;
            case 'L': result += "L"; break;
            case 'M': result += "M"; break;
            case 'N': result += "N"; break;
            case 'O': result += "O"; break;
            case 'P': result += "P"; break;
            case 'Q': result += "Q"; break;
            case 'R': result += "R"; break;
            case 'S': result += "S"; break;
            case 'T': result += "T"; break;
            case 'U': result += "U"; break;
            case 'V': result += "V"; break;
            case 'W': result += "W"; break;
            case 'X': result += "X"; break;
            case 'Y': result += "Y"; break;
            case 'Z': result += "Z"; break;

            // Special keys
            case VK_SPACE: result += "SPACE"; break;
            case VK_RETURN: result += "ENTER"; break;
            case VK_BACK: result += "BACKSPACE"; break;
            case VK_TAB: result += "TAB"; break;
            case VK_ESCAPE: result += "ESC"; break;
            case VK_INSERT: result += "INSERT"; break;
            case VK_DELETE: result += "DELETE"; break;
            case VK_HOME: result += "HOME"; break;
            case VK_END: result += "END"; break;
            case VK_PRIOR: result += "PAGEUP"; break;
            case VK_NEXT: result += "PAGEDOWN"; break;
            case VK_LEFT: result += "LEFT"; break;
            case VK_RIGHT: result += "RIGHT"; break;
            case VK_UP: result += "UP"; break;
            case VK_DOWN: result += "DOWN"; break;

            // Mouse buttons
            case VK_LBUTTON: result += "LEFTMOUSE"; break;
            case VK_RBUTTON: result += "RIGHTMOUSE"; break;
            case VK_MBUTTON: result += "MIDDLEMOUSE"; break;
            case VK_XBUTTON1: result += "MOUSE4"; break;
            case VK_XBUTTON2: result += "MOUSE5"; break;

            // Symbols
            case VK_OEM_PLUS: result += "="; break;
            case VK_OEM_MINUS: result += "-"; break;
            case VK_OEM_COMMA: result += ","; break;
            case VK_OEM_PERIOD: result += "."; break;
            case VK_OEM_1: result += ";"; break;
            case VK_OEM_2: result += "/"; break;
            case VK_OEM_3: result += "`"; break;
            case VK_OEM_4: result += "["; break;
            case VK_OEM_5: result += "\\"; break;
            case VK_OEM_6: result += "]"; break;
            case VK_OEM_7: result += "'"; break;

            default: {
                char keyStr[16];
                snprintf(keyStr, sizeof(keyStr), "0x%X", key);
                result += keyStr;
            }
        }

        strcpy_s(buffer, sizeof(buffer), result.c_str());
        return buffer;
    }

    // Listen for next key press (for hotkey binding UI)
    inline bool ListenForKeyPress(int& outKey, bool& outCtrl, bool& outShift, bool& outAlt) {
        // Check all regular keys
        for (int vk = 0; vk <= 255; ++vk) {
            // Skip modifier keys
            if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LMENU || vk == VK_RMENU) {
                continue;
            }

            if (GetAsyncKeyState(vk) & 0x8000) {
                outKey = vk;
                outCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                outShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                outAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                return true;
            }
        }
        return false;
    }
}
