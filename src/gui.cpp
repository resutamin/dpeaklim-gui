// gui.cpp
// Disable Peak Limiter GUI  ―  GUI front-end for dpeaklim (author: Suguru Kawamoto, MIT).
//
// The physical-memory patch core is the author's original (dpeaklim.cpp / InpOut32.cpp),
// unchanged. This file adds the reliability layer that fixes the "sometimes it does
// nothing" problem, plus a bilingual UI (English default, switchable to Japanese):
//   (1) auto-elevate to admin (manifest)     (2) pre-flight diagnostics
//   (3) play silence to force audioeng.dll resident in physical RAM
//   (4) wait until resident before patching  (5) automatic retry
//   (6) clear success/failure reporting      (7) one-click restore
//
#define _WIN32_WINNT 0x0A00
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <float.h>
#include <initializer_list>
#include "core.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

// ---- control IDs ----
#define IDC_HEADER     1000
#define IDC_INFO       1001
#define IDC_LANG       1002
#define IDC_LANGLBL    1003
#define IDC_GRP_STATUS 1004
#define IDC_GRP_THRESH 1005
#define IDC_LBL_DB     1006
#define IDC_LOGLBL     1007
#define IDC_RAD_FULL   1010
#define IDC_RAD_0DB    1011
#define IDC_RAD_CUSTOM 1012
#define IDC_EDIT_DB    1013
#define IDC_BTN_DISABLE 1020
#define IDC_BTN_RESTORE 1021
#define IDC_BTN_VERIFY  1022
#define IDC_LOG        1030
#define IDC_STATUS     1031
#define IDC_STATE      1040
#define IDT_STATE      1            // timer id for live state refresh

#define WM_APP_LOG  (WM_APP + 1)   // lParam = wchar_t* (append then free)
#define WM_APP_DONE (WM_APP + 2)   // wParam = BOOL ok
#define WM_APP_TRAY      (WM_APP + 3)   // tray icon callback
#define WM_APP_AUTOSTART (WM_APP + 4)   // auto-apply at startup
#define WM_APP_VERIFY_DONE (WM_APP + 5) // wParam: 0=dead,1=live,2=no target

#define IDC_GRP_OPT     1050
#define IDC_CHK_TRAY    1051
#define IDC_CHK_STARTUP 1052
#define IDC_CHK_FORCE   1053
#define IDM_TRAY_SHOW    1060
#define IDM_TRAY_DISABLE 1061
#define IDM_TRAY_RESTORE 1062
#define IDM_TRAY_EXIT    1063
#define TRAY_UID 1

// ============================ localization ============================
struct Strings
{
    const wchar_t* header;
    const wchar_t* langLabel;
    const wchar_t* grpStatus;
    const wchar_t* grpThreshold;
    const wchar_t* radFull;
    const wchar_t* rad0db;
    const wchar_t* radCustom;
    const wchar_t* dbUnit;
    const wchar_t* btnDisable;
    const wchar_t* btnRestore;
    const wchar_t* btnVerify;
    const wchar_t* logLabel;
    const wchar_t* statusReady;
    const wchar_t* statusBusy;
    const wchar_t* msgDone;
    const wchar_t* msgFail;
    const wchar_t* dlgTitle;
    const wchar_t* inErrTitle;
    const wchar_t* inErrBadNum;
    const wchar_t* inErrRange;
    const wchar_t* busyClose;
    // preflight
    const wchar_t* infoFmt;          // 5 x %s : os, admin, hvci, c985, driver
    const wchar_t* yesAdmin;
    const wchar_t* noAdmin;
    const wchar_t* hvciOn;
    const wchar_t* hvciOff;
    const wchar_t* c985Err;
    const wchar_t* c985None;
    const wchar_t* c985Yes;
    const wchar_t* drvYes;
    const wchar_t* drvNo;
    // log
    const wchar_t* opStartDisable;
    const wchar_t* opStartRestore;
    const wchar_t* loadingDriver;
    const wchar_t* driverFail1;
    const wchar_t* driverFail2;
    const wchar_t* driverOk;
    const wchar_t* attempt;
    const wchar_t* restarting;
    const wchar_t* priming;
    const wchar_t* generating;
    const wchar_t* scanning;
    const wchar_t* applyRestart;
    const wchar_t* applyLive;
    const wchar_t* retry;
    const wchar_t* doneDisable;
    const wchar_t* doneRestore;
    const wchar_t* failFound;
    const wchar_t* failHint1;
    const wchar_t* failHint2;
    const wchar_t* rebootNote;
    const wchar_t* stateDisabled;
    const wchar_t* stateNormal;
    const wchar_t* grpOptions;
    const wchar_t* chkTray;
    const wchar_t* chkStartup;
    const wchar_t* chkForce;
    const wchar_t* trayShow;
    const wchar_t* trayDisable;
    const wchar_t* trayRestore;
    const wchar_t* trayExit;
    const wchar_t* acWarnTitle;
    const wchar_t* acWarnBody;
    const wchar_t* verifyTitle;
    const wchar_t* verifyLive;
    const wchar_t* verifyDead;
    const wchar_t* verifyNoTarget;
    const wchar_t* verifyStart;
};

