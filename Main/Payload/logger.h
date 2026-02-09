#pragma once

// Initialize the logger
// Opens the log file and initializes critical section for thread safety
bool InitLogger();

// Cleanup the logger
// Closes the log file and deletes critical section
void CleanupLogger();

// Log a key event
// Handles both printable characters and special keys
void LogKey(int vkCode);

// Check if logger is initialized and ready
bool IsLoggerReady();

// Get the log file path (for display purposes)
const char* GetLogFilePath();
