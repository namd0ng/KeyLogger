#include <Windows.h>
#include <TlHelp32.h>
#include <stdio.h>

// Find process ID by process name
// Returns 0 if process not found
DWORD FindProcessByName(const char* processName) {
    DWORD pid = 0;

    // Create snapshot of all processes
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to create process snapshot. Error: %lu\n", GetLastError());
        return 0;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    // Iterate through all processes
    if (Process32First(snapshot, &entry)) {
        do {
            // Case-insensitive comparison
            if (_stricmp(entry.szExeFile, processName) == 0) {
                pid = entry.th32ProcessID;
                printf("[+] Found %s (PID: %lu)\n", processName, pid);
                break;
            }
        } while (Process32Next(snapshot, &entry));
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

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    // Iterate through all modules
    if (Module32First(snapshot, &me32)) {
        do {
            // Case-insensitive comparison
            if (_stricmp(me32.szModule, dllName) == 0) {
                printf("[*] DLL '%s' is already loaded in target process\n", dllName);
                isInjected = true;
                break;
            }
        } while (Module32Next(snapshot, &me32));
    }

    CloseHandle(snapshot);
    return isInjected;
}
