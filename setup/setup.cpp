// setup.cpp  ―  自前インストーラー（dpeaklim_setup.exe）
// dpeaklim_gui.exe と inpoutx64 ドライバをリソースに埋め込み、Program Files へ展開する。
// ショートカット作成・アンインストール情報登録・アンインストーラー同梱。追加ツール不要。
#define _WIN32_WINNT 0x0A00
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

#define APP_NAME   L"Disable Peak Limiter"
#define EXE_NAME   L"dpeaklim_gui.exe"
#define UNINST_KEY L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\DisablePeakLimiter"
#define TASK_NAME  L"dpeaklimGuiAutostart"

#define RES_GUI 101
#define RES_DLL 102
#define RES_SYS 103

#define IDC_CHK_DESK    2001
#define IDC_CHK_MENU    2002
#define IDC_BTN_INSTALL 2003
#define IDC_BTN_UNINST  2004
#define IDC_STATUS      2005
#define IDC_PATH        2006

static HFONT g_font = NULL;

static bool WriteResourceToFile(int id, const wchar_t* path)
{
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!r) return false;
    HGLOBAL g = LoadResource(NULL, r);
    if (!g) return false;
    void* p = LockResource(g);
    DWORD sz = SizeofResource(NULL, r);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0; BOOL ok = WriteFile(h, p, sz, &wr, NULL);
    CloseHandle(h);
    return ok && wr == sz;
}

static void InstallDir(wchar_t* out, size_t n)
{
    wchar_t pf[MAX_PATH] = L"C:\\Program Files";
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, pf);
    _snwprintf_s(out, n, _TRUNCATE, L"%s\\%s", pf, APP_NAME);
}

static void ShortcutPath(int csidl, wchar_t* out, size_t n)
{
    wchar_t dir[MAX_PATH] = L"";
    SHGetFolderPathW(NULL, csidl, NULL, 0, dir);
    _snwprintf_s(out, n, _TRUNCATE, L"%s\\%s.lnk", dir, APP_NAME);
}

static bool MakeShortcut(const wchar_t* target, const wchar_t* link, const wchar_t* workdir)
{
    IShellLinkW* sl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl);
    if (FAILED(hr)) return false;
    sl->SetPath(target);
    sl->SetWorkingDirectory(workdir);
    sl->SetDescription(L"Disable the Windows audio peak limiter");
    IPersistFile* pf = NULL;
    hr = sl->QueryInterface(IID_IPersistFile, (void**)&pf);
    if (SUCCEEDED(hr)) { hr = pf->Save(link, TRUE); pf->Release(); }
    sl->Release();
    return SUCCEEDED(hr);
}

static void RegStr(HKEY k, const wchar_t* name, const wchar_t* val)
{
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val, (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
}

static bool DoInstall(bool desktop, bool startmenu, wchar_t* installedExe, size_t exeN)
{
    wchar_t dir[MAX_PATH]; InstallDir(dir, MAX_PATH);
    CreateDirectoryW(dir, NULL);

    wchar_t exe[MAX_PATH], dll[MAX_PATH], sys[MAX_PATH], uninst[MAX_PATH], self[MAX_PATH];
    _snwprintf_s(exe,    MAX_PATH, _TRUNCATE, L"%s\\%s", dir, EXE_NAME);
    _snwprintf_s(dll,    MAX_PATH, _TRUNCATE, L"%s\\inpoutx64.dll", dir);
    _snwprintf_s(sys,    MAX_PATH, _TRUNCATE, L"%s\\inpoutx64.sys", dir);
    _snwprintf_s(uninst, MAX_PATH, _TRUNCATE, L"%s\\uninstall.exe", dir);

    if (!WriteResourceToFile(RES_GUI, exe)) return false;
    WriteResourceToFile(RES_DLL, dll);
    WriteResourceToFile(RES_SYS, sys);
    _snwprintf_s(installedExe, exeN, _TRUNCATE, L"%s", exe);

    GetModuleFileNameW(NULL, self, MAX_PATH);
    CopyFileW(self, uninst, FALSE);   // 自分自身をアンインストーラーとして設置

    CoInitialize(NULL);
    if (startmenu) { wchar_t lp[MAX_PATH]; ShortcutPath(CSIDL_COMMON_PROGRAMS, lp, MAX_PATH); MakeShortcut(exe, lp, dir); }
    if (desktop)   { wchar_t lp[MAX_PATH]; ShortcutPath(CSIDL_COMMON_DESKTOPDIRECTORY, lp, MAX_PATH); MakeShortcut(exe, lp, dir); }
    CoUninitialize();

    HKEY k;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, UNINST_KEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS)
    {
        RegStr(k, L"DisplayName", APP_NAME);
        RegStr(k, L"DisplayVersion", L"1.0");
        RegStr(k, L"Publisher", L"dpeaklim (Suguru Kawamoto, MIT) + GUI");
        RegStr(k, L"InstallLocation", dir);
        RegStr(k, L"DisplayIcon", exe);
        wchar_t us[MAX_PATH + 32];
        _snwprintf_s(us, _countof(us), _TRUNCATE, L"\"%s\" /uninstall", uninst);
        RegStr(k, L"UninstallString", us);
        DWORD one = 1;
        RegSetValueExW(k, L"NoModify", 0, REG_DWORD, (BYTE*)&one, 4);
        RegSetValueExW(k, L"NoRepair", 0, REG_DWORD, (BYTE*)&one, 4);
        RegCloseKey(k);
    }
    return true;
}

