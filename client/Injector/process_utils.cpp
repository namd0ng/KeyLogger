#include <Windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include "../Common/constants.h"

// Function pointer type for IsWow64Process2 (Win10 1709+)
typedef BOOL (WINAPI *fnIsWow64Process2)(HANDLE, USHORT*, USHORT*);

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

// Helper: get human-readable name for a machine type constant
static const char* MachineTypeName(USHORT machineType) {
    switch (machineType) {
        case IMAGE_FILE_MACHINE_AMD64:  return "x64";
        case IMAGE_FILE_MACHINE_ARM64:  return "ARM64";
        case IMAGE_FILE_MACHINE_I386:   return "x86";
        case IMAGE_FILE_MACHINE_UNKNOWN: return "Unknown";
        default:                         return "Other";
    }
}

// Validate that the injector's architecture matches the target process
// Returns true if architectures are compatible, false otherwise
bool ValidateArchitectureMatch(DWORD targetPid) {
    printf("[*] Validating architecture compatibility...\n");
    printf("[*] Injector build target: %s\n", BUILD_ARCH_NAME);

    // Determine this injector's expected machine type
    USHORT injectorMachine = IMAGE_FILE_MACHINE_UNKNOWN;
#if defined(_M_ARM64)
    injectorMachine = IMAGE_FILE_MACHINE_ARM64;
#elif defined(_M_X64) || defined(_M_AMD64)
    injectorMachine = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_IX86)
    injectorMachine = IMAGE_FILE_MACHINE_I386;
#endif

    // Try to use IsWow64Process2 (available since Win10 1709)
    // Dynamically loaded to maintain compatibility with older Windows
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    fnIsWow64Process2 pIsWow64Process2 = NULL;
    if (hKernel32) {
        pIsWow64Process2 = (fnIsWow64Process2)GetProcAddress(hKernel32, "IsWow64Process2");
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPid);
    if (hProcess == NULL) {
        printf("[-] Failed to open target process for architecture check. Error: %lu\n", GetLastError());
        return false;
    }

    USHORT targetMachine = IMAGE_FILE_MACHINE_UNKNOWN;

    if (pIsWow64Process2) {
        // Modern path: IsWow64Process2 gives exact machine types
        USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;

        if (pIsWow64Process2(hProcess, &processMachine, &nativeMachine)) {
            if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN) {
                // Not running under WOW64 — process is native
                targetMachine = nativeMachine;
            } else {
                // Running under WOW64 — processMachine is the actual type
                targetMachine = processMachine;
            }
        } else {
            printf("[-] IsWow64Process2 failed. Error: %lu\n", GetLastError());
            CloseHandle(hProcess);
            return false;
        }
    } else {
        // Fallback for older Windows: use IsWow64Process (only distinguishes 32/64)
        BOOL isWow64 = FALSE;
        if (IsWow64Process(hProcess, &isWow64)) {
            if (isWow64) {
                // 32-bit process on 64-bit OS
                targetMachine = IMAGE_FILE_MACHINE_I386;
            } else {
                // Same as OS native (could be x64 or x86 on 32-bit OS)
                SYSTEM_INFO si;
                GetNativeSystemInfo(&si);
                if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
                    targetMachine = IMAGE_FILE_MACHINE_AMD64;
                } else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
                    targetMachine = IMAGE_FILE_MACHINE_I386;
                }
            }
        } else {
            printf("[-] IsWow64Process failed. Error: %lu\n", GetLastError());
            CloseHandle(hProcess);
            return false;
        }
    }

    CloseHandle(hProcess);

    printf("[*] Target process architecture: %s\n", MachineTypeName(targetMachine));

    if (injectorMachine != targetMachine) {
        printf("[-] Architecture mismatch!\n");
        printf("[-] Injector is %s but target process is %s\n",
               MachineTypeName(injectorMachine), MachineTypeName(targetMachine));
        printf("[-] CreateRemoteThread requires matching architectures.\n");
        printf("[-] Rebuild the injector and payload DLL for %s.\n",
               MachineTypeName(targetMachine));
        return false;
    }

    printf("[+] Architecture match confirmed: %s\n", BUILD_ARCH_NAME);
    return true;
}