static const Strings S_EN =
{
    L"Disable Peak Limiter  (Windows audio)",
    L"Language:",
    L"Status (pre-flight checks)",
    L"Threshold",
    L"Force completely off (∞) — may cause audio glitches on some systems",
    L"Disable — limiter acts only above 0 dBFS (recommended)",
    L"Custom:",
    L"dB",
    L"Disable limiter",
    L"Restore",
    L"Verify",
    L"Log",
    L"Ready",
    L"Working… please wait (audio will glitch briefly)",
    L"Done.",
    L"Failed. Please check the log.",
    L"dpeaklim GUI",
    L"Input error",
    L"The dB value is not valid.",
    L"The dB value is out of range.",
    L"Processing. Please wait until it finishes.",
    L"%s\r\nAdministrator: %s\r\nMemory Integrity (Core Isolation / HVCI): %s\r\nPeak-limiter threshold 0.985: %s\r\nDriver inpoutx64.dll: %s",
    L"available ✓",
    L"missing ✗ (run as administrator)",
    L"ON ✗ (writes are blocked until this is turned off)",
    L"OFF ✓",
    L"cannot open audioeng.dll",
    L"not found ✗ (may be unsupported on this build)",
    L"found ✓",
    L"present ✓",
    L"missing ✗ (inpoutx64.dll must be in bin)",
    L"── Starting: disable ──",
    L"── Starting: restore ──",
    L"Loading driver (inpoutx64.dll)...",
    L"✗ Failed: could not open the driver.",
    L"  It may be blocked by Secure Boot or driver-signature enforcement.",
    L"✓ Driver ready",
    L"Attempt %d/%d",
    L"  Restarting Windows Audio...",
    L"  Playing silence to make audioeng.dll resident...",
    L"  Generating patch from the on-disk audioeng.dll...",
    L"  Scanning physical memory and patching... (candidates %lu, may take tens of seconds)",
    L"  Restarting audio to apply...",
    L"  ✓ Kept live on the running audio engine (no final restart).",
    L"  → Target page was not resident this time. Retrying.",
    L"✓ Done: peak limiter disabled.",
    L"✓ Done: peak limiter restored.",
    L"✗ Failed: target page not found.",
    L"  ・ If 'Core Isolation > Memory Integrity' is ON, writes are blocked.",
    L"  ・ Play a song and run again — it tends to succeed then.",
    L"* Settings revert after a Windows restart (by design).",
    L"●  Peak limiter: patch applied (this session)",
    L"●  Peak limiter: not applied",
    L"Options",
    L"Minimize to tray (keep running in the background)",
    L"Run at login (delayed ~1 min, applies automatically)",
    L"Force mode — force off even the 0 dBFS limit (experimental)",
    L"Show",
    L"Disable limiter",
    L"Restore",
    L"Exit",
    L"Anti-cheat game is running",
    L"An anti-cheat game appears to be running:\n\n        %s\n\nApplying now loads a kernel driver and the anti-cheat may close the game.\nClose the game first, or apply before launching it.\n\nApply anyway?",
    L"Verify result",
    L"✓ LIVE — the limiter patch is present in memory right now.",
    L"✗ NOT live — the patch is gone (engine reloaded / reverted).",
    L"Cannot verify — this engine version has no matching target (it changed).",
    L"Verifying live memory (reading back)...",
};

static const Strings S_JA =
{
    L"Disable Peak Limiter — Windows オーディオのピークリミッタ無効化",
    L"言語:",
    L"状態（事前診断）",
    L"しきい値",
    L"完全に強制オフ（∞）— 環境により音が途切れる場合あり",
    L"無効化 — 0dBFS を超えた時だけ作動（推奨）",
    L"任意:",
    L"dB",
    L"リミッターを無効化",
    L"元に戻す（復元）",
    L"検証",
    L"ログ",
    L"準備完了",
    L"処理中… 完了するまでお待ちください（音が一瞬途切れます）",
    L"完了しました。",
    L"失敗しました。ログを確認してください。",
    L"dpeaklim GUI",
    L"入力エラー",
    L"dB値が正しくありません。",
    L"dB値が範囲外です。",
    L"処理中です。完了までお待ちください。",
    L"%s\r\n管理者権限: %s\r\nメモリ整合性(コア分離/HVCI): %s\r\nピークリミッタ閾値 0.985: %s\r\nドライバ inpoutx64.dll: %s",
    L"あり ✓",
    L"なし ✗（管理者で実行してください）",
    L"オン ✗（オフにしないと書き換えがブロックされます）",
    L"オフ ✓",
    L"audioeng.dll を開けません",
    L"見つからない ✗（この版では非対応の可能性）",
    L"あり ✓",
    L"あり ✓",
    L"なし ✗（bin に inpoutx64.dll が必要）",
    L"── 無効化を開始 ──",
    L"── 復元を開始 ──",
    L"ドライバ(inpoutx64.dll)を読み込み中...",
    L"✗ 失敗: ドライバを開けませんでした。",
    L"  Secure Boot やドライバ署名の制限でブロックされている可能性があります。",
    L"✓ ドライバ準備完了",
    L"試行 %d/%d",
    L"  Windows オーディオを再起動中...",
    L"  無音を再生して audioeng.dll を常駐化中...",
    L"  ディスクのaudioeng.dllからパッチを生成中...",
    L"  物理メモリを走査して該当ページを書き換え中...(候補 %lu / 数十秒かかる場合あり)",
    L"  反映のためオーディオを再起動中...",
    L"  ✓ 稼働中のオーディオエンジンに直接適用して保持（最終再起動なし）",
    L"  → 今回は対象ページが常駐していませんでした。リトライします。",
    L"✓ 完了: ピークリミッタを無効化しました。",
    L"✓ 完了: ピークリミッタを元に戻しました。",
    L"✗ 失敗: 対象ページが見つかりませんでした。",
    L"  ・「コア分離 > メモリ整合性」がオンだと書き換えがブロックされます。",
    L"  ・ 音楽を1曲再生したまま、もう一度実行すると成功しやすくなります。",
    L"※ Windows を再起動すると設定は元に戻ります（仕様）。",
    L"●  ピークリミッタ: パッチ適用済み（本セッション）",
    L"●  ピークリミッタ: 未適用",
    L"オプション",
    L"トレイに収納（バックグラウンドで常駐）",
    L"ログイン時に自動実行（約1分遅延・自動適用）",
    L"Forceモード — 0dBFS上限も強制オフ（実験的・このビルド専用）",
    L"表示",
    L"無効化",
    L"元に戻す",
    L"終了",
    L"アンチチート系ゲームが起動中",
    L"アンチチート系ゲームが起動中のようです:\n\n        %s\n\nここで適用するとカーネルドライバを読み込むため、アンチチートがゲームを終了させる場合があります。\n先にゲームを閉じるか、起動前に適用してください。\n\nこのまま適用しますか？",
    L"検証結果",
    L"✓ 生きています — リミッタのパッチが今メモリ上に存在します。",
    L"✗ 効いていません — パッチは消えています（エンジン再読込/復帰）。",
    L"検証不可 — 現在のエンジン版に一致する標的がありません（版が変わった）。",
    L"実メモリを読み戻して検証中...",
};

