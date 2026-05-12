/*
 * COPYRIGHT: See COPYING in the top level directory
 * PURPOSE:   Customize Start menu dialogs
 *
 * PROGRAMMERS: SpaofSpaac
 *              Franco Tortoriello (torto09@gmail.com)
 */

#include "app.h"
#include "resource.h"
#include "util.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <windowsx.h>

#define EnableApply() \
    SendMessage(g_propSheet.hWnd, PSM_CHANGED, (WPARAM)g_hDlg, 0L)

#define SetComboIndex(iControl, index) \
    SendDlgItemMessage(g_hDlg, iControl, CB_SETCURSEL, (WPARAM)index, 0L)

static const TCHAR g_explorerKey[] =
    TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced");

static const TCHAR g_explorerPatcherKey[] =
    TEXT("Software\\ExplorerPatcher");

static const WCHAR g_policyExplorerKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";

static const WCHAR g_policyExplorerAltKey[] =
    L"Software\\Policies\\Microsoft\\Windows\\Explorer";

enum
{
    STARTMENU_RECENT_PROGRAMS_DEFAULT = 9,
    STARTMENU_RECENT_PROGRAMS_MAX = 30,
    STARTMENU_JUMPLIST_ITEMS_DEFAULT = 10,
    STARTMENU_JUMPLIST_ITEMS_MAX = 60,
};


static
PCWSTR WideStrChr(PCWSTR pszText, WCHAR ch)
{
    while (*pszText)
    {
        if (*pszText == ch)
            return pszText;
        pszText++;
    }

    return NULL;
}

static
PWSTR WideStrRChr(PWSTR pszText, WCHAR ch)
{
    PWSTR pszLast = NULL;

    while (*pszText)
    {
        if (*pszText == ch)
            pszLast = pszText;
        pszText++;
    }

    return pszLast;
}

static
int WideAtoi(PCWSTR pszText)
{
    int iValue = 0;

    while (*pszText == L' ' || *pszText == L'\t')
        pszText++;

    while (*pszText >= L'0' && *pszText <= L'9')
    {
        iValue = iValue * 10 + (*pszText - L'0');
        pszText++;
    }

    return iValue;
}

static
BOOL CopyWideString(PWSTR pszTarget, size_t cchTarget, PCWSTR pszSource)
{
    size_t i;

    if (!pszTarget || !cchTarget || !pszSource)
        return FALSE;

    for (i = 0; i + 1 < cchTarget && pszSource[i]; i++)
        pszTarget[i] = pszSource[i];
    pszTarget[i] = L'\0';
    return pszSource[i] == L'\0';
}

static
BOOL AppendWideString(PWSTR pszTarget, size_t cchTarget, PCWSTR pszSource)
{
    size_t i = 0;

    if (!pszTarget || !cchTarget || !pszSource)
        return FALSE;

    while (i < cchTarget && pszTarget[i])
        i++;
    if (i == cchTarget)
        return FALSE;

    return CopyWideString(pszTarget + i, cchTarget - i, pszSource);
}

typedef struct tagTBDSETTINGS
{
    int iPrograms;
    int iItems;
    int iMode;
} TBDSETTINGS;

typedef enum tagSTARTMENU_SETTING_KIND
{
    StartSettingCheckbox,
    StartSettingGroup
} STARTMENU_SETTING_KIND;

typedef enum tagSTARTMENU_OPTION_KIND
{
    StartOptionCheckbox,
    StartOptionRadio
} STARTMENU_OPTION_KIND;

typedef enum tagSTARTMENU_NODE_KIND
{
    StartNodeGroup,
    StartNodeCheckbox,
    StartNodeRadio
} STARTMENU_NODE_KIND;

typedef struct tagSTARTMENU_POLICY
{
    PCWSTR pszName;
    PCWSTR pszRegKey;
} STARTMENU_POLICY;

typedef struct tagSTARTMENU_OPTION
{
    STARTMENU_OPTION_KIND kind;
    PCWSTR pszKey;
    PCWSTR pszText;
    PCWSTR pszRegPath;
    PCWSTR pszValueName;
    DWORD dwCheckedValue;
    DWORD dwUncheckedValue;
    DWORD dwDefaultValue;
} STARTMENU_OPTION;

typedef struct tagSTARTMENU_SETTING
{
    STARTMENU_SETTING_KIND kind;
    PCWSTR pszKey;
    PCWSTR pszText;
    PCWSTR pszBitmap;
    PCWSTR pszRegPath;
    PCWSTR pszValueName;
    DWORD dwCheckedValue;
    DWORD dwUncheckedValue;
    DWORD dwDefaultValue;
    const STARTMENU_OPTION *pOptions;
    UINT cOptions;
    const STARTMENU_POLICY *pPolicies;
    UINT cPolicies;
} STARTMENU_SETTING;

typedef struct tagSTARTMENU_NODE
{
    STARTMENU_NODE_KIND kind;
    UINT iSetting;
    UINT iOption;
    BOOL bRestricted;
    HICON hIcon;
} STARTMENU_NODE;

typedef struct tagSTARTMENU7STATE
{
    HWND hDlg;
    HWND hParent;
    HWND hTree;
    HIMAGELIST hStateImages;
    HIMAGELIST hIcons;
    DWORD dwValues[32];
    BOOL bRestricted[32];
    HTREEITEM hRootItems[32];
    HTREEITEM hOptionItems[32][3];
    STARTMENU_NODE nodes[96];
    UINT cNodes;
    UINT cPrograms;
    UINT cItems;
} STARTMENU7STATE;

static TBDSETTINGS g_oldSettings;
static TBDSETTINGS g_newSettings;
static HWND g_hDlg;
static STARTMENU7STATE g_startMenu7;

static const STARTMENU_POLICY g_ControlPanelPolicies[] = {
    { L"NoControlPanel", NULL },
    { L"NoSetFolders", NULL },
};

