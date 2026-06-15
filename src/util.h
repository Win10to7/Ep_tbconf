#pragma once
#if !defined(UTIL_H)
#define UTIL_H

#include "config.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

_Success_(return > 0)
int AllocAndLoadString(HMODULE hModule, UINT id, _Out_ TCHAR **pTarget);

_Success_(return != 0)
int ShowMessageFromResource(HMODULE hModule, HWND hWnd,
    int msgId, int titleMsgId, UINT type);

#define ShowMessageFromAppResource(hWnd, msgId, titleMsgId, type) \
    ShowMessageFromResource(g_propSheet.hInstance, hWnd, \
        msgId, titleMsgId, type)

_Success_(return)
BOOL SetCustomVisualFx(void);
BOOL InitHighDpiSupport(void);
UINT GetWindowDpiValue(HWND hWnd);
int GetSystemMetricsForWindowDpi(int nIndex, HWND hWnd);

void NotifyTraySettingsChanged(BOOL bRebuildStartMenu);
BOOL StartIsBackExists(void);
BOOL ReadStartIsBackDword(PCWSTR pszValueName, DWORD *pdwValue);
BOOL WriteStartIsBackDword(PCWSTR pszValueName, DWORD dwValue);
void NotifyStartIsBackSettingsChanged(void);




#endif  /* !defined(UTIL_H) */