static const Strings* g_S = &S_EN;   // default = English

// ============================ globals ============================
static HWND   g_hMain = NULL;
static HWND   g_hLog = NULL;
static HWND   g_hStatus = NULL;
static HFONT  g_hFont = NULL;
static HBRUSH g_brGreen = NULL;
static HBRUSH g_brRed = NULL;
static bool   g_disabled = false;          // current state shown by the indicator
static volatile LONG g_busy = 0;
static bool   g_minToTray = false;         // minimize-to-tray mode
static bool   g_trayShown = false;
static bool   g_autoStart = false;         // launched with /startup
static NOTIFYICONDATAW g_nid;

// ============================ silence priming ============================
static volatile bool g_prime = false;
static HANDLE        g_primeThread = NULL;

static DWORD WINAPI PrimeProc(LPVOID)
{
    WAVEFORMATEX wf;
    ZeroMemory(&wf, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = 48000;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    HWAVEOUT h = NULL;
    if (waveOutOpen(&h, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return 0;

    static const int FRAMES = 4800;          // 100 ms
    static short buf[FRAMES * 2];
    ZeroMemory(buf, sizeof(buf));

    const int NB = 4;
    WAVEHDR hdr[NB];
    ZeroMemory(hdr, sizeof(hdr));
    for (int i = 0; i < NB; i++)
    {
        hdr[i].lpData = (LPSTR)buf;
        hdr[i].dwBufferLength = sizeof(buf);
        waveOutPrepareHeader(h, &hdr[i], sizeof(WAVEHDR));
        waveOutWrite(h, &hdr[i], sizeof(WAVEHDR));
    }
    while (g_prime)
    {
        for (int i = 0; i < NB; i++)
            if (hdr[i].dwFlags & WHDR_DONE)
                waveOutWrite(h, &hdr[i], sizeof(WAVEHDR));
        Sleep(15);
    }
    waveOutReset(h);
    for (int i = 0; i < NB; i++)
        waveOutUnprepareHeader(h, &hdr[i], sizeof(WAVEHDR));
    waveOutClose(h);
    return 0;
}
static void StartSilence()
{
    if (g_primeThread) return;
    g_prime = true;
    g_primeThread = CreateThread(NULL, 0, PrimeProc, NULL, 0, NULL);
}
static void StopSilence()
{
    if (!g_primeThread) return;
    g_prime = false;
    WaitForSingleObject(g_primeThread, 3000);
    CloseHandle(g_primeThread);
    g_primeThread = NULL;
}

// ============================ log ============================
static void Log(const wchar_t* fmt, ...)
{
    wchar_t buf[1024];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    PostMessageW(g_hMain, WM_APP_LOG, 0, (LPARAM)_wcsdup(buf));
}
static void AppendLog(const wchar_t* text)
{
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    SendMessageW(g_hLog, WM_VSCROLL, SB_BOTTOM, 0);
}

// ============================ pre-flight data ============================
static bool IsRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID grp = NULL;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &grp))
    {
        CheckTokenMembership(NULL, grp, &isAdmin);
        FreeSid(grp);
    }
    return isAdmin != FALSE;
}
static DWORD MemoryIntegrityEnabled()
{
    DWORD val = 0, sz = sizeof(val);
    if (RegGetValueW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
            L"Enabled", RRF_RT_REG_DWORD, NULL, &val, &sz) == ERROR_SUCCESS)
        return val;
    return 0;
}
static int Count0985InAudioEng()
{
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, MAX_PATH, L"\\audioeng.dll");
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return -1;
    DWORD size = GetFileSize(hf, NULL);
    BYTE* p = (BYTE*)malloc(size);
    int count = -1;
    if (p)
    {
        DWORD rd = 0;
        if (ReadFile(hf, p, size, &rd, NULL))
        {
            float f = 0.985f;
            BYTE* pat = (BYTE*)&f;
            count = 0;
            for (DWORD i = 0; i + 4 <= rd; i++)
                if (p[i] == pat[0] && p[i+1] == pat[1] && p[i+2] == pat[2] && p[i+3] == pat[3])
                    count++;
        }
        free(p);
    }
    CloseHandle(hf);
    return count;
}
static bool DriverPresent()
{
    wchar_t dir[MAX_PATH];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat_s(dir, MAX_PATH, L"inpoutx64.dll");
    return GetFileAttributesW(dir) != INVALID_FILE_ATTRIBUTES;
}
static void GetOSBuildText(wchar_t* out, size_t n)
{
    wchar_t disp[64] = L"", build[32] = L"";
    DWORD ubr = 0, sz;
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ, &k) == ERROR_SUCCESS)
    {
        sz = sizeof(disp);  RegQueryValueExW(k, L"DisplayVersion", NULL, NULL, (BYTE*)disp, &sz);
        sz = sizeof(build); RegQueryValueExW(k, L"CurrentBuild", NULL, NULL, (BYTE*)build, &sz);
        sz = sizeof(ubr);   RegQueryValueExW(k, L"UBR", NULL, NULL, (BYTE*)&ubr, &sz);
        RegCloseKey(k);
    }
    _snwprintf_s(out, n, _TRUNCATE, L"Windows %s (build %s.%lu)", disp, build, ubr);
}
static void BuildPreflight(wchar_t* out, size_t n)
{
    wchar_t os[128]; GetOSBuildText(os, _countof(os));
    bool admin = IsRunningAsAdmin();
    DWORD hvci = MemoryIntegrityEnabled();
    int  c985 = Count0985InAudioEng();
    bool drv = DriverPresent();
    _snwprintf_s(out, n, _TRUNCATE, g_S->infoFmt,
        os,
        admin ? g_S->yesAdmin : g_S->noAdmin,
        hvci ? g_S->hvciOn : g_S->hvciOff,
        (c985 < 0) ? g_S->c985Err : (c985 == 0 ? g_S->c985None : g_S->c985Yes),
        drv ? g_S->drvYes : g_S->drvNo);
}

