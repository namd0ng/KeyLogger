#include <Windows.h>
#include <TlHelp32.h>
#include <stdio.h>

// Helper: convert narrow string to wide string for comparison
static bool NarrowToWide(const char* narrow, wchar_t* wide, size_t wideSize) {
    int result = MultiByteToWideChar(CP_ACP, 0, narrow, -1, wide, (int)wideSize);
    return result > 0;
}

// Find process ID by process name
// Returns 0 if process not found
DWORD FindProcessByName(const char* processName) {
    DWORD pid = 0;

    // Convert process name to wide string for comparison
    wchar_t wProcessName[MAX_PATH];
    if (!NarrowToWide(processName, wProcessName, MAX_PATH)) {
        printf("[-] Failed to convert process name.\n");
        return 0;
    }

    // Create snapshot of all processes
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to create process snapshot. Error: %lu\n", GetLastError());
        return 0;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    // Iterate through all processes
    if (Process32FirstW(snapshot, &entry)) {
        do {
            // Case-insensitive wide string comparison
            if (_wcsicmp(entry.szExeFile, wProcessName) == 0) {
                pid = entry.th32ProcessID;
                printf("[+] Found %s (PID: %lu)\n", processName, pid);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    if (pid == 0) {
        printf("[-] Process '%s' not found!\n", processName);
    }

    return pid;
}

// Check if a DLL is already loaded in the target process
// Returns true if DLL is already injected, false otherwise
bool IsDLLAlreadyInjected(DWORD pid, const char* dllName) {
    bool isInjected = false;

    // Convert DLL name to wide string for comparison
    wchar_t wDllName[MAX_PATH];
    if (!NarrowToWide(dllName, wDllName, MAX_PATH)) {
        printf("[-] Failed to convert DLL name.\n");
        return false;
    }

    // Create snapshot of all modules in the target process
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            printf("[-] Access denied when checking modules. Run as Administrator.\n");
        } else {
            printf("[-] Failed to create module snapshot. Error: %lu\n", error);
        }
        return false;
    }

    MODULEENTRY32W me32;
    me32.dwSize = sizeof(MODULEENTRY32W);

    // Iterate through all modules
    if (Module32FirstW(snapshot, &me32)) {
        do {
            // Case-insensitive wide string comparison
            if (_wcsicmp(me32.szModule, wDllName) == 0) {
                printf("[*] DLL '%s' is already loaded in target process\n", dllName);
                isInjected = true;
                break;
            }
        } while (Module32NextW(snapshot, &me32));
    }

    CloseHandle(snapshot);
    return isInjected;
}
