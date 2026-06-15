/*
 * COPYRIGHT: See COPYING in the top level directory
 * PURPOSE:   Provides auxiliary functions
 *
 * PROGRAMMERS: ReactOS Team
 *              Franco Tortoriello (torto09@gmail.com)
 */

#include "app.h"

#include <commctrl.h>
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif

#ifndef PROCESS_PER_MONITOR_DPI_AWARE
#define PROCESS_PER_MONITOR_DPI_AWARE 2
#endif

typedef BOOL (WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE value);
typedef HRESULT (WINAPI *PFN_SetProcessDpiAwareness)(int value);
typedef BOOL (WINAPI *PFN_SetProcessDPIAware)(void);
typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND hWnd);
typedef int (WINAPI *PFN_GetSystemMetricsForDpi)(int nIndex, UINT dpi);

static
BOOL IsDpiApiAccessDenied(void)
{
    return GetLastError() == ERROR_ACCESS_DENIED;
}

BOOL InitHighDpiSupport(void)
{
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    HMODULE hShcore = NULL;
    PFN_SetProcessDpiAwarenessContext pSetProcessDpiAwarenessContext;
    PFN_SetProcessDPIAware pSetProcessDPIAware;
    PFN_SetProcessDpiAwareness pSetProcessDpiAwareness;

    pSetProcessDpiAwarenessContext = hUser32 ?
        (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32,
            "SetProcessDpiAwarenessContext") : NULL;
    if (pSetProcessDpiAwarenessContext)
    {
        SetLastError(ERROR_SUCCESS);
        if (pSetProcessDpiAwarenessContext(
            DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) ||
            IsDpiApiAccessDenied())
        {
            return TRUE;
        }
    }

    hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore)
    {
        pSetProcessDpiAwareness = (PFN_SetProcessDpiAwareness)GetProcAddress(
            hShcore, "SetProcessDpiAwareness");
        if (pSetProcessDpiAwareness)
        {
            HRESULT hr = pSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
            if (SUCCEEDED(hr) || hr == E_ACCESSDENIED)
            {
                FreeLibrary(hShcore);
                return TRUE;
            }
        }
        FreeLibrary(hShcore);
    }

    pSetProcessDPIAware = hUser32 ?
        (PFN_SetProcessDPIAware)GetProcAddress(hUser32,
            "SetProcessDPIAware") : NULL;
    if (!pSetProcessDPIAware)
        return FALSE;

    SetLastError(ERROR_SUCCESS);
    return pSetProcessDPIAware() || IsDpiApiAccessDenied();
}

UINT GetWindowDpiValue(HWND hWnd)
{
    static PFN_GetDpiForWindow s_pGetDpiForWindow;
    static BOOL s_bDpiForWindowInitialized;
    HDC hdc;
    UINT dpi;

    if (!s_bDpiForWindowInitialized)
    {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        s_pGetDpiForWindow = hUser32 ?
            (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow") :
            NULL;
        s_bDpiForWindowInitialized = TRUE;
    }

    if (s_pGetDpiForWindow && hWnd)
        return s_pGetDpiForWindow(hWnd);

    hdc = GetDC(hWnd);
    if (!hdc)
        return USER_DEFAULT_SCREEN_DPI;

    dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hWnd, hdc);
    return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
}

int GetSystemMetricsForWindowDpi(int nIndex, HWND hWnd)
{
    static PFN_GetSystemMetricsForDpi s_pGetSystemMetricsForDpi;
    static BOOL s_bMetricsForDpiInitialized;
    UINT dpi = GetWindowDpiValue(hWnd);

    if (!s_bMetricsForDpiInitialized)
    {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        s_pGetSystemMetricsForDpi = hUser32 ?
            (PFN_GetSystemMetricsForDpi)GetProcAddress(hUser32,
                "GetSystemMetricsForDpi") : NULL;
        s_bMetricsForDpiInitialized = TRUE;
    }

    if (s_pGetSystemMetricsForDpi)
        return s_pGetSystemMetricsForDpi(nIndex, dpi);

    return MulDiv(GetSystemMetrics(nIndex), dpi, USER_DEFAULT_SCREEN_DPI);
}