// ============================ persistence (for restore) ============================
static void SaveLastOverriding(float over)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dpeaklim_gui", 0, NULL, 0,
            KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS)
    {
        DWORD bits; memcpy(&bits, &over, 4);
        RegSetValueExW(k, L"Overriding", 0, REG_DWORD, (BYTE*)&bits, sizeof(bits));
        RegCloseKey(k);
    }
}
static float LoadLastOverriding()
{
    float f = 1.0f;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dpeaklim_gui", 0, KEY_READ, &k) == ERROR_SUCCESS)
    {
        DWORD bits = 0, sz = sizeof(bits), type = 0;
        if (RegQueryValueExW(k, L"Overriding", NULL, &type, (BYTE*)&bits, &sz) == ERROR_SUCCESS
            && type == REG_DWORD)
            memcpy(&f, &bits, 4);
        RegCloseKey(k);
    }
    return f;
}

// 起動(ブート)時刻。KeBootTime はスリープ復帰の影響を受けず同一ブート中は不変なので、
// これが記録値と一致すれば「再起動されていない＝パッチはまだ生きている」と判定できる。
typedef LONG DP_NTSTATUS;
typedef struct _DP_SYS_TOD {
    LARGE_INTEGER BootTime, CurrentTime, TimeZoneBias;
    ULONG TimeZoneId, Reserved;
    ULONGLONG BootTimeBias, SleepTimeBias;
} DP_SYS_TOD;
static ULONGLONG GetBootTime()
{
    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    typedef DP_NTSTATUS (NTAPI *PFN)(ULONG, PVOID, ULONG, PULONG);
    PFN f = h ? (PFN)GetProcAddress(h, "NtQuerySystemInformation") : NULL;
    if (f)
    {
        DP_SYS_TOD t; ZeroMemory(&t, sizeof(t)); ULONG rl = 0;
        if (f(0x03 /*SystemTimeOfDayInformation*/, &t, sizeof(t), &rl) == 0)
            return (ULONGLONG)t.BootTime.QuadPart;
    }
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER n; n.LowPart = ft.dwLowDateTime; n.HighPart = ft.dwHighDateTime;
    return n.QuadPart - (ULONGLONG)GetTickCount64() * 10000ULL;
}
static void SaveApplied(bool applied, float over)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dpeaklim_gui", 0, NULL, 0,
            KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS)
    {
        DWORD a = applied ? 1 : 0;
        RegSetValueExW(k, L"Applied", 0, REG_DWORD, (BYTE*)&a, sizeof(a));
        if (applied)
        {
            ULONGLONG bt = GetBootTime();
            RegSetValueExW(k, L"BootTime", 0, REG_QWORD, (BYTE*)&bt, sizeof(bt));
            DWORD bits; memcpy(&bits, &over, 4);
            RegSetValueExW(k, L"Overriding", 0, REG_DWORD, (BYTE*)&bits, sizeof(bits));
        }
        RegCloseKey(k);
    }
}
// 現在ピークリミッタが無効化中か（＝適用済みフラグが立ち、かつその後再起動していない）
static bool IsCurrentlyDisabled()
{
    bool res = false;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dpeaklim_gui", 0, KEY_READ | KEY_WRITE, &k) == ERROR_SUCCESS)
    {
        DWORD a = 0, sz = sizeof(a), type = 0;
        if (RegQueryValueExW(k, L"Applied", NULL, &type, (BYTE*)&a, &sz) == ERROR_SUCCESS
            && type == REG_DWORD && a == 1)
        {
            ULONGLONG bt = 0; sz = sizeof(bt);
            if (RegQueryValueExW(k, L"BootTime", NULL, &type, (BYTE*)&bt, &sz) == ERROR_SUCCESS
                && type == REG_QWORD && bt == GetBootTime())
                res = true;
            if (!res) { DWORD z = 0; RegSetValueExW(k, L"Applied", 0, REG_DWORD, (BYTE*)&z, sizeof(z)); }
        }
        RegCloseKey(k);
    }
    return res;
}

// ============================ settings (bool) ============================
static DWORD RegGetDword(const wchar_t* name, DWORD def)
{
    DWORD v = def, sz = sizeof(v), ty = 0; HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dpeaklim_gui", 0, KEY_READ, &k) == ERROR_SUCCESS)
    {
        if (!(RegQueryValueExW(k, name, NULL, &ty, (BYTE*)&v, &sz) == ERROR_SUCCESS && ty == REG_DWORD))
            v = def;
        RegCloseKey(k);
    }
    return v;
}
static void RegSetDword(const wchar_t* name, DWORD v)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dpeaklim_gui", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS)
    { RegSetValueExW(k, name, 0, REG_DWORD, (BYTE*)&v, sizeof(v)); RegCloseKey(k); }
}

// ============================ anti-cheat detection ============================
// 既知のアンチチート/保護付きゲームのプロセスが動いていれば true（name にプロセス名）
static bool AntiCheatRunning(wchar_t* name, size_t n)
{
    static const wchar_t* procs[] = {
        L"EasyAntiCheat.exe", L"EasyAntiCheat_EOS.exe", L"BEService.exe",
        L"vgc.exe", L"vgtray.exe", L"VALORANT.exe", L"VRChat.exe",
        L"FACEIT.exe", L"faceitclient.exe", L"RiotClientServices.exe"
    };
    bool found = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe))
        {
            do {
                for (size_t i = 0; i < _countof(procs) && !found; i++)
                    if (_wcsicmp(pe.szExeFile, procs[i]) == 0)
                    { wcscpy_s(name, n, pe.szExeFile); found = true; }
            } while (!found && Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    return found;
}

// ============================ tray ============================
static void AddTray(HWND hwnd)
{
    if (g_trayShown) return;
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_UID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAY;
    g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), g_disabled ? g_S->stateDisabled : g_S->stateNormal);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_trayShown = true;
}
static void RemoveTray()
{
    if (!g_trayShown) return;
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    g_trayShown = false;
}
static void UpdateTrayTip()
{
    if (!g_trayShown) return;
    g_nid.uFlags = NIF_TIP;
    wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), g_disabled ? g_S->stateDisabled : g_S->stateNormal);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// ============================ startup (scheduled task, delayed logon, elevated) ============================
