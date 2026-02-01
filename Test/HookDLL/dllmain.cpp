#include <Windows.h>
#include <stdio.h>

// Global variables
static HHOOK g_hHook = NULL;
static HINSTANCE g_hInstance = NULL;
static FILE* g_logFile = NULL;

// Hook procedure - must be exported from DLL
extern "C" __declspec(dllexport) LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_KEYDOWN) {
            KBDLLHOOKSTRUCT* pKbdStruct = (KBDLLHOOKSTRUCT*)lParam;

            // Log to file for educational demonstration
            if (g_logFile) {
                fprintf(g_logFile, "[HOOK] Key pressed: VK=0x%02X\n", pKbdStruct->vkCode);
                fflush(g_logFile);
            }
        }
    }

    // Call the next hook in the chain
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hModule;

        // Open log file for educational purposes
        fopen_s(&g_logFile, "C:\\hook_log.txt", "a");
        if (g_logFile) {
            fprintf(g_logFile, "\n=== Hook DLL Loaded (Educational Test) ===\n");
            fflush(g_logFile);
        }

        // Display notification
        MessageBoxA(
            NULL,
            "Hook DLL Loaded!\n\n"
            "This DLL demonstrates SetWindowsHookEx.\n"
            "Events will be logged to C:\\hook_log.txt\n\n"
            "Educational purposes only.",
            "Hook DLL - Process Attach",
            MB_OK | MB_ICONINFORMATION
        );
        break;

    case DLL_PROCESS_DETACH:
        // Clean up
        if (g_logFile) {
            fprintf(g_logFile, "=== Hook DLL Unloaded ===\n\n");
            fclose(g_logFile);
            g_logFile = NULL;
        }

        MessageBoxA(
            NULL,
            "Hook DLL is being unloaded.",
            "Hook DLL - Process Detach",
            MB_OK | MB_ICONINFORMATION
        );
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

// Optional: Export function to set hook handle (can be used by loader)
extern "C" __declspec(dllexport) void SetHookHandle(HHOOK hHook) {
    g_hHook = hHook;
}