static const STARTMENU_OPTION g_ControlPanelOptions[] = {
    { StartOptionRadio, L"ControlPanel\\Open", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowControlPanel", 0x00000001, 0, 0x00000001 },
    { StartOptionRadio, L"ControlPanel\\Menu", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowControlPanel", 0x00000002, 0, 0x00000001 },
    { StartOptionRadio, L"ControlPanel\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowControlPanel", 0x00000000, 0, 0x00000001 },
};

static const STARTMENU_POLICY g_DownloadsPolicies[] = {
    { L"NoStartMenuDownloads", L"Software\\Policies\\Microsoft\\Windows\\Explorer" },
};

static const STARTMENU_OPTION g_DownloadsOptions[] = {
    { StartOptionRadio, L"Downloads\\OpenFolder", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowDownloads", 0x00000001, 0, 0x00000000 },
    { StartOptionRadio, L"Downloads\\MenuOfFolder", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowDownloads", 0x00000002, 0, 0x00000000 },
    { StartOptionRadio, L"Downloads\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowDownloads", 0x00000000, 0, 0x00000000 },
};

static const STARTMENU_POLICY g_EnableDragDropPolicies[] = {
    { L"NoChangeStartMenu", NULL },
};

static const STARTMENU_POLICY g_FavoritesPolicies[] = {
    { L"NoFavoritesMenu", NULL },
};

static const STARTMENU_POLICY g_HomegroupPolicies[] = {
    { L"ForceHomegroupOnStartMenu", L"Software\\Policies\\Microsoft\\Windows\\Explorer" },
    { L"NoStartMenuHomegroup", L"Software\\Policies\\Microsoft\\Windows\\Explorer" },
};

static const STARTMENU_POLICY g_MyCompPolicies[] = {
    { L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\NonEnum" },
};

static const STARTMENU_OPTION g_MyCompOptions[] = {
    { StartOptionRadio, L"MyComp\\Open", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyComputer", 0x00000001, 0, 0x00000001 },
    { StartOptionRadio, L"MyComp\\Menu", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyComputer", 0x00000002, 0, 0x00000001 },
    { StartOptionRadio, L"MyComp\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyComputer", 0x00000000, 0, 0x00000001 },
};

static const STARTMENU_POLICY g_MyDocsPolicies[] = {
    { L"NoSMMyDocs", NULL },
};

static const STARTMENU_OPTION g_MyDocsOptions[] = {
    { StartOptionRadio, L"MyDocs\\OpenFolder", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyDocs", 0x00000001, 0, 0x00000001 },
    { StartOptionRadio, L"MyDocs\\MenuOfFolder", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyDocs", 0x00000002, 0, 0x00000001 },
    { StartOptionRadio, L"MyDocs\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyDocs", 0x00000000, 0, 0x00000001 },
};

static const STARTMENU_POLICY g_MyGamesPolicies[] = {
    { L"NoStartMenuMyGames", NULL },
};

static const STARTMENU_OPTION g_MyGamesOptions[] = {
    { StartOptionRadio, L"MyGames\\Open", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyGames", 0x00000001, 0, 0x00000001 },
    { StartOptionRadio, L"MyGames\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyGames", 0x00000000, 0, 0x00000001 },
    { StartOptionRadio, L"MyGames\\Menu", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyGames", 0x00000002, 0, 0x00000001 },
};

static const STARTMENU_POLICY g_MyMusicPolicies[] = {
    { L"NoStartMenuMyMusic", NULL },
    { L"{B5FF6591-8776-42A2-A704-2562C7AA5A3F}", NULL },
};

static const STARTMENU_OPTION g_MyMusicOptions[] = {
    { StartOptionRadio, L"MyMusic\\OpenFolder", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyMusic", 0x00000001, 0, 0x00000001 },
    { StartOptionRadio, L"MyMusic\\MenuOfFolder", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyMusic", 0x00000002, 0, 0x00000001 },
    { StartOptionRadio, L"MyMusic\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyMusic", 0x00000000, 0, 0x00000001 },
};

static const STARTMENU_POLICY g_MyPicsPolicies[] = {
    { L"NoSMMyPictures", NULL },
    { L"{E098BCD5-7A3C-456F-B143-84DF65C12337}", NULL },
};

static const STARTMENU_OPTION g_MyPicsOptions[] = {
    { StartOptionRadio, L"MyPics\\OpenFolder", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyPics", 0x00000001, 0, 0x00000001 },
    { StartOptionRadio, L"MyPics\\MenuOfFolder", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyPics", 0x00000002, 0, 0x00000001 },
    { StartOptionRadio, L"MyPics\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowMyPics", 0x00000000, 0, 0x00000001 },
};

static const STARTMENU_POLICY g_NetConnPolicies[] = {
    { L"NoNetworkConnections", NULL },
    { L"NoSetFolders", NULL },
};

static const STARTMENU_POLICY g_RecentItemsPolicies[] = {
    { L"NoRecentDocsMenu", NULL },
};

static const STARTMENU_POLICY g_RecordedTVPolicies[] = {
    { L"NoStartMenuRecordedTV", L"Software\\Policies\\Microsoft\\Windows\\Explorer" },
};

static const STARTMENU_OPTION g_RecordedTVOptions[] = {
    { StartOptionRadio, L"RecordedTV\\OpenFolder", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowRecordedTV", 0x00000001, 0, 0x00000000 },
    { StartOptionRadio, L"RecordedTV\\MenuOfFolder", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowRecordedTV", 0x00000002, 0, 0x00000000 },
    { StartOptionRadio, L"RecordedTV\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowRecordedTV", 0x00000000, 0, 0x00000000 },
};

static const STARTMENU_POLICY g_SearchFilesPolicies[] = {
    { L"NoStartMenuSearchFiles", NULL },
};

static const STARTMENU_OPTION g_SearchFilesOptions[] = {
    { StartOptionRadio, L"SearchFiles\\NoSearch", L"@shell32.dll,-30565", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_SearchFiles", 0x00000000, 0, 0x00000002 },
    { StartOptionRadio, L"SearchFiles\\FullIndex", L"@shell32.dll,-30566", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_SearchFiles", 0x00000002, 0, 0x00000002 },
    { StartOptionRadio, L"SearchFiles\\UserOnly", L"@shell32.dll,-30567", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_SearchFiles", 0x00000001, 0, 0x00000002 },
};

static const STARTMENU_POLICY g_SearchProgramsPolicies[] = {
    { L"NoStartMenuSearchPrograms", NULL },
};

static const STARTMENU_OPTION g_ShowAdminToolsOptions[] = {
    { StartOptionRadio, L"ShowAdminTools\\Menu", L"@shell32.dll,-30479", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_AdminToolsTemp", 0x00000001, 0, 0x00000000 },
    { StartOptionRadio, L"ShowAdminTools\\Both", L"@shell32.dll,-30478", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_AdminToolsTemp", 0x00000002, 0, 0x00000000 },
    { StartOptionRadio, L"ShowAdminTools\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_AdminToolsTemp", 0x00000000, 0, 0x00000000 },
};

static const STARTMENU_POLICY g_ShowHelpPolicies[] = {
    { L"NoSMHelp", NULL },
};

static const STARTMENU_POLICY g_ShowNetPlacesPolicies[] = {
    { L"NoStartMenuNetworkPlaces", NULL },
};

static const STARTMENU_POLICY g_ShowPrintersPolicies[] = {
    { L"NoSetFolders", NULL },
};

static const STARTMENU_POLICY g_ShowRunPolicies[] = {
    { L"NoRun", NULL },
};

static const STARTMENU_POLICY g_ShowSetProgramAccessAndDefaultsPolicies[] = {
    { L"NoSMConfigurePrograms", NULL },
};

static const STARTMENU_POLICY g_UserPolicies[] = {
    { L"NoUserFolderInStartMenu", NULL },
};

static const STARTMENU_OPTION g_UserOptions[] = {
    { StartOptionRadio, L"User\\OpenFolder", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowUser", 0x00000001, 0, 0x00000001 },
    { StartOptionRadio, L"User\\MenuOfFolder", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowUser", 0x00000002, 0, 0x00000001 },
    { StartOptionRadio, L"User\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowUser", 0x00000000, 0, 0x00000001 },
};

static const STARTMENU_POLICY g_VideosPolicies[] = {
    { L"NoStartMenuVideos", L"Software\\Policies\\Microsoft\\Windows\\Explorer" },
};

static const STARTMENU_OPTION g_VideosOptions[] = {
    { StartOptionRadio, L"Videos\\OpenFolder", L"@shell32.dll,-30593", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowVideos", 0x00000001, 0, 0x00000000 },
    { StartOptionRadio, L"Videos\\Hide", L"@shell32.dll,-30492", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowVideos", 0x00000000, 0, 0x00000000 },
    { StartOptionRadio, L"Videos\\MenuOfFolder", L"@shell32.dll,-30594", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowVideos", 0x00000002, 0, 0x00000000 },
};

static const STARTMENU_SETTING g_StartMenuSettings[] = {
    /* Computer */
    { StartSettingGroup, L"MyComp", L"@shell32.dll,-30480", L"%SystemRoot%\\system32\\imageres.dll,104", NULL, NULL, 0, 0, 0, g_MyCompOptions, ARRAYSIZE(g_MyCompOptions), g_MyCompPolicies, ARRAYSIZE(g_MyCompPolicies) }, 
    /* Connect to */
    { StartSettingCheckbox, L"NetConn", L"@%SystemRoot%\\system32\\van.dll,-2400", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowNetConn", 0x00000001, 0x00000000, 0x00000000, NULL, 0, g_NetConnPolicies, ARRAYSIZE(g_NetConnPolicies) },
    /* Control Panel */
    { StartSettingGroup, L"ControlPanel", L"@shell32.dll,-30488", L"%SystemRoot%\\System32\\imageres.dll,22", NULL, NULL, 0, 0, 0, g_ControlPanelOptions, ARRAYSIZE(g_ControlPanelOptions), g_ControlPanelPolicies, ARRAYSIZE(g_ControlPanelPolicies) },
    /* sud.dll */
    { StartSettingCheckbox, L"ShowSetProgramAccessAndDefaults", L"@sud.dll,-1", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowSetProgramAccessAndDefaults", 0x00000001, 0x00000000, 0x00000001, NULL, 0, g_ShowSetProgramAccessAndDefaultsPolicies, ARRAYSIZE(g_ShowSetProgramAccessAndDefaultsPolicies) },
    /* Debices and Printers */
    { StartSettingCheckbox, L"ShowPrinters", L"@shell32.dll,-30493", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowPrinters", 0x00000001, 0x00000000, 0x00000001, NULL, 0, g_ShowPrintersPolicies, ARRAYSIZE(g_ShowPrintersPolicies) },
    /* Documents */
    { StartSettingGroup, L"MyDocs", L"@shell32.dll,-30485", L"%SystemRoot%\\System32\\imageres.dll,189", NULL, NULL, 0, 0, 0, g_MyDocsOptions, ARRAYSIZE(g_MyDocsOptions), g_MyDocsPolicies, ARRAYSIZE(g_MyDocsPolicies) },
    /* Downloads */
    { StartSettingGroup, L"Downloads", L"@shell32.dll,-30603", L"%SystemRoot%\\System32\\imageres.dll,175", NULL, NULL, 0, 0, 0, g_DownloadsOptions, ARRAYSIZE(g_DownloadsOptions), g_DownloadsPolicies, ARRAYSIZE(g_DownloadsPolicies) },
    /* Drag and Drop */
    { StartSettingCheckbox, L"EnableDragDrop", L"@shell32.dll,-30475", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_EnableDragDrop", 0x00000001, 0x00000000, 0x00000001, NULL, 0, g_EnableDragDropPolicies, ARRAYSIZE(g_EnableDragDropPolicies) },
    /* Favorites */
    { StartSettingCheckbox, L"Favorites", L"@shell32.dll,-30484", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"StartMenuFavorites", 0x00000001, 0x00000000, 0x00000000, NULL, 0, g_FavoritesPolicies, ARRAYSIZE(g_FavoritesPolicies) },
    /* Games */
    { StartSettingGroup, L"MyGames", L"@shell32.dll,-30579", L"%SystemRoot%\\System32\\imageres.dll,10", NULL, NULL, 0, 0, 0, g_MyGamesOptions, ARRAYSIZE(g_MyGamesOptions), g_MyGamesPolicies, ARRAYSIZE(g_MyGamesPolicies) },
    /* Help and Support */
    { StartSettingCheckbox, L"ShowHelp", L"@shell32.dll,-30489", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowHelp", 0x00000001, 0x00000000, 0x00000001, NULL, 0, g_ShowHelpPolicies, ARRAYSIZE(g_ShowHelpPolicies) },
    /* Notify about new apps */
    { StartSettingCheckbox, L"NotifyNew", L"@shell32.dll,-30574", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_NotifyNewApps", 0x00000001, 0x00000000, 0x00000001, NULL, 0, NULL, 0 },
    /* Homegroup */
    { StartSettingCheckbox, L"Homegroup", L"@shell32.dll,-30604", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowHomegroup", 0x00000001, 0x00000000, 0x00000000, NULL, 0, g_HomegroupPolicies, ARRAYSIZE(g_HomegroupPolicies) },
    /* Music */
    { StartSettingGroup, L"MyMusic", L"@shell32.dll,-30487", L"%SystemRoot%\\System32\\imageres.dll,191", NULL, NULL, 0, 0, 0, g_MyMusicOptions, ARRAYSIZE(g_MyMusicOptions), g_MyMusicPolicies, ARRAYSIZE(g_MyMusicPolicies) },
    /* Network Places */
    { StartSettingCheckbox, L"ShowNetPlaces", L"@shell32.dll,-30481", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowNetPlaces", 0x00000001, 0x00000000, 0x00000000, NULL, 0, g_ShowNetPlacesPolicies, ARRAYSIZE(g_ShowNetPlacesPolicies) },
    /* Hover (I have 0 idea what this does) */
    { StartSettingCheckbox, L"HoverOpen", L"@shell32.dll,-30573", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_AutoCascade", 0x00000001, 0x00000000, 0x00000001, NULL, 0, NULL, 0 },
    /* User Account */
    { StartSettingGroup, L"User", L"@shell32.dll,-30497", L"%SystemRoot%\\system32\\imageres.dll,117", NULL, NULL, 0, 0, 0, g_UserOptions, ARRAYSIZE(g_UserOptions), g_UserPolicies, ARRAYSIZE(g_UserPolicies) },
    /* Pictures */
    { StartSettingGroup, L"MyPics", L"@shell32.dll,-30486", L"%SystemRoot%\\System32\\imageres.dll,190", NULL, NULL, 0, 0, 0, g_MyPicsOptions, ARRAYSIZE(g_MyPicsOptions), g_MyPicsPolicies, ARRAYSIZE(g_MyPicsPolicies) },
    /* Recent Items */
    { StartSettingCheckbox, L"RecentItems", L"@shell32.dll,-30607", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowRecentDocs", 0x00000001, 0x00000000, 0x00000000, NULL, 0, g_RecentItemsPolicies, ARRAYSIZE(g_RecentItemsPolicies) },
    /* Recorded TV */
    { StartSettingGroup, L"RecordedTV", L"@shell32.dll,-30605", L"%SystemRoot%\\System32\\imageres.dll,193", NULL, NULL, 0, 0, 0, g_RecordedTVOptions, ARRAYSIZE(g_RecordedTVOptions), g_RecordedTVPolicies, ARRAYSIZE(g_RecordedTVPolicies) },
    /* Run */
    { StartSettingCheckbox, L"ShowRun", L"@shell32.dll,-30483", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_ShowRun", 0x00000001, 0x00000000, 0x00000000, NULL, 0, g_ShowRunPolicies, ARRAYSIZE(g_ShowRunPolicies) },
    /* Search Files ? */
    { StartSettingGroup, L"SearchFiles", L"@shell32.dll,-30576", L"%SystemRoot%\\System32\\shell32.dll,235", NULL, NULL, 0, 0, 0, g_SearchFilesOptions, ARRAYSIZE(g_SearchFilesOptions), g_SearchFilesPolicies, ARRAYSIZE(g_SearchFilesPolicies) },
    /* Search ? */
    { StartSettingCheckbox, L"SearchPrograms", L"@shell32.dll,-30569", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_SearchPrograms", 0x00000001, 0x00000000, 0x00000001, NULL, 0, g_SearchProgramsPolicies, ARRAYSIZE(g_SearchProgramsPolicies) },
    /* Sort by Name */
    { StartSettingCheckbox, L"SortByName", L"@shell32.dll,-30571", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_SortByName", 0x00000001, 0x00000000, 0x00000001, NULL, 0, NULL, 0 },
    /* Admin tools */
    { StartSettingGroup, L"ShowAdminTools", L"@shell32.dll,-30515", L"%SystemRoot%\\System32\\main.cpl,500", NULL, NULL, 0, 0, 0, g_ShowAdminToolsOptions, ARRAYSIZE(g_ShowAdminToolsOptions), NULL, 0 },
    /* Use large icons */
    { StartSettingCheckbox, L"UseLargeIcons", L"@shell32.dll,-30572", NULL, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Start_LargeMFUIcons", 0x00000001, 0x00000000, 0x00000001, NULL, 0, NULL, 0 },
    /* Videos */
    { StartSettingGroup, L"Videos", L"@shell32.dll,-30606", L"%SystemRoot%\\System32\\imageres.dll,192", NULL, NULL, 0, 0, 0, g_VideosOptions, ARRAYSIZE(g_VideosOptions), g_VideosPolicies, ARRAYSIZE(g_VideosPolicies) },
};

enum
{
    StartMenuStateCheckboxClear = 1,
    StartMenuStateCheckboxChecked,
    StartMenuStateRadioClear,
    StartMenuStateRadioChecked,
    StartMenuStateCheckboxDisabledClear,
    StartMenuStateCheckboxDisabledChecked,
    StartMenuStateRadioDisabledClear,
    StartMenuStateRadioDisabledChecked,
};

static
BOOL IsStartMenuAdminToolsSetting(UINT iSetting)
{
    return lstrcmpiW(g_StartMenuSettings[iSetting].pszKey, L"ShowAdminTools") == 0;
}

static
DWORD ReadDwordWithDefault(HKEY hRoot, PCWSTR pszRegPath, PCWSTR pszValueName,
    DWORD dwDefaultValue)
{
    DWORD dwValue;
    DWORD dwType;
    DWORD dwSize = sizeof(DWORD);

    LSTATUS status = RegGetValueW(hRoot, pszRegPath, pszValueName,
        RRF_RT_REG_DWORD, &dwType, &dwValue, &dwSize);
    if (status == ERROR_SUCCESS && dwType == REG_DWORD)
        return dwValue;

    return dwDefaultValue;
}

static
DWORD ReadDwordWithDefaultBounded(HKEY hRoot, PCWSTR pszRegPath, PCWSTR pszValueName,
    DWORD dwDefaultValue, DWORD dwMinValue, DWORD dwMaxValue)
{
    DWORD dwValue = ReadDwordWithDefault(hRoot, pszRegPath, pszValueName,
        dwDefaultValue);
    if (dwValue < dwMinValue)
        return dwMinValue;
    if (dwValue > dwMaxValue)
        return dwMaxValue;
    return dwValue;
}

static
BOOL WriteDwordValue(HKEY hRoot, PCWSTR pszRegPath, PCWSTR pszValueName,
    DWORD dwValue)
{
    HKEY hKey;
    LSTATUS status = RegCreateKeyExW(hRoot, pszRegPath, 0, NULL, 0,
        KEY_SET_VALUE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS)
        return FALSE;

    status = RegSetValueExW(hKey, pszValueName, 0, REG_DWORD,
        (const BYTE *)&dwValue, sizeof(DWORD));
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

static
BOOL RegistryEntryExists(HKEY hRoot, PCWSTR pszRegPath, PCWSTR pszValueName)
{
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, pszRegPath, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return FALSE;

    BOOL bExists;
    if (pszValueName)
    {
        bExists = RegQueryValueExW(hKey, pszValueName, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
    }
    else
    {
        bExists = TRUE;
    }

    RegCloseKey(hKey);
    return bExists;
}

static
BOOL PolicyExistsForBase(PCWSTR pszBasePath, PCWSTR pszName)
{
    if (!pszBasePath || !*pszBasePath || !pszName || !*pszName)
        return FALSE;

    if (RegistryEntryExists(HKEY_CURRENT_USER, pszBasePath, pszName) ||
        RegistryEntryExists(HKEY_LOCAL_MACHINE, pszBasePath, pszName))
        return TRUE;

    WCHAR szPath[MAX_PATH];
    szPath[0] = L'\0';
    if (!CopyWideString(szPath, ARRAYSIZE(szPath), pszBasePath) ||
        !AppendWideString(szPath, ARRAYSIZE(szPath), L"\\") ||
        !AppendWideString(szPath, ARRAYSIZE(szPath), pszName))
        return FALSE;

    return RegistryEntryExists(HKEY_CURRENT_USER, szPath, NULL) ||
        RegistryEntryExists(HKEY_LOCAL_MACHINE, szPath, NULL);
}

static
BOOL StartMenuPolicyExists(const STARTMENU_POLICY *pPolicy)
{
    if (!pPolicy)
        return FALSE;

    if (pPolicy->pszRegKey && *pPolicy->pszRegKey)
        return PolicyExistsForBase(pPolicy->pszRegKey, pPolicy->pszName);

    return PolicyExistsForBase(g_policyExplorerKey, pPolicy->pszName) ||
        PolicyExistsForBase(g_policyExplorerAltKey, pPolicy->pszName);
}

static
BOOL StartMenuSettingRestricted(const STARTMENU_SETTING *pSetting)
{
    UINT iPolicy;

    if (!pSetting)
        return FALSE;

    for (iPolicy = 0; iPolicy < pSetting->cPolicies; iPolicy++)
    {
        if (StartMenuPolicyExists(&pSetting->pPolicies[iPolicy]))
            return TRUE;
    }

    return FALSE;
}

static
DWORD ReadAdminToolsValue(void)
{
    DWORD dwRoot = ReadDwordWithDefault(HKEY_CURRENT_USER, g_explorerKey,
        L"Start_AdminToolsRoot", 0);
    if (dwRoot)
        return 2;

    return ReadDwordWithDefault(HKEY_CURRENT_USER, g_explorerKey,
        L"StartMenuAdminTools", 0) ? 1 : 0;
}

static
BOOL WriteAdminToolsValue(DWORD dwValue)
{
    DWORD dwRoot = (dwValue == 2) ? 2 : 0;
    DWORD dwMenu = (dwValue >= 1) ? 1 : 0;

    {
        LSTATUS status;
        BOOL bOk = WriteDwordValue(HKEY_CURRENT_USER, g_explorerKey,
            L"Start_AdminToolsRoot", dwRoot) &&
            WriteDwordValue(HKEY_CURRENT_USER, g_explorerKey,
                L"StartMenuAdminTools", dwMenu);
        status = RegDeleteKeyValueW(HKEY_CURRENT_USER, g_explorerKey,
            L"Start_AdminToolsTemp");
        return bOk && (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND);
    }
}

static
BOOL LoadIndirectResourceString(PCWSTR pszSource, PWSTR pszTarget, size_t cchTarget)
{
    WCHAR szIndirect[260];

    if (!pszSource || !*pszSource)
        return FALSE;

    if (pszSource[0] == L'@')
    {
        if (!CopyWideString(szIndirect, ARRAYSIZE(szIndirect), pszSource))
            return FALSE;
    }
    else if (WideStrChr(pszSource, L','))
    {
        szIndirect[0] = L'@';
        szIndirect[1] = L'\0';
        if (!AppendWideString(szIndirect, ARRAYSIZE(szIndirect), pszSource))
            return FALSE;
    }
    else
    {
        return CopyWideString(pszTarget, cchTarget, pszSource);
    }

    if (SUCCEEDED(SHLoadIndirectString(szIndirect, pszTarget,
        (UINT)cchTarget, NULL)) && *pszTarget)
        return TRUE;

    return CopyWideString(pszTarget, cchTarget, pszSource);
}

static
BOOL LoadSettingIcon(PCWSTR pszBitmap, HICON *phIcon)
{
    WCHAR szPath[MAX_PATH];
    WCHAR *pszComma;
    int iIcon;
    HICON hSmall = NULL;

    *phIcon = NULL;
    if (!pszBitmap || !*pszBitmap)
        return FALSE;

    if (!ExpandEnvironmentStringsW(pszBitmap, szPath, ARRAYSIZE(szPath)))
        return FALSE;

    pszComma = WideStrRChr(szPath, L',');
    if (!pszComma)
        return FALSE;

    *pszComma = L'\0';
    iIcon = WideAtoi(pszComma + 1);
    if (ExtractIconExW(szPath, iIcon, NULL, &hSmall, 1) == 0 || !hSmall)
        return FALSE;

    *phIcon = hSmall;
    return TRUE;
}

static
BOOL AddThemeStateImage(HIMAGELIST hImageList, HTHEME hTheme,
    int iPartId, int iStateId)
{
    static const COLORREF crMask = RGB(255, 0, 255);
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap;
    HBITMAP hOldBitmap;
    HBRUSH hBrush = CreateSolidBrush(crMask);
    RECT rc = { 0, 0, 16, 16 };
    RECT rcDraw = rc;
    SIZE size = { 0, 0 };
    BOOL bRet = FALSE;

    hBitmap = CreateCompatibleBitmap(hdcScreen, 16, 16);
    if (!hBitmap)
        goto Cleanup;

    hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    FillRect(hdcMem, &rc, hBrush);
    DeleteObject(hBrush);
    hBrush = NULL;

    if (SUCCEEDED(GetThemePartSize(hTheme, hdcMem, iPartId, iStateId, NULL,
        TS_TRUE, &size)))
    {
        rcDraw.left = (16 - size.cx) / 2;
        rcDraw.top = (16 - size.cy) / 2;
        rcDraw.right = rcDraw.left + size.cx;
        rcDraw.bottom = rcDraw.top + size.cy;
    }

    if (SUCCEEDED(DrawThemeBackground(hTheme, hdcMem, iPartId, iStateId, &rcDraw, NULL)))
        bRet = ImageList_AddMasked(hImageList, hBitmap, crMask) != -1;
    SelectObject(hdcMem, hOldBitmap);

Cleanup:
    if (hBrush)
        DeleteObject(hBrush);
    if (hBitmap)
        DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    return bRet;
}

static
BOOL CreateStateImageList(STARTMENU7STATE *pState)
{
    HTHEME hTheme;
    BOOL bRet;

    pState->hStateImages = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 8, 0);
    if (!pState->hStateImages)
        return FALSE;

    hTheme = OpenThemeData(g_hDlg, L"Button");
    if (!hTheme)
        return FALSE;

    bRet = AddThemeStateImage(pState->hStateImages, hTheme,
            BP_CHECKBOX, CBS_UNCHECKEDNORMAL) &&
        AddThemeStateImage(pState->hStateImages, hTheme,
            BP_CHECKBOX, CBS_CHECKEDNORMAL) &&
        AddThemeStateImage(pState->hStateImages, hTheme,
            BP_RADIOBUTTON, RBS_UNCHECKEDNORMAL) &&
        AddThemeStateImage(pState->hStateImages, hTheme,
            BP_RADIOBUTTON, RBS_CHECKEDNORMAL) &&
        AddThemeStateImage(pState->hStateImages, hTheme,
            BP_CHECKBOX, CBS_UNCHECKEDDISABLED) &&
        AddThemeStateImage(pState->hStateImages, hTheme,
            BP_CHECKBOX, CBS_CHECKEDDISABLED) &&
        AddThemeStateImage(pState->hStateImages, hTheme,
            BP_RADIOBUTTON, RBS_UNCHECKEDDISABLED) &&
        AddThemeStateImage(pState->hStateImages, hTheme,
            BP_RADIOBUTTON, RBS_CHECKEDDISABLED);
    CloseThemeData(hTheme);
    return bRet;
}

static
void DestroyStartMenuImages(STARTMENU7STATE *pState)
{
    UINT iNode;
    for (iNode = 0; iNode < pState->cNodes; iNode++)
    {
        if (pState->nodes[iNode].hIcon)
        {
            DestroyIcon(pState->nodes[iNode].hIcon);
            pState->nodes[iNode].hIcon = NULL;
        }
    }

    if (pState->hIcons)
    {
        ImageList_Destroy(pState->hIcons);
        pState->hIcons = NULL;
    }

    if (pState->hStateImages)
    {
        ImageList_Destroy(pState->hStateImages);
        pState->hStateImages = NULL;
    }
}

static
int GetStateImageIndex(const STARTMENU_NODE *pNode, BOOL bChecked)
{
    if (pNode->kind == StartNodeRadio)
        return pNode->bRestricted ?
            (bChecked ? StartMenuStateRadioDisabledChecked : StartMenuStateRadioDisabledClear) :
            (bChecked ? StartMenuStateRadioChecked : StartMenuStateRadioClear);

    return pNode->bRestricted ?
        (bChecked ? StartMenuStateCheckboxDisabledChecked : StartMenuStateCheckboxDisabledClear) :
        (bChecked ? StartMenuStateCheckboxChecked : StartMenuStateCheckboxClear);
}

static
void SetTreeItemStateImage(HWND hTree, HTREEITEM hItem, int iImage)
{
    TreeView_SetItemState(hTree, hItem, INDEXTOSTATEIMAGEMASK(iImage),
        TVIS_STATEIMAGEMASK);
}

static
void LoadLegacyRegSettings(void)
{
    HKEY hKey;
    DWORD dwType;
    DWORD dwData = 0;
    DWORD dwSize;
    LSTATUS status;

#define ReadDword(valueName) \
    dwSize = sizeof(DWORD); \
    status = RegQueryValueEx(hKey, valueName, 0, &dwType, \
        (BYTE *)&dwData, &dwSize)

#define ReadInt(valueName, member) \
    ReadDword(valueName); \
    if (status == ERROR_SUCCESS && dwType == REG_DWORD) \
        g_oldSettings.member = (int)dwData

    status = RegOpenKeyEx(HKEY_CURRENT_USER, g_explorerPatcherKey, 0,
        KEY_QUERY_VALUE, &hKey);
    if (status == ERROR_SUCCESS)
    {
        ReadInt(TEXT("StartUI_EnableRoundedCorners"), iMode);
        RegCloseKey(hKey);
    }

    status = RegOpenKeyEx(HKEY_CURRENT_USER, g_explorerKey, 0,
        KEY_QUERY_VALUE, &hKey);
    if (status == ERROR_SUCCESS)
    {
        ReadInt(TEXT("Start_MinMFU"), iPrograms);
        ReadInt(TEXT("Start_JumpListItems"), iItems);
        RegCloseKey(hKey);
        if (g_oldSettings.iItems > STARTMENU_JUMPLIST_ITEMS_MAX)
            g_oldSettings.iItems = STARTMENU_JUMPLIST_ITEMS_MAX;
    }

#undef ReadInt
#undef ReadDword
}

static
void LoadLegacyDefaultSettings(void)
{
    g_oldSettings.iMode = 0;
    g_oldSettings.iPrograms = STARTMENU_RECENT_PROGRAMS_DEFAULT;
    g_oldSettings.iItems = STARTMENU_JUMPLIST_ITEMS_DEFAULT;
}

static
void LoadLegacySettings(void)
{
    LoadLegacyDefaultSettings();
    LoadLegacyRegSettings();
    g_newSettings = g_oldSettings;
}

static
void SetRanges(void)
{
    SendDlgItemMessage(g_hDlg, IDC_SM_MFU_PROGRAMS_SPIN,
        UDM_SETRANGE, 0L, MAKELONG(STARTMENU_RECENT_PROGRAMS_MAX, 0));
    SendDlgItemMessage(g_hDlg, IDC_SM_MFU_ITEMS_SPIN,
        UDM_SETRANGE, 0L, MAKELONG(STARTMENU_JUMPLIST_ITEMS_MAX, 0));
}

static
void InitComboBoxes(void)
{
    int iElement;
    TCHAR text[60] = TEXT("\0");

#define InitCombo(iControl, iString, nElements) \
    for (iElement = 0; iElement < nElements; iElement++) { \
        LoadString(g_propSheet.hInstance, iString + iElement, \
            (TCHAR *)&text, 59); \
        SendDlgItemMessage(g_hDlg, iControl, CB_ADDSTRING, 0L, (LPARAM)&text); \
    }

    InitCombo(IDC_SM_10DLG_MODE, IDS_SM_10DLG_MODE_DEFAULT, 3);

#undef InitCombo
}

static
void UpdateLegacyControls(void)
{
    SetComboIndex(IDC_SM_10DLG_MODE, g_oldSettings.iMode);
    SendDlgItemMessage(g_hDlg, IDC_SM_MFU_PROGRAMS_SPIN,
        UDM_SETPOS, 0L, (LPARAM)g_oldSettings.iPrograms);
    SendDlgItemMessage(g_hDlg, IDC_SM_MFU_ITEMS_SPIN,
        UDM_SETPOS, 0L, (LPARAM)g_oldSettings.iItems);
}

static
void InitLegacyPage(void)
{
    LoadLegacySettings();
    InitComboBoxes();
    UpdateLegacyControls();
    SetRanges();
}

static
BOOL WriteLegacyRegSettings(void)
{
    HKEY hKey;
    BOOL ret = TRUE;
    DWORD dwData;
    LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, g_explorerPatcherKey, 0,
        NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);

    if (status == ERROR_SUCCESS)
    {
        dwData = (DWORD)g_newSettings.iMode;
        if (RegSetValueEx(hKey, TEXT("StartUI_EnableRoundedCorners"), 0,
            REG_DWORD, (BYTE *)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
        {
            ret = FALSE;
        }
        RegCloseKey(hKey);
    }
    else
    {
        ret = FALSE;
    }

    status = RegCreateKeyEx(HKEY_CURRENT_USER, g_explorerKey, 0, NULL, 0,
        KEY_SET_VALUE, NULL, &hKey, NULL);
    if (status == ERROR_SUCCESS)
    {
        dwData = (DWORD)g_newSettings.iPrograms;
        if (dwData > STARTMENU_RECENT_PROGRAMS_MAX)
            dwData = STARTMENU_RECENT_PROGRAMS_MAX;
        if (RegSetValueEx(hKey, TEXT("Start_MinMFU"), 0,
            REG_DWORD, (BYTE *)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
            ret = FALSE;

        dwData = (DWORD)g_newSettings.iItems;
        if (dwData > STARTMENU_JUMPLIST_ITEMS_MAX)
            dwData = STARTMENU_JUMPLIST_ITEMS_MAX;
        if (RegSetValueEx(hKey, TEXT("Start_JumpListItems"), 0,
            REG_DWORD, (BYTE *)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
            ret = FALSE;

        RegCloseKey(hKey);
    }
    else
    {
        ret = FALSE;
    }

    return ret;
}

static
BOOL ReadTreeItemNode(HWND hTree, HTREEITEM hItem, STARTMENU_NODE **ppNode)
{
    TVITEMEX item;

    *ppNode = NULL;
    ZeroMemory(&item, sizeof(item));
    item.mask = TVIF_HANDLE | TVIF_PARAM;
    item.hItem = hItem;
    if (!TreeView_GetItem(hTree, &item))
        return FALSE;

    *ppNode = (STARTMENU_NODE *)item.lParam;
    return *ppNode != NULL;
}

static
void DrawStartMenuNodeImage(STARTMENU7STATE *pState,
    NMTVCUSTOMDRAW *pCustomDraw)
{
    STARTMENU_NODE *pNode;
    RECT rcText;
    RECT rcItem;
    RECT rcIcon;
    RECT rcFill;
    RECT rcFocus;
    RECT rcMeasure;
    TVITEMW item;
    WCHAR szText[256] = L"";
    HFONT hFont;
    HFONT hOldFont;
    COLORREF crTextOld;
    COLORREF crBkOld;
    COLORREF crIconBk;
    int cxImage;
    int cyImage;
    int x;
    int y;

    if (!ReadTreeItemNode(pState->hTree, (HTREEITEM)pCustomDraw->nmcd.dwItemSpec, &pNode) ||
        !pNode->hIcon)
        return;

    rcText.left = 0;
    rcText.top = 0;
    rcText.right = 0;
    rcText.bottom = 0;
    if (!TreeView_GetItemRect(pState->hTree, (HTREEITEM)pCustomDraw->nmcd.dwItemSpec,
        &rcText, TRUE))
        return;
    rcItem = rcText;
    if (!TreeView_GetItemRect(pState->hTree, (HTREEITEM)pCustomDraw->nmcd.dwItemSpec,
        &rcItem, FALSE))
        return;

    ZeroMemory(&item, sizeof(item));
    item.mask = TVIF_HANDLE | TVIF_TEXT;
    item.hItem = (HTREEITEM)pCustomDraw->nmcd.dwItemSpec;
    item.pszText = szText;
    item.cchTextMax = ARRAYSIZE(szText);
    if (!TreeView_GetItem(pState->hTree, &item))
        return;

    cxImage = GetSystemMetrics(SM_CXSMICON);
    cyImage = GetSystemMetrics(SM_CYSMICON);
    crTextOld = SetTextColor(pCustomDraw->nmcd.hdc, pCustomDraw->clrText);
    crBkOld = SetBkColor(pCustomDraw->nmcd.hdc, pCustomDraw->clrTextBk);
    crIconBk = (COLORREF)SendMessage(pState->hTree, TVM_GETBKCOLOR, 0L, 0L);
    if (crIconBk == CLR_NONE)
        crIconBk = pCustomDraw->clrTextBk;

    hFont = (HFONT)SendMessage(pState->hTree, WM_GETFONT, 0L, 0L);
    hOldFont = hFont ? SelectObject(pCustomDraw->nmcd.hdc, hFont) : NULL;

    rcIcon = rcText;
    rcIcon.right = rcIcon.left + cxImage + 2;
    rcMeasure = rcText;
    rcMeasure.left = rcIcon.right;
    rcMeasure.right = rcItem.right;
    DrawTextW(pCustomDraw->nmcd.hdc, szText, -1, &rcMeasure,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_CALCRECT);
    if (rcMeasure.right > rcItem.right)
        rcMeasure.right = rcItem.right;

    SetBkColor(pCustomDraw->nmcd.hdc, crIconBk);
    ExtTextOutW(pCustomDraw->nmcd.hdc, 0, 0, ETO_OPAQUE, &rcIcon, NULL, 0, NULL);

    rcFill = rcText;
    rcFill.left = rcIcon.right;
    rcFill.right = rcMeasure.right;
    if (pCustomDraw->nmcd.uItemState & CDIS_SELECTED)
    {
        rcFill.left -= 2;
        rcFill.right += 3;
        if (rcFill.right > rcItem.right)
            rcFill.right = rcItem.right;
    }
    SetBkColor(pCustomDraw->nmcd.hdc, pCustomDraw->clrTextBk);
    ExtTextOutW(pCustomDraw->nmcd.hdc, 0, 0, ETO_OPAQUE, &rcFill, NULL, 0, NULL);

    x = rcText.left;
    y = rcText.top + ((rcText.bottom - rcText.top) - cyImage) / 2;
    DrawIconEx(pCustomDraw->nmcd.hdc, x, y, pNode->hIcon, cxImage, cyImage,
        0, NULL, DI_NORMAL);

    SetBkMode(pCustomDraw->nmcd.hdc, TRANSPARENT);
    rcText.left = rcIcon.right;
    rcText.right = rcMeasure.right;
    DrawTextW(pCustomDraw->nmcd.hdc, szText, -1, &rcText,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    if (pCustomDraw->nmcd.uItemState & CDIS_FOCUS)
    {
        rcFocus = rcFill;
        DrawFocusRect(pCustomDraw->nmcd.hdc, &rcFocus);
    }
    if (hOldFont)
        SelectObject(pCustomDraw->nmcd.hdc, hOldFont);
    SetTextColor(pCustomDraw->nmcd.hdc, crTextOld);
    SetBkColor(pCustomDraw->nmcd.hdc, crBkOld);
}

static
BOOL StartMenuHitCustomContent(STARTMENU7STATE *pState, HTREEITEM hItem, POINT pt)
{
    STARTMENU_NODE *pNode;
    RECT rcText;
    RECT rcItem;
    RECT rcMeasure;
    RECT rcHit;
    TVITEMW item;
    WCHAR szText[256] = L"";
    HDC hdc;
    HFONT hFont;
    HFONT hOldFont;
    int cxImage;

    if (!ReadTreeItemNode(pState->hTree, hItem, &pNode))
        return FALSE;

    rcText.left = 0;
    rcText.top = 0;
    rcText.right = 0;
    rcText.bottom = 0;
    if (!TreeView_GetItemRect(pState->hTree, hItem, &rcText, TRUE))
        return FALSE;
    rcItem = rcText;
    if (!TreeView_GetItemRect(pState->hTree, hItem, &rcItem, FALSE))
        return FALSE;

    ZeroMemory(&item, sizeof(item));
    item.mask = TVIF_HANDLE | TVIF_TEXT;
    item.hItem = hItem;
    item.pszText = szText;
    item.cchTextMax = ARRAYSIZE(szText);
    if (!TreeView_GetItem(pState->hTree, &item))
        return FALSE;

    hdc = GetDC(pState->hTree);
    if (!hdc)
        return FALSE;
    hFont = (HFONT)SendMessage(pState->hTree, WM_GETFONT, 0L, 0L);
    hOldFont = hFont ? SelectObject(hdc, hFont) : NULL;

    cxImage = 0;
    rcMeasure = rcText;
    if (pNode->hIcon)
    {
        cxImage = GetSystemMetrics(SM_CXSMICON) + 2;
        rcMeasure.left += cxImage;
    }
    rcMeasure.right = rcItem.right;
    DrawTextW(hdc, szText, -1, &rcMeasure,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_CALCRECT);
    if (hOldFont)
        SelectObject(hdc, hOldFont);
    ReleaseDC(pState->hTree, hdc);

    if (rcMeasure.right > rcItem.right)
        rcMeasure.right = rcItem.right;
    rcHit = rcMeasure;
    if (pNode->hIcon)
        rcHit.left -= cxImage;
    return PtInRect(&rcHit, pt);
}

static
HTREEITEM StartMenuCustomHitItem(STARTMENU7STATE *pState, POINT pt)
{
    HTREEITEM hItem = TreeView_GetFirstVisible(pState->hTree);
    while (hItem)
    {
        RECT rcItem;
        rcItem.left = 0;
        rcItem.top = 0;
        rcItem.right = 0;
        rcItem.bottom = 0;
        if (TreeView_GetItemRect(pState->hTree, hItem, &rcItem, FALSE) &&
            pt.y >= rcItem.top && pt.y < rcItem.bottom &&
            StartMenuHitCustomContent(pState, hItem, pt))
        {
            return hItem;
        }
        hItem = TreeView_GetNextVisible(pState->hTree, hItem);
    }
    return NULL;
}


static
void ToggleTreeNode(STARTMENU7STATE *pState, HTREEITEM hItem);

static
BOOL HandleStartMenuTreeClick(STARTMENU7STATE *pState, POINT pt)
{
    TVHITTESTINFO hit;
    HTREEITEM hItem;
    STARTMENU_NODE *pNode;
    BOOL bNativeHit;
    BOOL bCustomHit;

    ZeroMemory(&hit, sizeof(hit));
    hit.pt = pt;
    TreeView_HitTest(pState->hTree, &hit);
    hItem = hit.hItem ? hit.hItem :
        StartMenuCustomHitItem(pState, hit.pt);
    pNode = NULL;
    if (hItem)
        ReadTreeItemNode(pState->hTree, hItem, &pNode);
    bNativeHit = hItem == hit.hItem &&
        (hit.flags & TVHT_ONITEMSTATEICON);
    if (pNode && !pNode->hIcon)
    {
        bNativeHit = bNativeHit ||
            (hItem == hit.hItem &&
            (hit.flags & (TVHT_ONITEMICON | TVHT_ONITEMLABEL)));
    }
    bCustomHit = hItem &&
        StartMenuHitCustomContent(pState, hItem, hit.pt);
    if (!hItem || !(bNativeHit || bCustomHit))
        return FALSE;

    TreeView_SelectItem(pState->hTree, hItem);
    SetFocus(pState->hTree);
    ToggleTreeNode(pState, hItem);
    return TRUE;
}

static
LRESULT CALLBACK StartMenuTreeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    STARTMENU7STATE *pState = (STARTMENU7STATE *)dwRefData;

    UNREFERENCED_PARAMETER(uIdSubclass);
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        POINT pt;
        UNREFERENCED_PARAMETER(hWnd);
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        if (pState && HandleStartMenuTreeClick(pState, pt))
            return 0;
        break;
    }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}




static
LRESULT HandleStartMenuTreeCustomDraw(STARTMENU7STATE *pState,
    NMTVCUSTOMDRAW *pCustomDraw)
{
    switch (pCustomDraw->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYPOSTPAINT;
    case CDDS_ITEMPOSTPAINT:
        DrawStartMenuNodeImage(pState, pCustomDraw);
        return CDRF_DODEFAULT;
    default:
        return CDRF_DODEFAULT;
    }
}


static
void InitializeStartMenuDefaults(STARTMENU7STATE *pState)
{
    UINT iSetting;

    for (iSetting = 0; iSetting < ARRAYSIZE(g_StartMenuSettings); iSetting++)
    {
        const STARTMENU_SETTING *pSetting = &g_StartMenuSettings[iSetting];
        if (pSetting->kind == StartSettingCheckbox)
        {
            pState->dwValues[iSetting] = pSetting->dwDefaultValue;
        }
        else if (pSetting->cOptions)
        {
            pState->dwValues[iSetting] = pSetting->pOptions[0].dwDefaultValue;
        }
        else
        {
            pState->dwValues[iSetting] = 0;
        }

        pState->bRestricted[iSetting] = FALSE;
    }

    pState->cPrograms = STARTMENU_RECENT_PROGRAMS_DEFAULT;
    pState->cItems = STARTMENU_JUMPLIST_ITEMS_DEFAULT;
}

static
void LoadStartMenuState(STARTMENU7STATE *pState)
{
    UINT iSetting;

    InitializeStartMenuDefaults(pState);
    pState->cPrograms = ReadDwordWithDefault(HKEY_CURRENT_USER, g_explorerKey,
        L"Start_MinMFU", STARTMENU_RECENT_PROGRAMS_DEFAULT);
    pState->cItems = ReadDwordWithDefaultBounded(HKEY_CURRENT_USER, g_explorerKey,
        L"Start_JumpListItems", STARTMENU_JUMPLIST_ITEMS_DEFAULT, 0,
        STARTMENU_JUMPLIST_ITEMS_MAX);

    for (iSetting = 0; iSetting < ARRAYSIZE(g_StartMenuSettings); iSetting++)
    {
        const STARTMENU_SETTING *pSetting = &g_StartMenuSettings[iSetting];
        pState->bRestricted[iSetting] = StartMenuSettingRestricted(pSetting);

        if (IsStartMenuAdminToolsSetting(iSetting))
        {
            pState->dwValues[iSetting] = ReadAdminToolsValue();
            continue;
        }

        if (pSetting->kind == StartSettingCheckbox)
        {
            pState->dwValues[iSetting] = ReadDwordWithDefault(HKEY_CURRENT_USER,
                pSetting->pszRegPath, pSetting->pszValueName,
                pSetting->dwDefaultValue);
        }
        else if (pSetting->cOptions)
        {
            pState->dwValues[iSetting] = ReadDwordWithDefault(HKEY_CURRENT_USER,
                pSetting->pOptions[0].pszRegPath,
                pSetting->pOptions[0].pszValueName,
                pSetting->pOptions[0].dwDefaultValue);
        }
    }
}

static
PCWSTR ParentCheckboxValueName(int iControl)
{
    switch (iControl)
    {
    case IDC_SM_TRACKPROGS:
        return L"Start_TrackProgs";
    case IDC_SM_TRACKDOCS:
        return L"Start_TrackDocs";
    default:
        return NULL;
    }
}

static
BOOL TryReadParentCheckboxState(HWND hWnd, int iControl, BOOL *pbChecked)
{
    HWND hControl;
    if (!hWnd || !pbChecked)
        return FALSE;

    hControl = GetDlgItem(hWnd, iControl);
    if (!hControl)
        return FALSE;

    *pbChecked = IsDlgButtonChecked(hWnd, iControl) == BST_CHECKED;
    return TRUE;
}

static
BOOL ParentCheckboxChecked(STARTMENU7STATE *pState, int iControl)
{
    PCWSTR pszValueName = ParentCheckboxValueName(iControl);
    HWND hWindows[4];
    BOOL bChecked;
    UINT i;

    if (!pszValueName)
        return TRUE;

    hWindows[0] = pState->hParent;
    hWindows[1] = GetParent(pState->hDlg);
    hWindows[2] = GetWindow(pState->hDlg, GW_OWNER);
    hWindows[3] = GetAncestor(pState->hDlg, GA_ROOTOWNER);
    for (i = 0; i < ARRAYSIZE(hWindows); i++)
    {
        if ((i == 0 || hWindows[i] != hWindows[i - 1]) &&
            TryReadParentCheckboxState(hWindows[i], iControl, &bChecked))
        {
            return bChecked;
        }
    }

    return ReadDwordWithDefault(HKEY_CURRENT_USER, g_explorerKey, pszValueName, TRUE) != 0;
}

static
void UpdateMfuControls(STARTMENU7STATE *pState)
{
    BOOL bTrackPrograms = ParentCheckboxChecked(pState, IDC_SM_TRACKPROGS);
    BOOL bTrackDocs = ParentCheckboxChecked(pState, IDC_SM_TRACKDOCS);

    SetDlgItemInt(pState->hDlg, IDC_SM_MFU_PROGRAMS,
        bTrackPrograms ? pState->cPrograms : 0, FALSE);
    SetDlgItemInt(pState->hDlg, IDC_SM_MFU_ITEMS,
        bTrackDocs ? pState->cItems : 0, FALSE);

    EnableWindow(GetDlgItem(pState->hDlg, IDC_SM_MFU_PROGRAMS), bTrackPrograms);
    EnableWindow(GetDlgItem(pState->hDlg, IDC_SM_MFU_PROGRAMS_SPIN), bTrackPrograms);
    EnableWindow(GetDlgItem(pState->hDlg, IDC_SM_MFU_ITEMS), bTrackDocs);
    EnableWindow(GetDlgItem(pState->hDlg, IDC_SM_MFU_ITEMS_SPIN), bTrackDocs);
}

static
STARTMENU_NODE *AppendStartMenuNode(STARTMENU7STATE *pState,
    STARTMENU_NODE_KIND kind, UINT iSetting, UINT iOption)
{
    if (pState->cNodes >= ARRAYSIZE(pState->nodes))
        return NULL;

    pState->nodes[pState->cNodes].kind = kind;
    pState->nodes[pState->cNodes].iSetting = iSetting;
    pState->nodes[pState->cNodes].iOption = iOption;
    pState->nodes[pState->cNodes].bRestricted = pState->bRestricted[iSetting];
    pState->nodes[pState->cNodes].hIcon = NULL;
    return &pState->nodes[pState->cNodes++];
}

static
BOOL SettingCheckedValue(STARTMENU7STATE *pState, UINT iSetting)
{
    const STARTMENU_SETTING *pSetting = &g_StartMenuSettings[iSetting];
    return pState->dwValues[iSetting] == pSetting->dwCheckedValue;
}

static
BOOL OptionCheckedValue(STARTMENU7STATE *pState, UINT iSetting, UINT iOption)
{
    return pState->dwValues[iSetting] ==
        g_StartMenuSettings[iSetting].pOptions[iOption].dwCheckedValue;
}

static
void RefreshStartMenuSettingUI(STARTMENU7STATE *pState, UINT iSetting)
{
    const STARTMENU_SETTING *pSetting = &g_StartMenuSettings[iSetting];

    if (pSetting->kind == StartSettingCheckbox)
    {
        STARTMENU_NODE *pNode;
        if (ReadTreeItemNode(pState->hTree, pState->hRootItems[iSetting], &pNode))
        {
            SetTreeItemStateImage(pState->hTree, pState->hRootItems[iSetting],
                GetStateImageIndex(pNode, SettingCheckedValue(pState, iSetting)));
        }
    }
    else
    {
        UINT iOption;
        for (iOption = 0; iOption < pSetting->cOptions; iOption++)
        {
            STARTMENU_NODE *pNode;
            if (!ReadTreeItemNode(pState->hTree,
                pState->hOptionItems[iSetting][iOption], &pNode))
                continue;

            SetTreeItemStateImage(pState->hTree,
                pState->hOptionItems[iSetting][iOption],
                GetStateImageIndex(pNode, OptionCheckedValue(pState, iSetting, iOption)));
        }
    }
}

static
BOOL InitializeStartMenuTree(STARTMENU7STATE *pState)
{
    UINT iSetting;



    if (!CreateStateImageList(pState))
        return FALSE;

    TreeView_SetImageList(pState->hTree, pState->hStateImages, TVSIL_STATE);

    for (iSetting = 0; iSetting < ARRAYSIZE(g_StartMenuSettings); iSetting++)
    {
        const STARTMENU_SETTING *pSetting = &g_StartMenuSettings[iSetting];
        STARTMENU_NODE *pNode = AppendStartMenuNode(pState,
            pSetting->kind == StartSettingCheckbox ? StartNodeCheckbox : StartNodeGroup,
            iSetting, 0);
        TVINSERTSTRUCTW item;
        WCHAR szText[256] = L"";

        if (!pNode)
            return FALSE;

        if (!LoadIndirectResourceString(pSetting->pszText, szText, ARRAYSIZE(szText)))
            return FALSE;

        if (pSetting->pszBitmap && *pSetting->pszBitmap)
        {
            HICON hIcon;
            if (LoadSettingIcon(pSetting->pszBitmap, &hIcon))
                pNode->hIcon = hIcon;
        }

        ZeroMemory(&item, sizeof(item));
        item.hParent = TVI_ROOT;
        item.hInsertAfter = TVI_LAST;
        item.item.mask = TVIF_TEXT | TVIF_PARAM;
        item.item.pszText = szText;
        item.item.lParam = (LPARAM)pNode;
        pState->hRootItems[iSetting] = TreeView_InsertItem(pState->hTree, &item);
        if (!pState->hRootItems[iSetting])
            return FALSE;

        if (pSetting->kind == StartSettingCheckbox)
        {
            RefreshStartMenuSettingUI(pState, iSetting);
        }
        else
        {
            UINT iOption;
            for (iOption = 0; iOption < pSetting->cOptions; iOption++)
            {
                STARTMENU_NODE *pOptionNode = AppendStartMenuNode(pState,
                    StartNodeRadio, iSetting, iOption);
                WCHAR szOptionText[256] = L"";

                if (!pOptionNode)
                    return FALSE;
                if (!LoadIndirectResourceString(pSetting->pOptions[iOption].pszText,
                    szOptionText, ARRAYSIZE(szOptionText)))
                    return FALSE;

                ZeroMemory(&item, sizeof(item));
                item.hParent = pState->hRootItems[iSetting];
                item.hInsertAfter = TVI_LAST;
                item.item.mask = TVIF_TEXT | TVIF_PARAM;
                item.item.pszText = szOptionText;
                item.item.lParam = (LPARAM)pOptionNode;
                pState->hOptionItems[iSetting][iOption] =
                    TreeView_InsertItem(pState->hTree, &item);
                if (!pState->hOptionItems[iSetting][iOption])
                    return FALSE;
            }

            TreeView_Expand(pState->hTree, pState->hRootItems[iSetting], TVE_EXPAND);
            RefreshStartMenuSettingUI(pState, iSetting);
        }


    }

    return TRUE;
}

static
void ToggleTreeNode(STARTMENU7STATE *pState, HTREEITEM hItem)
{
    STARTMENU_NODE *pNode;

    if (!ReadTreeItemNode(pState->hTree, hItem, &pNode) || pNode->bRestricted)
        return;

    switch (pNode->kind)
    {
    case StartNodeCheckbox:
    {
        const STARTMENU_SETTING *pSetting = &g_StartMenuSettings[pNode->iSetting];
        pState->dwValues[pNode->iSetting] =
            SettingCheckedValue(pState, pNode->iSetting) ?
            pSetting->dwUncheckedValue : pSetting->dwCheckedValue;
        RefreshStartMenuSettingUI(pState, pNode->iSetting);
        break;
    }

    case StartNodeRadio:
        pState->dwValues[pNode->iSetting] =
            g_StartMenuSettings[pNode->iSetting].pOptions[pNode->iOption].dwCheckedValue;
        RefreshStartMenuSettingUI(pState, pNode->iSetting);
        break;

    default:
        break;
    }
}

static
BOOL WriteStartMenuSettings(const STARTMENU7STATE *pState)
{
    UINT iSetting;

    if (IsWindowEnabled(GetDlgItem(pState->hDlg, IDC_SM_MFU_PROGRAMS)) &&
        !WriteDwordValue(HKEY_CURRENT_USER, g_explorerKey,
            L"Start_MinMFU", pState->cPrograms > STARTMENU_RECENT_PROGRAMS_MAX ?
            STARTMENU_RECENT_PROGRAMS_MAX : pState->cPrograms))
        return FALSE;

    if (IsWindowEnabled(GetDlgItem(pState->hDlg, IDC_SM_MFU_ITEMS)) &&
        !WriteDwordValue(HKEY_CURRENT_USER, g_explorerKey,
            L"Start_JumpListItems", pState->cItems > STARTMENU_JUMPLIST_ITEMS_MAX ?
            STARTMENU_JUMPLIST_ITEMS_MAX : pState->cItems))
        return FALSE;

    for (iSetting = 0; iSetting < ARRAYSIZE(g_StartMenuSettings); iSetting++)
    {
        const STARTMENU_SETTING *pSetting = &g_StartMenuSettings[iSetting];
        if (pState->bRestricted[iSetting])
            continue;

        if (IsStartMenuAdminToolsSetting(iSetting))
        {
            if (!WriteAdminToolsValue(pState->dwValues[iSetting]))
                return FALSE;
            continue;
        }

        if (pSetting->kind == StartSettingCheckbox)
        {
            if (!WriteDwordValue(HKEY_CURRENT_USER, pSetting->pszRegPath,
                pSetting->pszValueName, pState->dwValues[iSetting]))
                return FALSE;
        }
        else if (pSetting->cOptions)
        {
            if (!WriteDwordValue(HKEY_CURRENT_USER,
                pSetting->pOptions[0].pszRegPath,
                pSetting->pOptions[0].pszValueName,
                pState->dwValues[iSetting]))
                return FALSE;
        }
    }

    return TRUE;
}

static
void ResetStartMenuDialogDefaults(STARTMENU7STATE *pState)
{
    InitializeStartMenuDefaults(pState);
    UpdateMfuControls(pState);

    for (UINT iSetting = 0; iSetting < ARRAYSIZE(g_StartMenuSettings); iSetting++)
        RefreshStartMenuSettingUI(pState, iSetting);
}

static
BOOL InitializeStartMenu7Dialog(HWND hWnd)
{
    ZeroMemory(&g_startMenu7, sizeof(g_startMenu7));
    g_startMenu7.hDlg = hWnd;
    g_startMenu7.hParent = GetWindow(hWnd, GW_OWNER);
    g_startMenu7.hTree = GetDlgItem(hWnd, IDC_SM_SysTreeView);
    if (!g_startMenu7.hTree)
        return FALSE;

    LoadStartMenuState(&g_startMenu7);
    SetRanges();
    UpdateMfuControls(&g_startMenu7);

    if (!InitializeStartMenuTree(&g_startMenu7))
        return FALSE;

    SetWindowSubclass(g_startMenu7.hTree, StartMenuTreeSubclassProc, 1,
        (DWORD_PTR)&g_startMenu7);
    TreeView_SelectSetFirstVisible(g_startMenu7.hTree,
        TreeView_GetRoot(g_startMenu7.hTree));
    return TRUE;
}

static
void CleanupStartMenu7Dialog(void)
{
    if (g_startMenu7.hTree)
        RemoveWindowSubclass(g_startMenu7.hTree, StartMenuTreeSubclassProc, 1);
    DestroyStartMenuImages(&g_startMenu7);
    ZeroMemory(&g_startMenu7, sizeof(g_startMenu7));
}

static
void ApplyLegacySettings(void)
{
    if (!WriteLegacyRegSettings())
        UpdateLegacyControls();

    g_oldSettings = g_newSettings;
    NotifyTraySettingsChanged(TRUE);
}

static
void HandleLegacyCommand(WORD iControl)
{
#define GetComboIndex() \
    (BYTE)SendDlgItemMessage(g_hDlg, iControl, CB_GETCURSEL, 0L, 0L)
#define GetUdPos(iUd) \
    (int)LOWORD(SendDlgItemMessage(g_hDlg, iUd, UDM_GETPOS, 0L, 0L))

    switch (iControl)
    {
    case IDC_SM_10DLG_MODE:
        g_newSettings.iMode = GetComboIndex();
        break;

    case IDOK:
        g_newSettings.iPrograms = GetUdPos(IDC_SM_MFU_PROGRAMS_SPIN);
        g_newSettings.iItems = GetUdPos(IDC_SM_MFU_ITEMS_SPIN);
        ApplyLegacySettings();
        EndDialog(g_hDlg, iControl);
        break;

    case IDC_SM_DEFAULT_BUTTON:
        LoadLegacyDefaultSettings();
        UpdateLegacyControls();
        break;

    case IDCANCEL:
        g_newSettings = g_oldSettings;
        EndDialog(g_hDlg, iControl);
        break;

    default:
        return;
    }

    EnableApply();

#undef GetUdPos
#undef GetComboIndex
}

static
void HandleLegacyComboBoxSelChange(WORD iControl)
{
    if (iControl != IDC_SM_10DLG_MODE)
        return;

    g_newSettings.iMode =
        (BYTE)SendDlgItemMessage(g_hDlg, iControl, CB_GETCURSEL, 0L, 0L);
    EnableApply();
}

INT_PTR CALLBACK StartMenu10DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
        g_hDlg = hWnd;
        InitLegacyPage();
        return 0;

    case WM_COMMAND:
        switch HIWORD(wParam)
        {
        case BN_CLICKED:
            HandleLegacyCommand(LOWORD(wParam));
            break;

        case CBN_SELCHANGE:
            HandleLegacyComboBoxSelChange(LOWORD(wParam));
            break;
        }
        return 0;

    case WM_CLOSE:
        EndDialog(g_hDlg, (INT)uMsg);
        return 0;
    }

    return 0;
}

INT_PTR CALLBACK StartMenu7DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
        g_hDlg = hWnd;
        return InitializeStartMenu7Dialog(hWnd) ? TRUE : FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SM_DEFAULT_BUTTON:
            ResetStartMenuDialogDefaults(&g_startMenu7);
            return TRUE;

        case IDOK:
            if (IsWindowEnabled(GetDlgItem(hWnd, IDC_SM_MFU_PROGRAMS)))
                g_startMenu7.cPrograms = GetDlgItemInt(hWnd, IDC_SM_MFU_PROGRAMS, NULL, FALSE);
            if (IsWindowEnabled(GetDlgItem(hWnd, IDC_SM_MFU_ITEMS)))
                g_startMenu7.cItems = GetDlgItemInt(hWnd, IDC_SM_MFU_ITEMS, NULL, FALSE);

            if (!WriteStartMenuSettings(&g_startMenu7))
            {
                ShowMessageFromAppResource(hWnd, IDS_ERROR_GENERIC, IDS_ERROR, MB_OK);
                return TRUE;
            }

            NotifyTraySettingsChanged(TRUE);

            CleanupStartMenu7Dialog();
            EndDialog(hWnd, IDOK);
            return TRUE;

        case IDCANCEL:
            CleanupStartMenu7Dialog();
            EndDialog(hWnd, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_NOTIFY:
    {
        NMHDR *pHdr = (NMHDR *)lParam;
        if (pHdr->idFrom == IDC_SM_SysTreeView)
        {
            if (pHdr->code == NM_CLICK || pHdr->code == NM_DBLCLK)
            {
                DWORD dwPos = GetMessagePos();
                POINT pt;
                pt.x = GET_X_LPARAM(dwPos);
                pt.y = GET_Y_LPARAM(dwPos);
                MapWindowPoints(HWND_DESKTOP, g_startMenu7.hTree, &pt, 1);
                if (HandleStartMenuTreeClick(&g_startMenu7, pt))
                    return TRUE;
            }
            else if (pHdr->code == TVN_KEYDOWN)
            {
                const NMTVKEYDOWN *pKey = (const NMTVKEYDOWN *)lParam;
                if (pKey->wVKey == VK_SPACE)
                {
                    HTREEITEM hItem = TreeView_GetSelection(g_startMenu7.hTree);
                    if (hItem)
                        ToggleTreeNode(&g_startMenu7, hItem);
                    return TRUE;
                }
            }
            else if (pHdr->code == NM_CUSTOMDRAW)
            {
                SetWindowLongPtr(hWnd, DWLP_MSGRESULT,
                    HandleStartMenuTreeCustomDraw(&g_startMenu7,
                        (NMTVCUSTOMDRAW *)lParam));
                return TRUE;
            }
        }
        break;
    }

    case WM_CLOSE:
        CleanupStartMenu7Dialog();
        EndDialog(hWnd, IDCANCEL);
        return TRUE;

    case WM_DESTROY:
        CleanupStartMenu7Dialog();
        return TRUE;
    }

    return 0;
}