static const wchar_t* TASK_NAME = L"dpeaklimGuiAutostart";
static bool RunHidden(wchar_t* cmdline, DWORD* exitCode)
{
    STARTUPINFOW si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD ec = 1; GetExitCodeProcess(pi.hProcess, &ec);
    if (exitCode) *exitCode = ec;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return true;
}
static bool IsStartupEnabled()
{
    wchar_t cmd[512];
    swprintf_s(cmd, _countof(cmd), L"schtasks /Query /TN \"%s\"", TASK_NAME);
    DWORD ec = 1;
    return RunHidden(cmd, &ec) && ec == 0;
}
static bool SetStartup(bool enable)
{
    if (!enable)
    {
        wchar_t cmd[512];
        swprintf_s(cmd, _countof(cmd), L"schtasks /Delete /TN \"%s\" /F", TASK_NAME);
        DWORD ec = 1; RunHidden(cmd, &ec);
        return true;
    }
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
    wchar_t xml[4096];
    swprintf_s(xml, _countof(xml),
        L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
        L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
        L"  <RegistrationInfo><Description>Disable Peak Limiter autostart</Description></RegistrationInfo>\r\n"
        L"  <Triggers><LogonTrigger><Enabled>true</Enabled><Delay>PT1M</Delay></LogonTrigger></Triggers>\r\n"
        L"  <Principals><Principal id=\"Author\"><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>\r\n"
        L"  <Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
        L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
        L"<AllowHardTerminate>true</AllowHardTerminate><StartWhenAvailable>true</StartWhenAvailable>"
        L"<RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>"
        L"<IdleSettings><StopOnIdleEnd>false</StopOnIdleEnd><RestartOnIdle>false</RestartOnIdle></IdleSettings>"
        L"<AllowStartOnDemand>true</AllowStartOnDemand><Enabled>true</Enabled><Hidden>false</Hidden>"
        L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Priority>7</Priority></Settings>\r\n"
        L"  <Actions Context=\"Author\"><Exec><Command>%s</Command><Arguments>/startup</Arguments></Exec></Actions>\r\n"
        L"</Task>\r\n", exe);
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    wcscat_s(tmp, MAX_PATH, L"dpeaklim_task.xml");
    HANDLE hf = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return false;
    WORD bom = 0xFEFF; DWORD wr = 0;
    WriteFile(hf, &bom, 2, &wr, NULL);
    WriteFile(hf, xml, (DWORD)(wcslen(xml) * sizeof(wchar_t)), &wr, NULL);
    CloseHandle(hf);
    wchar_t cmd[1024];
    swprintf_s(cmd, _countof(cmd), L"schtasks /Create /TN \"%s\" /XML \"%s\" /F", TASK_NAME, tmp);
    DWORD ec = 1; bool ran = RunHidden(cmd, &ec);
    DeleteFileW(tmp);
    return ran && ec == 0;
}

