#include <Windows.h>
#include <stdio.h>
#include <string>

// Type definition for the hook procedure from DLL
typedef LRESULT(CALLBACK* HOOKPROC_TYPE)(int, WPARAM, LPARAM);
typedef void (*SetHookHandle_TYPE)(HHOOK);

// Function to get the directory of the current executable
std::string GetExeDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    return exeDir;
}

// Function to load DLL from Visual Studio build structure
HMODULE LoadHookDLL(const std::string& baseDir) {
    // HookLoader.exe : Test/HookLoader/x64/Debug(or Release)/
    // HookDLL.dll : Test/HookDLL/x64/Debug(or Release)/
    const char* relativePaths[] = {
        "..\\..\\HookDLL\\x64\\Debug\\HookDLL.dll",    // Debug
        "..\\..\\HookDLL\\x64\\Release\\HookDLL.dll"   // Release
    };

    const char* buildConfigs[] = { "Debug", "Release" };

    for (int i = 0; i < 2; i++) {
        std::string fullPath = baseDir + "\\" + relativePaths[i];
        printf("[*] Trying %s build: %s\n", buildConfigs[i], relativePaths[i]);

        HMODULE hDll = LoadLibraryA(fullPath.c_str());
        if (hDll) {
            printf("[+] DLL loaded from %s build!\n", buildConfigs[i]);
            printf("[+] Full path: %s\n", fullPath.c_str());
            return hDll;
        }
    }
    return NULL;
}

int main() {
    printf("======================================\n");
    printf("  Hook-Based DLL Injection Test\n");
    printf("======================================\n\n");

    // Display build configuration
#ifdef _DEBUG
    printf("[*] Build Configuration: Debug\n");
#else
    printf("[*] Build Configuration: Release\n");
#endif

    // Get executable directory
    std::string exeDir = GetExeDirectory();
    printf("[*] Executable directory: %s\n\n", exeDir.c_str());

    // Step 1: Load the DLL from Visual Studio build structure
    printf("[*] Loading HookDLL.dll from Visual Studio build structure...\n");
    HMODULE hDll = LoadHookDLL(exeDir);
    if (!hDll) {
        printf("\n[-] Failed to load HookDLL.dll. Error: %lu\n", GetLastError());
        printf("[-] Expected locations:\n");
        printf("    - ..\\..\\HookDLL\\x64\\Debug\\HookDLL.dll\n");
        printf("    - ..\\..\\HookDLL\\x64\\Release\\HookDLL.dll\n\n");
        printf("[-] Please build HookDLL project in Visual Studio first.\n");
        return 1;
    }
    printf("[+] DLL loaded successfully! Handle: 0x%p\n\n", hDll);

    // Step 2: Get the address of the hook procedure
    printf("[*] Getting hook procedure address...\n");
    HOOKPROC_TYPE hookProc = (HOOKPROC_TYPE)GetProcAddress(hDll, "KeyboardProc");
    if (!hookProc) {
        printf("[-] Failed to get KeyboardProc address. Error: %lu\n", GetLastError());
        FreeLibrary(hDll);
        return 1;
    }
    printf("[+] Hook procedure found at: 0x%p\n", hookProc);

    // Step 3: Install the hook using SetWindowsHookEx
    printf("[*] Installing keyboard hook (WH_KEYBOARD_LL)...\n");
    HHOOK hHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,      // Low-level keyboard hook
        hookProc,            // Hook procedure from DLL
        hDll,                // DLL handle (important for global hooks)
        0                    // 0 = system-wide hook
    );

    if (!hHook) {
        printf("[-] Failed to set hook. Error: %lu\n", GetLastError());
        FreeLibrary(hDll);
        return 1;
    }
    printf("[+] Hook installed successfully! Handle: 0x%p\n", hHook);

    // Optional: Pass hook handle to DLL
    SetHookHandle_TYPE setHookHandle = (SetHookHandle_TYPE)GetProcAddress(hDll, "SetHookHandle");
    if (setHookHandle) {
        setHookHandle(hHook);
        printf("[+] Hook handle passed to DLL\n");
    }

    printf("\n[*] Hook is now active (system-wide)\n");
    printf("[*] Events will be logged to C:\\hook_log.txt\n");
    printf("[*] Press CTRL+C or close this window to unhook and exit\n\n");

    // Step 4: Message loop (required for hooks to work)
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Step 5: Cleanup
    printf("\n[*] Cleaning up...\n");

    if (!UnhookWindowsHookEx(hHook)) {
        printf("[-] Failed to unhook. Error: %lu\n", GetLastError());
    } else {
        printf("[+] Hook uninstalled successfully\n");
    }

    if (!FreeLibrary(hDll)) {
        printf("[-] Failed to free library. Error: %lu\n", GetLastError());
    } else {
        printf("[+] DLL unloaded successfully\n");
    }

    printf("\n[+] Program terminated.\n");
    return 0;
}