_Success_(return >= 0)
static
int LengthOfStrResource(HMODULE hModule, UINT id)
{
    if (!hModule)
        return -1;

    /* There are always blocks of 16 strings */
    TCHAR *name = MAKEINTRESOURCE((id >> 4) + 1);

    /* Find the string table block */
    HRSRC hrSrc = FindResource(hModule, name, RT_STRING);
    if (!hrSrc)
        return -1;

    HGLOBAL hRes = LoadResource(hModule, hrSrc);
    if (!hRes)
        return -1;

    /* Note: Always use WCHAR because the resources are in Unicode */
    WCHAR *pStrLen = (WCHAR *)LockResource(hRes);
    if (!pStrLen)
        return -1;

    /* Find the string we're looking for */
    id &= 0xF; /* Position in the block, same as % 16 */
    for (UINT x = 0; x < id; x++)
        pStrLen += (*pStrLen) + 1;

    /* Found the string */
    return (int)(*pStrLen);
}

_Success_(return > 0)
int AllocAndLoadString(HMODULE hModule, UINT id, _Out_ TCHAR **pTarget)
{
    int len = LengthOfStrResource(hModule, id);
    if (len++ > 0)
    {
        (*pTarget) = (TCHAR *)Alloc(0, len * sizeof(TCHAR));
        if (*pTarget)
        {
            int ret = LoadString(hModule, id, *pTarget, len);
            if (ret > 0)
                return ret;

            /* Could not load the string */
            Free((HLOCAL)(*pTarget));
        }
    }

    *pTarget = NULL;
    return 0;
}

_Success_(return != 0)
int ShowMessageFromResource(HMODULE hModule, HWND hWnd,
    int msgId, int titleMsgId, UINT type)
{
    TCHAR *msg;
    TCHAR *msgTitle;

    if (!AllocAndLoadString(hModule, msgId, &msg))
        return 0;

    if (!AllocAndLoadString(hModule, titleMsgId, &msgTitle))
    {
        Free(msg);
        return 0;
    }

    int ret = MessageBox(hWnd, msg, msgTitle, MB_APPLMODAL | type);

    Free(msg);
    Free(msgTitle);
    return ret;
}

/* Set the Performance Visual Effects preset to "custom", so that those settings
 * are reflected in the system property sheet page.
 */
_Success_(return)
BOOL SetCustomVisualFx(void)
{
    HKEY hKey;
    LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER,
        TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects"),
        0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS)
        return FALSE;

    DWORD dwData = 3;
    LRESULT result = RegSetValueEx(hKey, TEXT("VisualFXSetting"), 0, REG_DWORD,
        (BYTE *)&dwData, sizeof(DWORD));

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}
static const WCHAR g_startIsBackKey[] = L"SOFTWARE\\StartIsBack";

BOOL StartIsBackExists(void)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, g_startIsBackKey, 0,
        KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return FALSE;
    RegCloseKey(hKey);
    return TRUE;
}

BOOL ReadStartIsBackDword(PCWSTR pszValueName, DWORD *pdwValue)
{
    DWORD dwType = 0;
    DWORD dwSize;

    if (!pszValueName || !*pszValueName || !pdwValue)
        return FALSE;

    dwSize = sizeof(*pdwValue);
    return RegGetValueW(HKEY_CURRENT_USER, g_startIsBackKey, pszValueName,
        RRF_RT_REG_DWORD, &dwType, pdwValue, &dwSize) == ERROR_SUCCESS &&
        dwType == REG_DWORD;
}

BOOL WriteStartIsBackDword(PCWSTR pszValueName, DWORD dwValue)
{
    HKEY hKey;
    LSTATUS status;

    if (!pszValueName || !*pszValueName)
        return FALSE;
    if (!StartIsBackExists())
        return TRUE;

    status = RegOpenKeyExW(HKEY_CURRENT_USER, g_startIsBackKey, 0,
        KEY_SET_VALUE, &hKey);
    if (status != ERROR_SUCCESS)
        return FALSE;

    status = RegSetValueExW(hKey, pszValueName, 0, REG_DWORD,
        (const BYTE *)&dwValue, sizeof(dwValue));
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}


static
void NotifySettingChanged(LPCTSTR pszSetting)
{
    SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0L, (LPARAM)pszSetting);
}

void NotifyTraySettingsChanged(BOOL bRebuildStartMenu)
{
    static const UINT SBM_REBUILDMENU = WM_USER + 13;
    HWND hTaskbar;

    NotifySettingChanged(TEXT("TraySettings"));
    if (!bRebuildStartMenu)
        return;

    hTaskbar = FindWindow(TEXT("Shell_TrayWnd"), TEXT(""));
    if (hTaskbar)
        PostMessage(hTaskbar, SBM_REBUILDMENU, 0L, 0L);
}

void NotifyStartIsBackSettingsChanged(void)
{
    NotifySettingChanged(TEXT("SIBSettings"));
}