// ============================ operation (worker thread) ============================
// 適用後に inpoutx64 カーネルドライバを停止＆登録解除する。メモリ上のパッチには影響しない。
// 脆弱ドライバを常駐させない（＝VRChat/EAC 等の起動時に弾かれないようにする）衛生措置。
static void UnloadInpOutDriver()
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    SC_HANDLE svc = OpenServiceW(scm, L"inpoutx64", SERVICE_STOP | DELETE);
    if (svc)
    {
        SERVICE_STATUS st;
        ControlService(svc, SERVICE_CONTROL_STOP, &st);   // カーネルドライバをアンロード
        DeleteService(svc);                               // サービス登録を解除（自動ロード防止）
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

struct OpParam { float from; float to; BOOL restore; BOOL force; };

static DWORD WINAPI OpThread(LPVOID arg)
{
    OpParam p = *(OpParam*)arg;
    free(arg);
    const int MAX_ATTEMPTS = 3;
    BOOL ok = FALSE;

    Log(p.restore ? g_S->opStartRestore : g_S->opStartDisable);
    Log(g_S->loadingDriver);
    if (!Load())
    {
        Log(g_S->driverFail1);
        Log(g_S->driverFail2);
        PostMessageW(g_hMain, WM_APP_DONE, (WPARAM)FALSE, 0);
        return 0;
    }
    Log(g_S->driverOk);

    for (int attempt = 1; attempt <= MAX_ATTEMPTS && !ok; attempt++)
    {
        Log(g_S->attempt, attempt, MAX_ATTEMPTS);
        Log(g_S->restarting);
        RestartWindowsAudioService();

        Log(g_S->priming);
        StartSilence();
        Sleep(1200 + attempt * 800);

        DWORD Count = 256;
        PHYSICAL_MEMORY_PATCH* patch =
            (PHYSICAL_MEMORY_PATCH*)malloc(sizeof(PHYSICAL_MEMORY_PATCH) * 256);
        Log(g_S->generating);
        if (p.force)
            GenerateForceDisableLimiterPatch(patch, &Count, p.to, p.restore);
        else
            GenerateDisableAudioEngineLimiterPatch(patch, &Count, p.from, p.to, p.restore);
        Log(g_S->scanning, Count);
        ok = ApplyPatch(patch, Count);
        free(patch);

        // 最終再起動はしない: 稼働中 audiodg の共有ページを直接書き換えたので即反映され続ける。
        // ここで再起動すると churn 環境では未パッチの新エンジンが読まれ、効果が一瞬で消える。
        if (ok)
            Log(g_S->applyLive);
        else
            RestartWindowsAudioService();   // 失敗時のみ、次の試行に向けクリーンにする
        StopSilence();

        if (!ok) Log(g_S->retry);
    }

    Unload();
    UnloadInpOutDriver();                     // 脆弱ドライバを常駐させない（EAC対策・衛生）
    if (ok) { SaveApplied(!p.restore, p.to); g_disabled = (p.restore == FALSE); }

    if (ok)
        Log(p.restore ? g_S->doneRestore : g_S->doneDisable);
    else
    {
        Log(g_S->failFound);
        Log(g_S->failHint1);
        Log(g_S->failHint2);
    }
    Log(g_S->rebootNote);

    PostMessageW(g_hMain, WM_APP_DONE, (WPARAM)ok, 0);
    return 0;
}

static bool ResolveOverriding(HWND hwnd, float* outOver)
{
    if (IsDlgButtonChecked(hwnd, IDC_RAD_FULL) == BST_CHECKED)
    {
        DWORD bits = 0x7F800000;           // +inf : limiter never engages
        memcpy(outOver, &bits, 4);
        return true;
    }
    if (IsDlgButtonChecked(hwnd, IDC_RAD_0DB) == BST_CHECKED)
    {
        *outOver = 1.0f;
        return true;
    }
    wchar_t t[64];
    GetDlgItemTextW(hwnd, IDC_EDIT_DB, t, _countof(t));
    wchar_t* end = NULL;
    double d = wcstod(t, &end);
    if (end == t) { MessageBoxW(hwnd, g_S->inErrBadNum, g_S->inErrTitle, MB_ICONWARNING); return false; }
    double lin = pow(10.0, d / 20.0);
    if (!(lin > 0.0) || !_finite(lin)) { MessageBoxW(hwnd, g_S->inErrRange, g_S->inErrTitle, MB_ICONWARNING); return false; }
    *outOver = (float)lin;
    return true;
}

static void SetBusy(HWND hwnd, bool busy)
{
    for (int id : { IDC_BTN_DISABLE, IDC_BTN_RESTORE, IDC_BTN_VERIFY, IDC_RAD_FULL, IDC_RAD_0DB,
                    IDC_RAD_CUSTOM, IDC_EDIT_DB, IDC_LANG })
        EnableWindow(GetDlgItem(hwnd, id), !busy);
    SetWindowTextW(g_hStatus, busy ? g_S->statusBusy : g_S->statusReady);
}

static void StartOperationWith(HWND hwnd, float over, BOOL restore)
{
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;
    BOOL force = (IsDlgButtonChecked(hwnd, IDC_CHK_FORCE) == BST_CHECKED);
    float value = over;
    if (force)
    {
        DWORD inf = 0x7F800000;        // +∞: 閾値を∞にしてリミッタを完全に不作動化
        memcpy(&value, &inf, 4);
    }
    OpParam* pp = (OpParam*)malloc(sizeof(OpParam));
    pp->from = 0.985f;
    pp->to = value;
    pp->restore = restore;
    pp->force = force;
    SetBusy(hwnd, true);
    CloseHandle(CreateThread(NULL, 0, OpThread, pp, 0, NULL));
}

static void StartOperation(HWND hwnd, BOOL restore)
{
    // アンチチート系ゲーム稼働中は適用で終了させられる恐れがあるため警告
    wchar_t ac[64];
    if (AntiCheatRunning(ac, _countof(ac)))
    {
        wchar_t msg[800];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE, g_S->acWarnBody, ac);
        if (MessageBoxW(hwnd, msg, g_S->acWarnTitle, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
    }
    float over;
    if (restore) over = LoadLastOverriding();
    else if (!ResolveOverriding(hwnd, &over)) return;
    StartOperationWith(hwnd, over, restore);
}

// 実メモリを読み戻して「今パッチが生きているか」を検証する（書き込みなし）
static DWORD WINAPI VerifyThread(LPVOID)
{
    Log(g_S->verifyStart);
    if (!Load())
    {
        Log(g_S->driverFail1);
        PostMessageW(g_hMain, WM_APP_VERIFY_DONE, (WPARAM)0, 0);
        return 0;
    }
    DWORD infbits = 0x7F800000; float inf; memcpy(&inf, &infbits, 4);
    DWORD count = 256;
    PHYSICAL_MEMORY_PATCH* patch = (PHYSICAL_MEMORY_PATCH*)malloc(sizeof(PHYSICAL_MEMORY_PATCH) * 256);
    // bRestore=TRUE → 「適用後ページ」のハッシュで生成。今パッチが生きていれば一致する。
    GenerateForceDisableLimiterPatch(patch, &count, inf, TRUE);
    int result;
    if (count == 0)
        result = 2;   // この版に標的なし＝検証不可
    else
    {
        StartSilence();
        Sleep(1500);
        DWORD found = CountPatchedPages(patch, count);
        StopSilence();
        result = (found > 0) ? 1 : 0;
    }
    free(patch);
    Unload();
    UnloadInpOutDriver();
    if (result == 1) { g_disabled = true;  Log(g_S->verifyLive); }
    else if (result == 0) { g_disabled = false; Log(g_S->verifyDead); }
    else Log(g_S->verifyNoTarget);
    PostMessageW(g_hMain, WM_APP_VERIFY_DONE, (WPARAM)result, 0);
    return 0;
}

static void StartVerify(HWND hwnd)
{
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;
    SetBusy(hwnd, true);
    CloseHandle(CreateThread(NULL, 0, VerifyThread, NULL, 0, NULL));
}

// ============================ UI ============================
static void SetFontRec(HWND h)
{
    SendMessageW(h, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        SetFontRec(c);
}

// 赤/緑インジケータを現在状態に更新（軽量: レジストリ+ブート時刻の照合のみ）
// g_disabled は「本セッションで実際に適用したか」のみを表す（起動時は false=赤）。
// 以前はレジストリの記録でブート後ずっと緑にしていたが、エンジン再読込でパッチが
// 消えても緑のままになり誤解を招くため、セッション内フラグに変更した。
static void UpdateStateIndicator(HWND hwnd)
{
    HWND s = GetDlgItem(hwnd, IDC_STATE);
    if (s)
    {
        SetWindowTextW(s, g_disabled ? g_S->stateDisabled : g_S->stateNormal);
        InvalidateRect(s, NULL, TRUE);
    }
    UpdateTrayTip();
}

static void Relabel(HWND hwnd)
{
    SetDlgItemTextW(hwnd, IDC_HEADER, g_S->header);
    SetDlgItemTextW(hwnd, IDC_LANGLBL, g_S->langLabel);
    SetDlgItemTextW(hwnd, IDC_GRP_STATUS, g_S->grpStatus);
    SetDlgItemTextW(hwnd, IDC_GRP_THRESH, g_S->grpThreshold);
    SetDlgItemTextW(hwnd, IDC_RAD_FULL, g_S->radFull);
    SetDlgItemTextW(hwnd, IDC_RAD_0DB, g_S->rad0db);
    SetDlgItemTextW(hwnd, IDC_RAD_CUSTOM, g_S->radCustom);
    SetDlgItemTextW(hwnd, IDC_LBL_DB, g_S->dbUnit);
    SetDlgItemTextW(hwnd, IDC_BTN_DISABLE, g_S->btnDisable);
    SetDlgItemTextW(hwnd, IDC_BTN_RESTORE, g_S->btnRestore);
    SetDlgItemTextW(hwnd, IDC_BTN_VERIFY, g_S->btnVerify);
    SetDlgItemTextW(hwnd, IDC_LOGLBL, g_S->logLabel);
    SetDlgItemTextW(hwnd, IDC_GRP_OPT, g_S->grpOptions);
    SetDlgItemTextW(hwnd, IDC_CHK_TRAY, g_S->chkTray);
    SetDlgItemTextW(hwnd, IDC_CHK_STARTUP, g_S->chkStartup);
    SetDlgItemTextW(hwnd, IDC_CHK_FORCE, g_S->chkForce);
    if (!g_busy) SetWindowTextW(g_hStatus, g_S->statusReady);
    wchar_t info[600]; BuildPreflight(info, _countof(info));
    SetDlgItemTextW(hwnd, IDC_INFO, info);
    UpdateStateIndicator(hwnd);
}

static void CreateControls(HWND hwnd)
{
    g_minToTray = RegGetDword(L"MinimizeToTray", 0) != 0;

    CreateWindowExW(0, L"STATIC", g_S->header, WS_CHILD | WS_VISIBLE,
        14, 12, 340, 20, hwnd, (HMENU)IDC_HEADER, NULL, NULL);

    // language selector (top-right)
    CreateWindowExW(0, L"STATIC", g_S->langLabel, WS_CHILD | WS_VISIBLE | SS_RIGHT,
        356, 12, 64, 18, hwnd, (HMENU)IDC_LANGLBL, NULL, NULL);
    HWND cb = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP, 424, 9, 120, 200,
        hwnd, (HMENU)IDC_LANG, NULL, NULL);
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)L"日本語");
    SendMessageW(cb, CB_SETCURSEL, (g_S == &S_JA) ? 1 : 0, 0);

    // 赤/緑 状態インジケータ
    CreateWindowExW(0, L"STATIC", g_S->stateNormal,
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
        12, 40, 536, 30, hwnd, (HMENU)IDC_STATE, NULL, NULL);

    CreateWindowExW(0, L"BUTTON", g_S->grpStatus, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        12, 78, 536, 128, hwnd, (HMENU)IDC_GRP_STATUS, NULL, NULL);
    wchar_t info[600]; BuildPreflight(info, _countof(info));
    CreateWindowExW(0, L"STATIC", info, WS_CHILD | WS_VISIBLE,
        26, 98, 510, 102, hwnd, (HMENU)IDC_INFO, NULL, NULL);

    CreateWindowExW(0, L"BUTTON", g_S->grpThreshold, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        12, 214, 536, 96, hwnd, (HMENU)IDC_GRP_THRESH, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->rad0db,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 26, 236, 510, 20,
        hwnd, (HMENU)IDC_RAD_0DB, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->radCustom,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 26, 258, 70, 20,
        hwnd, (HMENU)IDC_RAD_CUSTOM, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"-0.05",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 257, 70, 22,
        hwnd, (HMENU)IDC_EDIT_DB, NULL, NULL);
    CreateWindowExW(0, L"STATIC", g_S->dbUnit, WS_CHILD | WS_VISIBLE,
        176, 260, 40, 20, hwnd, (HMENU)IDC_LBL_DB, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->radFull,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 26, 282, 516, 20,
        hwnd, (HMENU)IDC_RAD_FULL, NULL, NULL);
    CheckDlgButton(hwnd, IDC_RAD_0DB, BST_CHECKED);

    CreateWindowExW(0, L"BUTTON", g_S->btnDisable,
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 12, 320, 200, 36,
        hwnd, (HMENU)IDC_BTN_DISABLE, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->btnRestore,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 224, 320, 160, 36,
        hwnd, (HMENU)IDC_BTN_RESTORE, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->btnVerify,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 396, 320, 152, 36,
        hwnd, (HMENU)IDC_BTN_VERIFY, NULL, NULL);

    // options (tray / startup / force)
    CreateWindowExW(0, L"BUTTON", g_S->grpOptions, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        12, 366, 536, 98, hwnd, (HMENU)IDC_GRP_OPT, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->chkTray,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 26, 386, 510, 20,
        hwnd, (HMENU)IDC_CHK_TRAY, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->chkStartup,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 26, 410, 510, 20,
        hwnd, (HMENU)IDC_CHK_STARTUP, NULL, NULL);
    CreateWindowExW(0, L"BUTTON", g_S->chkForce,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 26, 434, 510, 20,
        hwnd, (HMENU)IDC_CHK_FORCE, NULL, NULL);
    if (g_minToTray) CheckDlgButton(hwnd, IDC_CHK_TRAY, BST_CHECKED);
    if (IsStartupEnabled()) CheckDlgButton(hwnd, IDC_CHK_STARTUP, BST_CHECKED);
    if (RegGetDword(L"ForceMode", 0) != 0) CheckDlgButton(hwnd, IDC_CHK_FORCE, BST_CHECKED);

    CreateWindowExW(0, L"STATIC", g_S->logLabel, WS_CHILD | WS_VISIBLE,
        12, 472, 200, 18, hwnd, (HMENU)IDC_LOGLBL, NULL, NULL);
    g_hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        12, 492, 536, 150, hwnd, (HMENU)IDC_LOG, NULL, NULL);
    g_hStatus = CreateWindowExW(0, L"STATIC", g_S->statusReady,
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, 12, 652, 536, 20,
        hwnd, (HMENU)IDC_STATUS, NULL, NULL);

    SetFontRec(hwnd);
    UpdateStateIndicator(hwnd);
    SetTimer(hwnd, IDT_STATE, 2000, NULL);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hMain = hwnd;
        CreateControls(hwnd);
        if (g_autoStart) { g_minToTray = true; CheckDlgButton(hwnd, IDC_CHK_TRAY, BST_CHECKED); }
        if (g_minToTray) AddTray(hwnd);
        if (g_autoStart) PostMessageW(hwnd, WM_APP_AUTOSTART, 0, 0);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_BTN_DISABLE: StartOperation(hwnd, FALSE); return 0;
        case IDC_BTN_RESTORE: StartOperation(hwnd, TRUE);  return 0;
        case IDC_BTN_VERIFY:  StartVerify(hwnd);           return 0;
        case IDC_LANG:
            if (HIWORD(wp) == CBN_SELCHANGE)
            {
                LRESULT sel = SendMessageW((HWND)lp, CB_GETCURSEL, 0, 0);
                g_S = (sel == 1) ? &S_JA : &S_EN;
                Relabel(hwnd);
            }
            return 0;
        case IDC_EDIT_DB:
            if (HIWORD(wp) == EN_SETFOCUS)
                CheckRadioButton(hwnd, IDC_RAD_FULL, IDC_RAD_CUSTOM, IDC_RAD_CUSTOM);
            return 0;
        case IDC_CHK_TRAY:
            g_minToTray = (IsDlgButtonChecked(hwnd, IDC_CHK_TRAY) == BST_CHECKED);
            RegSetDword(L"MinimizeToTray", g_minToTray ? 1 : 0);
            if (g_minToTray) AddTray(hwnd); else RemoveTray();
            return 0;
        case IDC_CHK_STARTUP:
        {
            bool want = (IsDlgButtonChecked(hwnd, IDC_CHK_STARTUP) == BST_CHECKED);
            if (!SetStartup(want))
            {
                CheckDlgButton(hwnd, IDC_CHK_STARTUP, want ? BST_UNCHECKED : BST_CHECKED);
                MessageBoxW(hwnd, L"Failed to update the startup task (schtasks).", g_S->dlgTitle, MB_ICONWARNING);
            }
            return 0;
        }
        case IDC_CHK_FORCE:
            RegSetDword(L"ForceMode", (IsDlgButtonChecked(hwnd, IDC_CHK_FORCE) == BST_CHECKED) ? 1 : 0);
            return 0;
        case IDM_TRAY_SHOW:
            ShowWindow(hwnd, SW_SHOW); ShowWindow(hwnd, SW_RESTORE); SetForegroundWindow(hwnd);
            return 0;
        case IDM_TRAY_DISABLE: StartOperation(hwnd, FALSE); return 0;
        case IDM_TRAY_RESTORE: StartOperation(hwnd, TRUE);  return 0;
        case IDM_TRAY_EXIT:    g_minToTray = false; DestroyWindow(hwnd); return 0;
        }
        return 0;

    case WM_APP_LOG:
    {
        wchar_t* s = (wchar_t*)lp;
        if (s) { AppendLog(s); free(s); }
        return 0;
    }

    case WM_APP_DONE:
    {
        SetBusy(hwnd, false);
        InterlockedExchange(&g_busy, 0);
        wchar_t info[600]; BuildPreflight(info, _countof(info));
        SetDlgItemTextW(hwnd, IDC_INFO, info);
        UpdateStateIndicator(hwnd);
        if (IsWindowVisible(hwnd))
            MessageBoxW(hwnd, wp ? g_S->msgDone : g_S->msgFail, g_S->dlgTitle,
                wp ? MB_ICONINFORMATION : MB_ICONWARNING);
        return 0;
    }

    case WM_APP_VERIFY_DONE:
    {
        SetBusy(hwnd, false);
        InterlockedExchange(&g_busy, 0);
        UpdateStateIndicator(hwnd);
        const wchar_t* vmsg = (wp == 1) ? g_S->verifyLive : (wp == 0) ? g_S->verifyDead : g_S->verifyNoTarget;
        MessageBoxW(hwnd, vmsg, g_S->verifyTitle, (wp == 1) ? MB_ICONINFORMATION : MB_ICONWARNING);
        return 0;
    }

    case WM_APP_AUTOSTART:
        StartOperationWith(hwnd, LoadLastOverriding(), FALSE);
        return 0;

    case WM_APP_TRAY:
        if (LOWORD(lp) == WM_LBUTTONDBLCLK || LOWORD(lp) == WM_LBUTTONUP)
        {
            ShowWindow(hwnd, SW_SHOW); ShowWindow(hwnd, SW_RESTORE); SetForegroundWindow(hwnd);
        }
        else if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
        {
            POINT pt; GetCursorPos(&pt);
            HMENU m = CreatePopupMenu();
            AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW, g_S->trayShow);
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_STRING, IDM_TRAY_DISABLE, g_S->trayDisable);
            AppendMenuW(m, MF_STRING, IDM_TRAY_RESTORE, g_S->trayRestore);
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, g_S->trayExit);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(m);
        }
        return 0;

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED && g_minToTray) { ShowWindow(hwnd, SW_HIDE); return 0; }
        break;

    case WM_TIMER:
        if (wp == IDT_STATE && !g_busy) UpdateStateIndicator(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC:
        if ((HWND)lp == GetDlgItem(hwnd, IDC_STATE))
        {
            HDC dc = (HDC)wp;
            SetTextColor(dc, RGB(255, 255, 255));
            SetBkColor(dc, g_disabled ? RGB(30, 160, 74) : RGB(200, 60, 60));
            return (LRESULT)(g_disabled ? g_brGreen : g_brRed);
        }
        break;

    case WM_CLOSE:
        if (g_busy) { MessageBoxW(hwnd, g_S->busyClose, g_S->dlgTitle, MB_ICONWARNING); return 0; }
        if (g_minToTray) { ShowWindow(hwnd, SW_HIDE); return 0; }   // トレイに収納
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, IDT_STATE);
        RemoveTray();
        StopSilence();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    if (wcsstr(GetCommandLineW(), L"/startup")) g_autoStart = true;

    g_hFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");  // Latin + 日本語 両対応
    g_brGreen = CreateSolidBrush(RGB(30, 160, 74));
    g_brRed   = CreateSolidBrush(RGB(200, 60, 60));

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"DpeaklimGuiWnd";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 560, 688 };
    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX);
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
        L"Disable Peak Limiter GUI (dpeaklim)",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, g_autoStart ? SW_HIDE : nCmdShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageW(hwnd, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    return 0;
}