static void DoUninstall()
{
    wchar_t dir[MAX_PATH]; InstallDir(dir, MAX_PATH);

    // ログイン自動起動タスクを削除（あれば）
    {
        wchar_t cmd[256];
        _snwprintf_s(cmd, 256, _TRUNCATE, L"schtasks /Delete /TN \"%s\" /F", TASK_NAME);
        STARTUPINFOW si = { sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        { WaitForSingleObject(pi.hProcess, 8000); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    }
    // ショートカット削除
    { wchar_t lp[MAX_PATH];
      ShortcutPath(CSIDL_COMMON_PROGRAMS, lp, MAX_PATH); DeleteFileW(lp);
      ShortcutPath(CSIDL_COMMON_DESKTOPDIRECTORY, lp, MAX_PATH); DeleteFileW(lp); }
    // レジストリ削除
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, UNINST_KEY);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\dpeaklim_gui");
    // ファイル削除
    const wchar_t* files[] = { EXE_NAME, L"inpoutx64.dll", L"inpoutx64.sys", L"inpout32.dll", L"inpout32.sys" };
    for (int i = 0; i < 5; i++) { wchar_t p[MAX_PATH]; _snwprintf_s(p, MAX_PATH, _TRUNCATE, L"%s\\%s", dir, files[i]); DeleteFileW(p); }
    // 自分自身(uninstall.exe)とフォルダは、終了後に cmd で削除
    wchar_t self[MAX_PATH]; GetModuleFileNameW(NULL, self, MAX_PATH);
    wchar_t cmd[1200];
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE,
        L"cmd.exe /c ping 127.0.0.1 -n 2 >nul & del /q \"%s\" & rmdir \"%s\"", self, dir);
    STARTUPINFOW si = { sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
}

static HWND Label(HWND parent, const wchar_t* t, int x, int y, int w, int h, int id = -1, DWORD extra = 0)
{
    HWND c = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | extra, x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        Label(hwnd, L"Disable Peak Limiter — セットアップ", 16, 14, 440, 22);
        Label(hwnd, L"Windows オーディオのピークリミッタ無効化ツールをインストールします。",
              16, 42, 450, 36, -1, 0);
        wchar_t dir[MAX_PATH]; InstallDir(dir, MAX_PATH);
        wchar_t line[MAX_PATH + 32]; _snwprintf_s(line, _countof(line), _TRUNCATE, L"インストール先: %s", dir);
        Label(hwnd, line, 16, 84, 450, 20, IDC_PATH);

        HWND d = CreateWindowExW(0, L"BUTTON", L"デスクトップにショートカットを作成",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 16, 116, 430, 22, hwnd, (HMENU)IDC_CHK_DESK, NULL, NULL);
        HWND m = CreateWindowExW(0, L"BUTTON", L"スタートメニューに登録",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 16, 142, 430, 22, hwnd, (HMENU)IDC_CHK_MENU, NULL, NULL);
        SendMessageW(d, WM_SETFONT, (WPARAM)g_font, TRUE); SendMessageW(m, WM_SETFONT, (WPARAM)g_font, TRUE);
        CheckDlgButton(hwnd, IDC_CHK_DESK, BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_MENU, BST_CHECKED);

        HWND bi = CreateWindowExW(0, L"BUTTON", L"インストール",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 16, 180, 200, 40, hwnd, (HMENU)IDC_BTN_INSTALL, NULL, NULL);
        HWND bu = CreateWindowExW(0, L"BUTTON", L"アンインストール",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 240, 180, 200, 40, hwnd, (HMENU)IDC_BTN_UNINST, NULL, NULL);
        SendMessageW(bi, WM_SETFONT, (WPARAM)g_font, TRUE); SendMessageW(bu, WM_SETFONT, (WPARAM)g_font, TRUE);

        Label(hwnd, L"", 16, 232, 450, 20, IDC_STATUS);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_BTN_INSTALL)
        {
            SetDlgItemTextW(hwnd, IDC_STATUS, L"インストール中...");
            bool desk = IsDlgButtonChecked(hwnd, IDC_CHK_DESK) == BST_CHECKED;
            bool menu = IsDlgButtonChecked(hwnd, IDC_CHK_MENU) == BST_CHECKED;
            wchar_t exe[MAX_PATH] = L"";
            bool ok = DoInstall(desk, menu, exe, MAX_PATH);
            SetDlgItemTextW(hwnd, IDC_STATUS, ok ? L"完了しました。" : L"失敗しました。");
            if (ok)
            {
                if (MessageBoxW(hwnd, L"インストールが完了しました。今すぐ起動しますか？",
                    APP_NAME, MB_ICONINFORMATION | MB_YESNO) == IDYES)
                    ShellExecuteW(hwnd, L"open", exe, NULL, NULL, SW_SHOWNORMAL);
                DestroyWindow(hwnd);
            }
            else MessageBoxW(hwnd, L"インストールに失敗しました（管理者で実行していますか？）。", APP_NAME, MB_ICONWARNING);
            return 0;
        }
        if (LOWORD(wp) == IDC_BTN_UNINST)
        {
            if (MessageBoxW(hwnd, L"アンインストールしますか？（ファイル・ショートカット・登録を削除します）",
                APP_NAME, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES)
            {
                DoUninstall();
                MessageBoxW(hwnd, L"アンインストールしました。", APP_NAME, MB_ICONINFORMATION);
                DestroyWindow(hwnd);
            }
            return 0;
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // /uninstall で起動された場合（Add/Remove Programs から）
    if (wcsstr(GetCommandLineW(), L"/uninstall"))
    {
        if (MessageBoxW(NULL, L"Disable Peak Limiter をアンインストールしますか？",
            APP_NAME, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES)
        {
            DoUninstall();
            MessageBoxW(NULL, L"アンインストールしました。", APP_NAME, MB_ICONINFORMATION);
        }
        return 0;
    }

    g_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"DpeaklimSetupWnd";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 480, 300 };
    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Disable Peak Limiter セットアップ",
        style, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageW(hwnd, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    return 0;
}
