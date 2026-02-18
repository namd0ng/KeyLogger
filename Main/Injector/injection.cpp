#include <Windows.h>
#include <stdio.h>

// Inject DLL into target process using CreateRemoteThread technique
// Returns true on success, false on failure
bool InjectDLL(DWORD pid, const char* dllPath) {
    printf("\n[*] Starting DLL injection into PID: %lu\n", pid);
    printf("[*] DLL Path: %s\n", dllPath);

    // Step 1: Open target process with full access
    printf("[*] Step 1: Opening target process...\n");
    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        pid
    );

    if (hProcess == NULL) {
        DWORD error = GetLastError();
        printf("[-] Failed to open process. Error: %lu\n", error);
        if (error == ERROR_ACCESS_DENIED) {
            printf("[-] Access denied! Run as Administrator.\n");
        }
        return false;
    }
    printf("[+] Process opened successfully. Handle: 0x%p\n", hProcess);

    // Step 2: Allocate memory in target process for DLL path
    printf("[*] Step 2: Allocating memory in target process...\n");
    SIZE_T dllPathSize = strlen(dllPath) + 1;
    LPVOID remoteMem = VirtualAllocEx(
        hProcess,
        NULL,
        dllPathSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (remoteMem == NULL) {
        printf("[-] Failed to allocate memory in target process. Error: %lu\n", GetLastError());
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] Memory allocated at: 0x%p (Size: %zu bytes)\n", remoteMem, dllPathSize);

    // Step 3: Write DLL path to allocated memory
    printf("[*] Step 3: Writing DLL path to target process memory...\n");
    SIZE_T bytesWritten = 0;
    BOOL writeResult = WriteProcessMemory(
        hProcess,
        remoteMem,
        dllPath,
        dllPathSize,
        &bytesWritten
    );

    if (!writeResult || bytesWritten != dllPathSize) {
        printf("[-] Failed to write DLL path to target process. Error: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] DLL path written successfully (%zu bytes)\n", bytesWritten);

    // Step 4: Get address of LoadLibraryA from kernel32.dll
    printf("[*] Step 4: Getting LoadLibraryA address...\n");
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32 == NULL) {
        printf("[-] Failed to get kernel32.dll handle. Error: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");
    if (loadLibraryAddr == NULL) {
        printf("[-] Failed to get LoadLibraryA address. Error: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] LoadLibraryA address: 0x%p\n", loadLibraryAddr);

    // Step 5: Create remote thread to execute LoadLibraryA with DLL path
    printf("[*] Step 5: Creating remote thread to load DLL...\n");
    HANDLE hThread = CreateRemoteThread(
        hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)loadLibraryAddr,
        remoteMem,
        0,
        NULL
    );

    if (hThread == NULL) {
        printf("[-] Failed to create remote thread. Error: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    printf("[+] Remote thread created successfully. Handle: 0x%p\n", hThread);

    // Wait for the remote thread to complete (LoadLibrary to finish)
    printf("[*] Waiting for DLL to load...\n");
    DWORD waitResult = WaitForSingleObject(hThread, 5000);  // 5 second timeout
    if (waitResult == WAIT_TIMEOUT) {
        printf("[!] Warning: Thread did not complete within 5 seconds\n");
    } else if (waitResult == WAIT_OBJECT_0) {
        printf("[+] DLL load thread completed\n");
    }

    // Check thread exit code (will be the HMODULE of loaded DLL if successful)
    DWORD exitCode = 0;
    if (GetExitCodeThread(hThread, &exitCode)) {
        if (exitCode != 0) {
            printf("[+] DLL loaded successfully! Module handle: 0x%lX\n", exitCode);
        } else {
            printf("[-] DLL load failed! LoadLibrary returned NULL\n");
            CloseHandle(hThread);
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
    }

    // Cleanup
    printf("[*] Cleaning up...\n");
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    printf("[+] DLL injection completed successfully!\n\n");
    return true;
}
