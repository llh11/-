// =================================================================================
//  新闻调度器配置 v7.8
//  作者: Gemini
//  更新日期: 2024-07-28
//
//  功能更新:
//  - [新增] “小组件设置”选项卡现在支持对倒计时和课表所有元素的字体、颜色进行详细配置。
//  - [新增] 颜色选择框现在可以实时预览所选颜色。
//  - [优化] “生成并填充课表”功能现在会智能保留已存在的、时间匹配的课程安排。
//  - [同步] 更新了版本号，并与主程序 v7.8 的配置文件格式完全兼容。
// =================================================================================

#include <Windows.h>
#include <CommCtrl.h>
#include <Commdlg.h>
#include <shlwapi.h>
#include <winreg.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <memory>
#include <cstdio>
#include <Urlmon.h>
#include "resource.h" // 包含资源头文件

// --- 链接器依赖 ---
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "comdlg32.lib")

// 启用 Visual Styles
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace std;

// --- 常量与全局定义 ---
const int SCREEN_WIDTH = 720;
const int SCREEN_HEIGHT = 680;
const wchar_t CONFIG_DIR[] = L"D:\\news\\";
const wchar_t CONFIG_FILE_NAME[] = L"config.ini";
const wchar_t ICON_FILE_NAME[] = L"favicon.ico";
const wchar_t ICON_DOWNLOAD_URL[] = L"https://www.lcez.cn/favicon.ico";
const wchar_t APP_REG_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t APP_REG_VALUE[] = L"NewsScheduler";
const wchar_t MAIN_APP_EXE_NAME[] = L"新闻.exe";


// --- 结构体与枚举 ---
enum class EventType { Unknown, OpenURL, SystemAction, CloseBrowsers };
struct ScheduledEvent {
    wstring sectionName;
    EventType type = EventType::OpenURL;
    int hour = 19, minute = 0;
    wstring parameters;
    bool enableVolumeFade = false, enableMouseClick = false;
    int startVolume = 0, endVolume = 100, fadeDuration = 5;
    int clickX = 0, clickY = 0;
};

struct TimetableEntry {
    wstring subject;
    wstring time; // 格式 "HH:MM-HH:MM"
    int classNumber = 0; // 用于填充对话框
};

enum class TemplateItemType { COURSE_BLOCK, BREAK_BLOCK };
struct TemplateItem {
    TemplateItemType type;
    wstring displayName;
    int consecutiveClasses = 4;
    int classDuration = 45;
    int breakDuration = 10;
    wstring breakName;
    int startHour = 12, startMinute = 0, endHour = 13, endMinute = 0;
};

struct CountdownSettings {
    wstring title = L"距离高考还有";
    SYSTEMTIME targetTime = { 2025, 6, 4, 7, 9, 0, 0, 0 };
    int titleFontSize = 24;
    int timeFontSize = 48;
    COLORREF titleFontColor = RGB(255, 255, 255);
    COLORREF timeFontColor = RGB(255, 255, 255);
};

struct TimetableSettings {
    int headerFontSize = 20;
    int entryFontSize = 18;
    COLORREF headerFontColor = RGB(255, 255, 255);
    COLORREF entryFontColor = RGB(255, 255, 255);
    COLORREF timeFontColor = RGB(200, 220, 255);
    COLORREF highlightColor = RGB(255, 255, 0);
};

// --- 全局变量 ---
HINSTANCE g_hInstance = NULL;
HWND g_hMainWnd = NULL;
HICON g_hAppIcon = NULL;
HFONT g_hFont = NULL;
HWND g_hTabCtrl = NULL;
vector<HWND> g_tabControls[5];

vector<ScheduledEvent> g_events;
int g_nextEventId = 1;

vector<TimetableEntry> g_timetables[7];
vector<TemplateItem> g_timetableTemplates[7];
int g_templateStartH[7] = { 8,8,8,8,8,8,8 };
int g_templateStartM[7] = { 0,0,0,0,0,0,0 };
int g_currentDay = 0; // 0=Mon, 6=Sun

vector<wstring> g_availableCourses;
bool g_isAutoRun = false;

CountdownSettings g_countdownSettings;
TimetableSettings g_timetableSettings;

// --- 函数原型 ---
void LoadConfiguration();
void SaveConfiguration();
void InitUI(HWND hWnd);
void SwitchTab(int tabIndex);
void SelectDay(int dayIndex);
void PopulateEventList();
void PopulateTimetableList();
void PopulateTemplateList();
void PopulateAvailableCoursesList();
void PopulateAllLists();
void GenerateAndFillTimetable();
HICON LoadAppIcon(HINSTANCE hInstance);
void ChooseColorFor(HWND hWnd, COLORREF* colorRef, HWND hStaticSample);
INT_PTR CALLBACK EventDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK TimetableDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK CourseBlockDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK BreakBlockDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK FillCoursesDlgProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


// --- 辅助函数 ---
wstring GetConfigPath() {
    return wstring(CONFIG_DIR) + CONFIG_FILE_NAME;
}

// --- 注册表操作 ---
void SetAutoRun(bool enable) {
    HKEY hKey;
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);
    PathAppendW(szPath, MAIN_APP_EXE_NAME);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, APP_REG_KEY, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            RegSetValueExW(hKey, APP_REG_VALUE, 0, REG_SZ, (BYTE*)szPath, (wcslen(szPath) + 1) * sizeof(wchar_t));
        }
        else {
            RegDeleteValueW(hKey, APP_REG_VALUE);
        }
        RegCloseKey(hKey);
    }
}

bool IsAutoRunEnabled() {
    HKEY hKey;
    bool enabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, APP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, APP_REG_VALUE, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            enabled = true;
        }
        RegCloseKey(hKey);
    }
    return enabled;
}

// --- 配置加载/保存 ---
void LoadConfiguration() {
    wstring configPath = GetConfigPath();
    wchar_t buffer[4096];

    g_events.clear();
    g_nextEventId = 1;
    wchar_t allSections[16384];
    GetPrivateProfileSectionNamesW(allSections, 16384, configPath.c_str());
    for (wchar_t* pSection = allSections; *pSection != L'\0'; pSection += wcslen(pSection) + 1) {
        if (wcsstr(pSection, L"Event") == pSection) {
            ScheduledEvent event;
            event.sectionName = pSection;
            GetPrivateProfileStringW(pSection, L"Type", L"OpenURL", buffer, 2048, configPath.c_str());
            if (_wcsicmp(buffer, L"OpenURL") == 0) event.type = EventType::OpenURL;
            else if (_wcsicmp(buffer, L"SystemAction") == 0) event.type = EventType::SystemAction;
            else if (_wcsicmp(buffer, L"CloseBrowsers") == 0) event.type = EventType::CloseBrowsers;
            else event.type = EventType::Unknown;

            GetPrivateProfileStringW(pSection, L"Time", L"19:00", buffer, 2048, configPath.c_str());
            swscanf_s(buffer, L"%d:%d", &event.hour, &event.minute);
            GetPrivateProfileStringW(pSection, L"Parameters", L"", buffer, 2048, configPath.c_str());
            event.parameters = buffer;

            event.enableVolumeFade = GetPrivateProfileIntW(pSection, L"EnableVolumeFade", 0, configPath.c_str());
            event.startVolume = GetPrivateProfileIntW(pSection, L"StartVolume", 0, configPath.c_str());
            event.endVolume = GetPrivateProfileIntW(pSection, L"EndVolume", 100, configPath.c_str());
            event.fadeDuration = GetPrivateProfileIntW(pSection, L"FadeDuration", 5, configPath.c_str());
            event.enableMouseClick = GetPrivateProfileIntW(pSection, L"EnableMouseClick", 0, configPath.c_str());
            event.clickX = GetPrivateProfileIntW(pSection, L"ClickX", 0, configPath.c_str());
            event.clickY = GetPrivateProfileIntW(pSection, L"ClickY", 0, configPath.c_str());

            g_events.push_back(event);
            int currentId = _wtoi(event.sectionName.c_str() + 5);
            if (currentId >= g_nextEventId) g_nextEventId = currentId + 1;
        }
    }

    for (int i = 0; i < 7; ++i) {
        g_timetables[i].clear();
        g_timetableTemplates[i].clear();
        wstring dayStr = to_wstring(i);

        wstring timetableSection = L"Timetable_" + dayStr;
        for (int j = 1; j <= 30; ++j) {
            wchar_t subject[256], time[256];
            GetPrivateProfileStringW(timetableSection.c_str(), (L"Class" + to_wstring(j)).c_str(), L"", subject, 256, configPath.c_str());
            GetPrivateProfileStringW(timetableSection.c_str(), (L"Time" + to_wstring(j)).c_str(), L"", time, 256, configPath.c_str());
            if (wcslen(subject) > 0) g_timetables[i].push_back({ subject, time });
            else break;
        }

        wstring templateSection = L"Template_" + dayStr;
        g_templateStartH[i] = GetPrivateProfileIntW(templateSection.c_str(), L"StartH", 8, configPath.c_str());
        g_templateStartM[i] = GetPrivateProfileIntW(templateSection.c_str(), L"StartM", 0, configPath.c_str());
        int templateCount = GetPrivateProfileIntW(templateSection.c_str(), L"Count", 0, configPath.c_str());
        for (int k = 0; k < templateCount; ++k) {
            TemplateItem item;
            wstring itemSection = templateSection + L"_Item" + to_wstring(k);
            item.type = (TemplateItemType)GetPrivateProfileIntW(itemSection.c_str(), L"Type", 0, configPath.c_str());
            if (item.type == TemplateItemType::COURSE_BLOCK) {
                item.consecutiveClasses = GetPrivateProfileIntW(itemSection.c_str(), L"Count", 4, configPath.c_str());
                item.classDuration = GetPrivateProfileIntW(itemSection.c_str(), L"Duration", 45, configPath.c_str());
                item.breakDuration = GetPrivateProfileIntW(itemSection.c_str(), L"Break", 10, configPath.c_str());
                item.displayName = L"按此规则课程数: " + to_wstring(item.consecutiveClasses)
                    + L", 每节" + to_wstring(item.classDuration) + L"分, 休息" + to_wstring(item.breakDuration) + L"分";
            }
            else {
                GetPrivateProfileStringW(itemSection.c_str(), L"Name", L"休息", buffer, 256, configPath.c_str());
                item.breakName = buffer;
                item.startHour = GetPrivateProfileIntW(itemSection.c_str(), L"StartH", 12, configPath.c_str());
                item.startMinute = GetPrivateProfileIntW(itemSection.c_str(), L"StartM", 0, configPath.c_str());
                item.endHour = GetPrivateProfileIntW(itemSection.c_str(), L"EndH", 13, configPath.c_str());
                item.endMinute = GetPrivateProfileIntW(itemSection.c_str(), L"EndM", 0, configPath.c_str());
                wchar_t timeBuf[100];
                swprintf_s(timeBuf, L"%s (%02d:%02d - %02d:%02d)", item.breakName.c_str(), item.startHour, item.startMinute, item.endHour, item.endMinute);
                item.displayName = timeBuf;
            }
            g_timetableTemplates[i].push_back(item);
        }
    }

    g_availableCourses.clear();
    GetPrivateProfileStringW(L"CourseLibrary", L"Courses", L"", buffer, 4096, configPath.c_str());
    if (wcslen(buffer) == 0) {
        g_availableCourses = { L"语文", L"数学", L"英语", L"物理", L"化学", L"生物", L"政治", L"历史", L"地理", L"信息", L"体育", L"美术", L"音乐", L"班会", L"心理健康", L"自由活动", L"通用技术" };
    }
    else {
        wstringstream ss(buffer);
        wstring course;
        while (getline(ss, course, L',')) {
            if (!course.empty()) g_availableCourses.push_back(course);
        }
    }

    GetPrivateProfileStringW(L"Countdown", L"Title", L"距离高考还有", buffer, 256, configPath.c_str());
    g_countdownSettings.title = buffer;
    g_countdownSettings.targetTime.wYear = GetPrivateProfileIntW(L"Countdown", L"Year", 2025, configPath.c_str());
    g_countdownSettings.targetTime.wMonth = GetPrivateProfileIntW(L"Countdown", L"Month", 6, configPath.c_str());
    g_countdownSettings.targetTime.wDay = GetPrivateProfileIntW(L"Countdown", L"Day", 7, configPath.c_str());
    g_countdownSettings.targetTime.wHour = GetPrivateProfileIntW(L"Countdown", L"Hour", 9, configPath.c_str());
    g_countdownSettings.targetTime.wMinute = GetPrivateProfileIntW(L"Countdown", L"Minute", 0, configPath.c_str());
    g_countdownSettings.titleFontSize = GetPrivateProfileIntW(L"Countdown", L"TitleFontSize", 24, configPath.c_str());
    g_countdownSettings.timeFontSize = GetPrivateProfileIntW(L"Countdown", L"TimeFontSize", 48, configPath.c_str());
    g_countdownSettings.titleFontColor = GetPrivateProfileIntW(L"Countdown", L"TitleFontColor", RGB(255, 255, 255), configPath.c_str());
    g_countdownSettings.timeFontColor = GetPrivateProfileIntW(L"Countdown", L"TimeFontColor", RGB(255, 255, 255), configPath.c_str());

    g_timetableSettings.headerFontSize = GetPrivateProfileIntW(L"Timetable", L"HeaderFontSize", 20, configPath.c_str());
    g_timetableSettings.entryFontSize = GetPrivateProfileIntW(L"Timetable", L"EntryFontSize", 18, configPath.c_str());
    g_timetableSettings.headerFontColor = GetPrivateProfileIntW(L"Timetable", L"HeaderFontColor", RGB(255, 255, 255), configPath.c_str());
    g_timetableSettings.entryFontColor = GetPrivateProfileIntW(L"Timetable", L"EntryFontColor", RGB(255, 255, 255), configPath.c_str());
    g_timetableSettings.timeFontColor = GetPrivateProfileIntW(L"Timetable", L"TimeFontColor", RGB(200, 220, 255), configPath.c_str());
    g_timetableSettings.highlightColor = GetPrivateProfileIntW(L"Timetable", L"HighlightColor", RGB(255, 255, 0), configPath.c_str());

    g_isAutoRun = IsAutoRunEnabled();
}

void SaveConfiguration() {
    wstring configPath = GetConfigPath();
    if (CreateDirectoryW(CONFIG_DIR, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        wchar_t allSections[16384];
        GetPrivateProfileSectionNamesW(allSections, 16384, configPath.c_str());
        for (wchar_t* pSection = allSections; *pSection != L'\0'; pSection += wcslen(pSection) + 1) {
            if (wcsstr(pSection, L"Event") == pSection) {
                WritePrivateProfileStringW(pSection, NULL, NULL, configPath.c_str());
            }
        }
        for (const auto& event : g_events) {
            const wchar_t* typeStr = L"Unknown";
            if (event.type == EventType::OpenURL) typeStr = L"OpenURL";
            if (event.type == EventType::SystemAction) typeStr = L"SystemAction";
            if (event.type == EventType::CloseBrowsers) typeStr = L"CloseBrowsers";
            WritePrivateProfileStringW(event.sectionName.c_str(), L"Type", typeStr, configPath.c_str());
            wchar_t timeBuf[10];
            swprintf_s(timeBuf, L"%02d:%02d", event.hour, event.minute);
            WritePrivateProfileStringW(event.sectionName.c_str(), L"Time", timeBuf, configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"Parameters", event.parameters.c_str(), configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"EnableVolumeFade", to_wstring(event.enableVolumeFade).c_str(), configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"StartVolume", to_wstring(event.startVolume).c_str(), configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"EndVolume", to_wstring(event.endVolume).c_str(), configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"FadeDuration", to_wstring(event.fadeDuration).c_str(), configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"EnableMouseClick", to_wstring(event.enableMouseClick).c_str(), configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"ClickX", to_wstring(event.clickX).c_str(), configPath.c_str());
            WritePrivateProfileStringW(event.sectionName.c_str(), L"ClickY", to_wstring(event.clickY).c_str(), configPath.c_str());
        }

        for (int i = 0; i < 7; ++i) {
            wstring dayStr = to_wstring(i);
            wstring timetableSection = L"Timetable_" + dayStr;
            WritePrivateProfileStringW(timetableSection.c_str(), NULL, NULL, configPath.c_str());
            for (size_t j = 0; j < g_timetables[i].size(); ++j) {
                WritePrivateProfileStringW(timetableSection.c_str(), (L"Class" + to_wstring(j + 1)).c_str(), g_timetables[i][j].subject.c_str(), configPath.c_str());
                WritePrivateProfileStringW(timetableSection.c_str(), (L"Time" + to_wstring(j + 1)).c_str(), g_timetables[i][j].time.c_str(), configPath.c_str());
            }
            wstring templateSection = L"Template_" + dayStr;
            WritePrivateProfileStringW(templateSection.c_str(), NULL, NULL, configPath.c_str());
            WritePrivateProfileStringW(templateSection.c_str(), L"StartH", to_wstring(g_templateStartH[i]).c_str(), configPath.c_str());
            WritePrivateProfileStringW(templateSection.c_str(), L"StartM", to_wstring(g_templateStartM[i]).c_str(), configPath.c_str());
            WritePrivateProfileStringW(templateSection.c_str(), L"Count", to_wstring(g_timetableTemplates[i].size()).c_str(), configPath.c_str());
            for (size_t k = 0; k < g_timetableTemplates[i].size(); ++k) {
                const auto& item = g_timetableTemplates[i][k];
                wstring itemSection = templateSection + L"_Item" + to_wstring(k);
                WritePrivateProfileStringW(itemSection.c_str(), L"Type", to_wstring((int)item.type).c_str(), configPath.c_str());
                if (item.type == TemplateItemType::COURSE_BLOCK) {
                    WritePrivateProfileStringW(itemSection.c_str(), L"Count", to_wstring(item.consecutiveClasses).c_str(), configPath.c_str());
                    WritePrivateProfileStringW(itemSection.c_str(), L"Duration", to_wstring(item.classDuration).c_str(), configPath.c_str());
                    WritePrivateProfileStringW(itemSection.c_str(), L"Break", to_wstring(item.breakDuration).c_str(), configPath.c_str());
                }
                else {
                    WritePrivateProfileStringW(itemSection.c_str(), L"Name", item.breakName.c_str(), configPath.c_str());
                    WritePrivateProfileStringW(itemSection.c_str(), L"StartH", to_wstring(item.startHour).c_str(), configPath.c_str());
                    WritePrivateProfileStringW(itemSection.c_str(), L"StartM", to_wstring(item.startMinute).c_str(), configPath.c_str());
                    WritePrivateProfileStringW(itemSection.c_str(), L"EndH", to_wstring(item.endHour).c_str(), configPath.c_str());
                    WritePrivateProfileStringW(itemSection.c_str(), L"EndM", to_wstring(item.endMinute).c_str(), configPath.c_str());
                }
            }
        }

        wstringstream ss;
        for (size_t i = 0; i < g_availableCourses.size(); ++i) {
            ss << g_availableCourses[i] << (i == g_availableCourses.size() - 1 ? L"" : L",");
        }
        WritePrivateProfileStringW(L"CourseLibrary", L"Courses", ss.str().c_str(), configPath.c_str());

        WritePrivateProfileStringW(L"Countdown", L"Title", g_countdownSettings.title.c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"Year", to_wstring(g_countdownSettings.targetTime.wYear).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"Month", to_wstring(g_countdownSettings.targetTime.wMonth).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"Day", to_wstring(g_countdownSettings.targetTime.wDay).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"Hour", to_wstring(g_countdownSettings.targetTime.wHour).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"Minute", to_wstring(g_countdownSettings.targetTime.wMinute).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"TitleFontSize", to_wstring(g_countdownSettings.titleFontSize).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"TimeFontSize", to_wstring(g_countdownSettings.timeFontSize).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"TitleFontColor", to_wstring(g_countdownSettings.titleFontColor).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Countdown", L"TimeFontColor", to_wstring(g_countdownSettings.timeFontColor).c_str(), configPath.c_str());

        WritePrivateProfileStringW(L"Timetable", L"HeaderFontSize", to_wstring(g_timetableSettings.headerFontSize).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Timetable", L"EntryFontSize", to_wstring(g_timetableSettings.entryFontSize).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Timetable", L"HeaderFontColor", to_wstring(g_timetableSettings.headerFontColor).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Timetable", L"EntryFontColor", to_wstring(g_timetableSettings.entryFontColor).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Timetable", L"TimeFontColor", to_wstring(g_timetableSettings.timeFontColor).c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Timetable", L"HighlightColor", to_wstring(g_timetableSettings.highlightColor).c_str(), configPath.c_str());

        SetAutoRun(g_isAutoRun);
        MessageBoxW(g_hMainWnd, L"配置已成功保存！", L"成功", MB_OK | MB_ICONINFORMATION);
    }
    else {
        MessageBoxW(g_hMainWnd, L"无法创建配置目录 D:\\news\\", L"错误", MB_OK | MB_ICONERROR);
    }
}

// --- UI填充 ---
void PopulateEventList() {
    HWND hList = g_tabControls[0][0];
    ListView_DeleteAllItems(hList);
    for (size_t i = 0; i < g_events.size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.lParam = (LPARAM)i;

        wchar_t timeBuf[10];
        swprintf_s(timeBuf, L"%02d:%02d", g_events[i].hour, g_events[i].minute);

        const wchar_t* typeStr = L"未知";
        if (g_events[i].type == EventType::OpenURL) typeStr = L"打开网页";
        if (g_events[i].type == EventType::SystemAction) typeStr = L"系统操作";
        if (g_events[i].type == EventType::CloseBrowsers) typeStr = L"关闭浏览器";

        lvi.pszText = const_cast<LPWSTR>(g_events[i].sectionName.c_str());
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, timeBuf);
        ListView_SetItemText(hList, i, 2, const_cast<LPWSTR>(typeStr));
        ListView_SetItemText(hList, i, 3, const_cast<LPWSTR>(g_events[i].parameters.c_str()));
    }
}

void PopulateTimetableList() {
    HWND hList = g_tabControls[1][7];
    ListView_DeleteAllItems(hList);
    for (size_t i = 0; i < g_timetables[g_currentDay].size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.lParam = (LPARAM)i;
        lvi.pszText = const_cast<LPWSTR>(g_timetables[g_currentDay][i].subject.c_str());
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, const_cast<LPWSTR>(g_timetables[g_currentDay][i].time.c_str()));
    }
}

void PopulateTemplateList() {
    HWND hList = g_tabControls[2][7];
    ListView_DeleteAllItems(hList);
    for (size_t i = 0; i < g_timetableTemplates[g_currentDay].size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.lParam = (LPARAM)i;
        lvi.pszText = const_cast<LPWSTR>(g_timetableTemplates[g_currentDay][i].displayName.c_str());
        ListView_InsertItem(hList, &lvi);
    }
    SetDlgItemInt(g_hMainWnd, IDC_EDIT_TEMPLATE_START_H, g_templateStartH[g_currentDay], FALSE);
    SetDlgItemInt(g_hMainWnd, IDC_EDIT_TEMPLATE_START_M, g_templateStartM[g_currentDay], FALSE);
}

void PopulateAvailableCoursesList() {
    HWND hList = g_tabControls[3][3];
    ListView_DeleteAllItems(hList);
    for (size_t i = 0; i < g_availableCourses.size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.pszText = const_cast<LPWSTR>(g_availableCourses[i].c_str());
        ListView_InsertItem(hList, &lvi);
    }
}

void PopulateAllLists() {
    PopulateEventList();
    PopulateTimetableList();
    PopulateTemplateList();
    PopulateAvailableCoursesList();
}

// --- 主窗口过程 ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitUI(hWnd);
        LoadConfiguration();
        PopulateAllLists();

        // Populate Widget Settings Tab
        SetWindowTextW(g_tabControls[4][2], g_countdownSettings.title.c_str());
        DateTime_SetSystemtime(g_tabControls[4][4], GDT_VALID, &g_countdownSettings.targetTime);
        DateTime_SetSystemtime(g_tabControls[4][5], GDT_VALID, &g_countdownSettings.targetTime);
        SetDlgItemInt(hWnd, IDC_EDIT_CD_TITLE_FONTSIZE, g_countdownSettings.titleFontSize, FALSE);
        SetDlgItemInt(hWnd, IDC_EDIT_CD_TIME_FONTSIZE, g_countdownSettings.timeFontSize, FALSE);
        SetDlgItemInt(hWnd, IDC_EDIT_TT_HEADER_FONTSIZE, g_timetableSettings.headerFontSize, FALSE);
        SetDlgItemInt(hWnd, IDC_EDIT_TT_ENTRY_FONTSIZE, g_timetableSettings.entryFontSize, FALSE);

        // Invalidate all color swatches
        for (int i = 8; i <= 18; i += 2) InvalidateRect(g_tabControls[4][i], NULL, TRUE);

        CheckDlgButton(hWnd, IDC_CHK_AUTORUN, g_isAutoRun ? BST_CHECKED : BST_UNCHECKED);

        g_currentDay = 0;
        EnableWindow(g_tabControls[1][0], FALSE);
        EnableWindow(g_tabControls[2][0], FALSE);

        SwitchTab(0);
        break;
    case WM_NOTIFY: {
        LPNMHDR lpnm = (LPNMHDR)lParam;
        if (lpnm->idFrom == IDC_TAB_CONTROL) {
            if (lpnm->code == TCN_SELCHANGE) {
                SwitchTab(TabCtrl_GetCurSel(g_hTabCtrl));
            }
        }
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int selected = -1;

        if (wmId >= IDC_BTN_DAY_PREVIEW_START && wmId <= IDC_BTN_DAY_PREVIEW_START + 6) {
            SelectDay(wmId - IDC_BTN_DAY_PREVIEW_START);
            break;
        }
        if (wmId >= IDC_BTN_DAY_TEMPLATE_START && wmId <= IDC_BTN_DAY_TEMPLATE_START + 6) {
            SelectDay(wmId - IDC_BTN_DAY_TEMPLATE_START);
            break;
        }

        switch (wmId) {
        case IDC_BTN_ADD_EVENT: {
            ScheduledEvent newEvent;
            newEvent.sectionName = L"Event" + to_wstring(g_nextEventId);
            if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_EVENT_DIALOG), hWnd, EventDlgProc, (LPARAM)&newEvent) == IDOK) {
                g_events.push_back(newEvent);
                g_nextEventId++;
                PopulateEventList();
            }
            break;
        }
        case IDC_BTN_EDIT_EVENT:
            selected = ListView_GetNextItem(g_tabControls[0][0], -1, LVNI_SELECTED);
            if (selected != -1) {
                if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_EVENT_DIALOG), hWnd, EventDlgProc, (LPARAM)&g_events[selected]) == IDOK) {
                    PopulateEventList();
                }
            }
            else MessageBoxW(hWnd, L"请选择一个事件。", L"提示", MB_OK);
            break;
        case IDC_BTN_DEL_EVENT:
            selected = ListView_GetNextItem(g_tabControls[0][0], -1, LVNI_SELECTED);
            if (selected != -1) {
                if (MessageBoxW(hWnd, L"确定要删除所选事件吗?", L"确认", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    g_events.erase(g_events.begin() + selected);
                    PopulateEventList();
                }
            }
            else MessageBoxW(hWnd, L"请选择一个事件。", L"提示", MB_OK);
            break;
        case IDC_BTN_EDIT_CLASS:
            selected = ListView_GetNextItem(g_tabControls[1][7], -1, LVNI_SELECTED);
            if (selected != -1) {
                if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_TIMETABLE_DIALOG), hWnd, TimetableDlgProc, (LPARAM)&g_timetables[g_currentDay][selected]) == IDOK) {
                    PopulateTimetableList();
                }
            }
            else MessageBoxW(hWnd, L"请选择一个课程。", L"提示", MB_OK);
            break;
        case IDC_BTN_DEL_CLASS:
            selected = ListView_GetNextItem(g_tabControls[1][7], -1, LVNI_SELECTED);
            if (selected != -1) {
                if (MessageBoxW(hWnd, L"确定要删除所选课程吗?", L"确认", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    g_timetables[g_currentDay].erase(g_timetables[g_currentDay].begin() + selected);
                    PopulateTimetableList();
                }
            }
            else MessageBoxW(hWnd, L"请选择一个课程。", L"提示", MB_OK);
            break;
        case IDC_BTN_CLASS_UP:
            selected = ListView_GetNextItem(g_tabControls[1][7], -1, LVNI_SELECTED);
            if (selected > 0) {
                swap(g_timetables[g_currentDay][selected], g_timetables[g_currentDay][selected - 1]);
                PopulateTimetableList();
                ListView_SetItemState(g_tabControls[1][7], selected - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
            break;
        case IDC_BTN_CLASS_DOWN:
            selected = ListView_GetNextItem(g_tabControls[1][7], -1, LVNI_SELECTED);
            if (selected != -1 && selected < (int)g_timetables[g_currentDay].size() - 1) {
                swap(g_timetables[g_currentDay][selected], g_timetables[g_currentDay][selected + 1]);
                PopulateTimetableList();
                ListView_SetItemState(g_tabControls[1][7], selected + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
            break;
        case IDC_BTN_ADD_COURSE_BLOCK: {
            TemplateItem newItem = { TemplateItemType::COURSE_BLOCK };
            if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_COURSE_BLOCK_DIALOG), hWnd, CourseBlockDlgProc, (LPARAM)&newItem) == IDOK) {
                g_timetableTemplates[g_currentDay].push_back(newItem);
                PopulateTemplateList();
            }
            break;
        }
        case IDC_BTN_ADD_BREAK_BLOCK: {
            TemplateItem newItem = { TemplateItemType::BREAK_BLOCK };
            if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_BREAK_BLOCK_DIALOG), hWnd, BreakBlockDlgProc, (LPARAM)&newItem) == IDOK) {
                g_timetableTemplates[g_currentDay].push_back(newItem);
                PopulateTemplateList();
            }
            break;
        }
        case IDC_BTN_EDIT_BLOCK:
            selected = ListView_GetNextItem(g_tabControls[2][7], -1, LVNI_SELECTED);
            if (selected != -1) {
                TemplateItem* item = &g_timetableTemplates[g_currentDay][selected];
                INT_PTR result = -1;
                if (item->type == TemplateItemType::COURSE_BLOCK) {
                    result = DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_COURSE_BLOCK_DIALOG), hWnd, CourseBlockDlgProc, (LPARAM)item);
                }
                else {
                    result = DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_BREAK_BLOCK_DIALOG), hWnd, BreakBlockDlgProc, (LPARAM)item);
                }
                if (result == IDOK) PopulateTemplateList();
            }
            else MessageBoxW(hWnd, L"请选择一个模板项。", L"提示", MB_OK);
            break;
        case IDC_BTN_DEL_BLOCK:
            selected = ListView_GetNextItem(g_tabControls[2][7], -1, LVNI_SELECTED);
            if (selected != -1) {
                if (MessageBoxW(hWnd, L"确定要删除所选模板项吗?", L"确认", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    g_timetableTemplates[g_currentDay].erase(g_timetableTemplates[g_currentDay].begin() + selected);
                    PopulateTemplateList();
                }
            }
            else MessageBoxW(hWnd, L"请选择一个模板项。", L"提示", MB_OK);
            break;
        case IDC_BTN_BLOCK_UP:
            selected = ListView_GetNextItem(g_tabControls[2][7], -1, LVNI_SELECTED);
            if (selected > 0) {
                swap(g_timetableTemplates[g_currentDay][selected], g_timetableTemplates[g_currentDay][selected - 1]);
                PopulateTemplateList();
                ListView_SetItemState(g_tabControls[2][7], selected - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
            break;
        case IDC_BTN_BLOCK_DOWN:
            selected = ListView_GetNextItem(g_tabControls[2][7], -1, LVNI_SELECTED);
            if (selected != -1 && selected < (int)g_timetableTemplates[g_currentDay].size() - 1) {
                swap(g_timetableTemplates[g_currentDay][selected], g_timetableTemplates[g_currentDay][selected + 1]);
                PopulateTemplateList();
                ListView_SetItemState(g_tabControls[2][7], selected + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
            break;
        case IDC_BTN_GENERATE_TIMETABLE:
            GenerateAndFillTimetable();
            break;
        case IDC_BTN_ADD_COURSE_TO_LIB: {
            wchar_t courseName[256];
            GetWindowTextW(g_tabControls[3][4], courseName, 256);
            if (wcslen(courseName) > 0) {
                g_availableCourses.push_back(courseName);
                PopulateAvailableCoursesList();
                SetWindowTextW(g_tabControls[3][4], L"");
            }
            else MessageBoxW(hWnd, L"请输入课程名称。", L"提示", MB_OK);
            break;
        }
        case IDC_BTN_DEL_COURSE_FROM_LIB:
            selected = ListView_GetNextItem(g_tabControls[3][3], -1, LVNI_SELECTED);
            if (selected != -1) {
                if (MessageBoxW(hWnd, L"确定要删除所选课程吗?", L"确认", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    g_availableCourses.erase(g_availableCourses.begin() + selected);
                    PopulateAvailableCoursesList();
                }
            }
            else MessageBoxW(hWnd, L"请选择要删除的课程。", L"提示", MB_OK);
            break;
        case IDC_CHK_AUTORUN:
            g_isAutoRun = (IsDlgButtonChecked(hWnd, IDC_CHK_AUTORUN) == BST_CHECKED);
            break;
        case IDC_STATIC_CD_TITLE_COLOR:
            ChooseColorFor(hWnd, &g_countdownSettings.titleFontColor, g_tabControls[4][8]);
            break;
        case IDC_STATIC_CD_TIME_COLOR:
            ChooseColorFor(hWnd, &g_countdownSettings.timeFontColor, g_tabControls[4][10]);
            break;
        case IDC_STATIC_TT_HEADER_COLOR:
            ChooseColorFor(hWnd, &g_timetableSettings.headerFontColor, g_tabControls[4][12]);
            break;
        case IDC_STATIC_TT_ENTRY_COLOR:
            ChooseColorFor(hWnd, &g_timetableSettings.entryFontColor, g_tabControls[4][14]);
            break;
        case IDC_STATIC_TT_TIME_COLOR:
            ChooseColorFor(hWnd, &g_timetableSettings.timeFontColor, g_tabControls[4][16]);
            break;
        case IDC_STATIC_TT_HIGHLIGHT_COLOR:
            ChooseColorFor(hWnd, &g_timetableSettings.highlightColor, g_tabControls[4][18]);
            break;
        case IDC_BTN_SAVE:
            wchar_t buffer[256];
            GetWindowTextW(g_tabControls[4][2], buffer, 256);
            g_countdownSettings.title = buffer;
            DateTime_GetSystemtime(g_tabControls[4][4], &g_countdownSettings.targetTime);
            SYSTEMTIME timeOnly;
            DateTime_GetSystemtime(g_tabControls[4][5], &timeOnly);
            g_countdownSettings.targetTime.wHour = timeOnly.wHour;
            g_countdownSettings.targetTime.wMinute = timeOnly.wMinute;
            g_countdownSettings.targetTime.wSecond = 0;
            g_countdownSettings.titleFontSize = GetDlgItemInt(hWnd, IDC_EDIT_CD_TITLE_FONTSIZE, NULL, FALSE);
            g_countdownSettings.timeFontSize = GetDlgItemInt(hWnd, IDC_EDIT_CD_TIME_FONTSIZE, NULL, FALSE);
            g_timetableSettings.headerFontSize = GetDlgItemInt(hWnd, IDC_EDIT_TT_HEADER_FONTSIZE, NULL, FALSE);
            g_timetableSettings.entryFontSize = GetDlgItemInt(hWnd, IDC_EDIT_TT_ENTRY_FONTSIZE, NULL, FALSE);
            g_templateStartH[g_currentDay] = GetDlgItemInt(hWnd, IDC_EDIT_TEMPLATE_START_H, NULL, FALSE);
            g_templateStartM[g_currentDay] = GetDlgItemInt(hWnd, IDC_EDIT_TEMPLATE_START_M, NULL, FALSE);
            SaveConfiguration();
            break;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        HWND hStatic = (HWND)lParam;
        if (hStatic == g_tabControls[4][8]) {
            SetBkColor(hdcStatic, g_countdownSettings.titleFontColor);
            return (INT_PTR)CreateSolidBrush(g_countdownSettings.titleFontColor);
        }
        if (hStatic == g_tabControls[4][10]) {
            SetBkColor(hdcStatic, g_countdownSettings.timeFontColor);
            return (INT_PTR)CreateSolidBrush(g_countdownSettings.timeFontColor);
        }
        if (hStatic == g_tabControls[4][12]) {
            SetBkColor(hdcStatic, g_timetableSettings.headerFontColor);
            return (INT_PTR)CreateSolidBrush(g_timetableSettings.headerFontColor);
        }
        if (hStatic == g_tabControls[4][14]) {
            SetBkColor(hdcStatic, g_timetableSettings.entryFontColor);
            return (INT_PTR)CreateSolidBrush(g_timetableSettings.entryFontColor);
        }
        if (hStatic == g_tabControls[4][16]) {
            SetBkColor(hdcStatic, g_timetableSettings.timeFontColor);
            return (INT_PTR)CreateSolidBrush(g_timetableSettings.timeFontColor);
        }
        if (hStatic == g_tabControls[4][18]) {
            SetBkColor(hdcStatic, g_timetableSettings.highlightColor);
            return (INT_PTR)CreateSolidBrush(g_timetableSettings.highlightColor);
        }
        break;
    }
    case WM_DESTROY:
        if (g_hAppIcon) DestroyIcon(g_hAppIcon);
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// --- UI初始化和管理 ---
void InitUI(HWND hWnd) {
    g_hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");

    if (!g_hFont) {
        g_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    g_hTabCtrl = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 5, 5, SCREEN_WIDTH - 25, SCREEN_HEIGHT - 100, hWnd, (HMENU)IDC_TAB_CONTROL, g_hInstance, NULL);
    SendMessage(g_hTabCtrl, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    TCITEMW tie = { 0 };
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(L"计划事件"); TabCtrl_InsertItem(g_hTabCtrl, 0, &tie);
    tie.pszText = const_cast<LPWSTR>(L"课表预览"); TabCtrl_InsertItem(g_hTabCtrl, 1, &tie);
    tie.pszText = const_cast<LPWSTR>(L"课表模板"); TabCtrl_InsertItem(g_hTabCtrl, 2, &tie);
    tie.pszText = const_cast<LPWSTR>(L"程序设置"); TabCtrl_InsertItem(g_hTabCtrl, 3, &tie);
    tie.pszText = const_cast<LPWSTR>(L"小组件设置"); TabCtrl_InsertItem(g_hTabCtrl, 4, &tie);

    // Tab 0: 计划事件
    HWND hEventList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, 15, 45, 670, 490, hWnd, (HMENU)IDC_LIST_EVENTS, g_hInstance, NULL);
    ListView_SetExtendedListViewStyle(hEventList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    LVCOLUMNW lvc = { 0 }; lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = const_cast<LPWSTR>(L"节名称"); lvc.cx = 120; ListView_InsertColumn(hEventList, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>(L"时间"); lvc.cx = 80; ListView_InsertColumn(hEventList, 1, &lvc);
    lvc.pszText = const_cast<LPWSTR>(L"类型"); lvc.cx = 100; ListView_InsertColumn(hEventList, 2, &lvc);
    lvc.pszText = const_cast<LPWSTR>(L"参数"); lvc.cx = 340; ListView_InsertColumn(hEventList, 3, &lvc);
    g_tabControls[0].push_back(hEventList);
    g_tabControls[0].push_back(CreateWindowW(L"BUTTON", L"添加", WS_CHILD, 15, 545, 80, 30, hWnd, (HMENU)IDC_BTN_ADD_EVENT, g_hInstance, NULL));
    g_tabControls[0].push_back(CreateWindowW(L"BUTTON", L"编辑", WS_CHILD, 105, 545, 80, 30, hWnd, (HMENU)IDC_BTN_EDIT_EVENT, g_hInstance, NULL));
    g_tabControls[0].push_back(CreateWindowW(L"BUTTON", L"删除", WS_CHILD, 195, 545, 80, 30, hWnd, (HMENU)IDC_BTN_DEL_EVENT, g_hInstance, NULL));

    const wchar_t* days[] = { L"周一", L"周二", L"周三", L"周四", L"周五", L"周六", L"周日" };
    int dayButtonWidth = 90, dayButtonHeight = 25, dayButtonSpacing = 5, dayButtonsTop = 45, dayButtonsLeft = 15;
    int listTop = dayButtonsTop + dayButtonHeight + 10, listLeft = 15, listWidth = 670;

    // Tab 1: 课表预览
    int previewListHeight = 545 - listTop - 10;
    int previewBottomButtonsTop = listTop + previewListHeight + 10;
    for (int i = 0; i < 7; ++i) {
        g_tabControls[1].push_back(CreateWindowW(L"BUTTON", days[i], WS_CHILD | WS_TABSTOP, dayButtonsLeft + i * (dayButtonWidth + dayButtonSpacing), dayButtonsTop, dayButtonWidth, dayButtonHeight, hWnd, (HMENU)(IDC_BTN_DAY_PREVIEW_START + i), g_hInstance, NULL));
    }
    HWND hTimetableList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, listLeft, listTop, listWidth, previewListHeight, hWnd, (HMENU)IDC_LIST_TIMETABLE, g_hInstance, NULL);
    ListView_SetExtendedListViewStyle(hTimetableList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    lvc.pszText = const_cast<LPWSTR>(L"课程/活动"); lvc.cx = (listWidth / 2) - 10; ListView_InsertColumn(hTimetableList, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>(L"时间"); lvc.cx = (listWidth / 2) - 10; ListView_InsertColumn(hTimetableList, 1, &lvc);
    g_tabControls[1].push_back(hTimetableList);
    g_tabControls[1].push_back(CreateWindowW(L"BUTTON", L"编辑", WS_CHILD, listLeft, previewBottomButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_EDIT_CLASS, g_hInstance, NULL));
    g_tabControls[1].push_back(CreateWindowW(L"BUTTON", L"删除", WS_CHILD, listLeft + 90, previewBottomButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_DEL_CLASS, g_hInstance, NULL));
    g_tabControls[1].push_back(CreateWindowW(L"BUTTON", L"上移", WS_CHILD, listLeft + 210, previewBottomButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_CLASS_UP, g_hInstance, NULL));
    g_tabControls[1].push_back(CreateWindowW(L"BUTTON", L"下移", WS_CHILD, listLeft + 300, previewBottomButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_CLASS_DOWN, g_hInstance, NULL));

    // Tab 2: 课表模板
    int templateListHeight = 465 - listTop - 5;
    int templateStartTimeTop = listTop + templateListHeight + 10;
    int templateActionButtonsTop = templateStartTimeTop + 30;
    int templateMoveButtonsTop = templateActionButtonsTop + 40;
    for (int i = 0; i < 7; ++i) {
        g_tabControls[2].push_back(CreateWindowW(L"BUTTON", days[i], WS_CHILD | WS_TABSTOP, dayButtonsLeft + i * (dayButtonWidth + dayButtonSpacing), dayButtonsTop, dayButtonWidth, dayButtonHeight, hWnd, (HMENU)(IDC_BTN_DAY_TEMPLATE_START + i), g_hInstance, NULL));
    }
    HWND hTemplateList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, listLeft, listTop, listWidth, templateListHeight, hWnd, (HMENU)IDC_LIST_TEMPLATE, g_hInstance, NULL);
    ListView_SetExtendedListViewStyle(hTemplateList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    lvc.pszText = const_cast<LPWSTR>(L"模板项 (课程块 / 休息块)"); lvc.cx = listWidth - 10; ListView_InsertColumn(hTemplateList, 0, &lvc);
    g_tabControls[2].push_back(hTemplateList);
    g_tabControls[2].push_back(CreateWindowW(L"STATIC", L"首节课开始时间:", WS_CHILD | SS_RIGHT, listLeft, templateStartTimeTop, 120, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_NUMBER, listLeft + 125, templateStartTimeTop, 35, 20, hWnd, (HMENU)IDC_EDIT_TEMPLATE_START_H, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"STATIC", L":", WS_CHILD | SS_CENTER, listLeft + 165, templateStartTimeTop, 10, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_NUMBER, listLeft + 180, templateStartTimeTop, 35, 20, hWnd, (HMENU)IDC_EDIT_TEMPLATE_START_M, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"BUTTON", L"添加课程块", WS_CHILD, listLeft, templateActionButtonsTop, 120, 30, hWnd, (HMENU)IDC_BTN_ADD_COURSE_BLOCK, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"BUTTON", L"添加休息块", WS_CHILD, listLeft + 130, templateActionButtonsTop, 120, 30, hWnd, (HMENU)IDC_BTN_ADD_BREAK_BLOCK, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"BUTTON", L"编辑项", WS_CHILD, listLeft + 260, templateActionButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_EDIT_BLOCK, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"BUTTON", L"删除项", WS_CHILD, listLeft + 350, templateActionButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_DEL_BLOCK, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"BUTTON", L"上移", WS_CHILD, listLeft, templateMoveButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_BLOCK_UP, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"BUTTON", L"下移", WS_CHILD, listLeft + 90, templateMoveButtonsTop, 80, 30, hWnd, (HMENU)IDC_BTN_BLOCK_DOWN, g_hInstance, NULL));
    g_tabControls[2].push_back(CreateWindowW(L"BUTTON", L"生成并填充课表 >>", WS_CHILD | BS_DEFPUSHBUTTON, listLeft + listWidth - 160, templateMoveButtonsTop, 160, 30, hWnd, (HMENU)IDC_BTN_GENERATE_TIMETABLE, g_hInstance, NULL));

    // Tab 3: 程序设置
    g_tabControls[3].push_back(CreateWindowW(L"STATIC", L"程序选项:", WS_CHILD, 15, 45, 150, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[3].push_back(CreateWindowW(L"BUTTON", L"开机时自动启动主程序", WS_CHILD | BS_AUTOCHECKBOX, 15, 70, 250, 30, hWnd, (HMENU)IDC_CHK_AUTORUN, g_hInstance, NULL));
    g_tabControls[3].push_back(CreateWindowW(L"STATIC", L"课程库管理:", WS_CHILD, 350, 45, 150, 20, hWnd, NULL, g_hInstance, NULL));
    HWND hCourseList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_BORDER | LVS_SHOWSELALWAYS, 350, 70, 335, 430, hWnd, (HMENU)IDC_LIST_AVAILABLE_COURSES, g_hInstance, NULL);
    ListView_SetExtendedListViewStyle(hCourseList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    lvc.pszText = const_cast<LPWSTR>(L"课程名称"); lvc.cx = 325; ListView_InsertColumn(hCourseList, 0, &lvc);
    g_tabControls[3].push_back(hCourseList);
    g_tabControls[3].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 350, 510, 225, 25, hWnd, (HMENU)IDC_EDIT_NEW_COURSE_NAME, g_hInstance, NULL));
    g_tabControls[3].push_back(CreateWindowW(L"BUTTON", L"添加课程", WS_CHILD, 585, 510, 100, 25, hWnd, (HMENU)IDC_BTN_ADD_COURSE_TO_LIB, g_hInstance, NULL));
    g_tabControls[3].push_back(CreateWindowW(L"BUTTON", L"删除选中课程", WS_CHILD, 565, 545, 120, 25, hWnd, (HMENU)IDC_BTN_DEL_COURSE_FROM_LIB, g_hInstance, NULL));

    // Tab 4: 小组件设置
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"倒计时设置", WS_CHILD | BS_GROUPBOX, 15, 45, 670, 200, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"标题:", WS_CHILD | SS_RIGHT, 30, 70, 80, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER, 120, 70, 400, 22, hWnd, (HMENU)IDC_EDIT_CD_TITLE, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"目标日期:", WS_CHILD | SS_RIGHT, 30, 100, 80, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_CHILD | DTS_SHORTDATEFORMAT, 120, 100, 120, 22, hWnd, (HMENU)IDC_DTP_CD_DATE, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_CHILD | DTS_TIMEFORMAT, 250, 100, 100, 22, hWnd, (HMENU)IDC_DTP_CD_TIME, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"标题字体:", WS_CHILD | SS_RIGHT, 30, 130, 80, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_NUMBER, 120, 130, 60, 22, hWnd, (HMENU)IDC_EDIT_CD_TITLE_FONTSIZE, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"", WS_CHILD | WS_BORDER | SS_NOTIFY, 190, 130, 50, 22, hWnd, (HMENU)IDC_STATIC_CD_TITLE_COLOR, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"时间字体:", WS_CHILD | SS_RIGHT, 30, 160, 80, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_NUMBER, 120, 160, 60, 22, hWnd, (HMENU)IDC_EDIT_CD_TIME_FONTSIZE, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"", WS_CHILD | WS_BORDER | SS_NOTIFY, 190, 160, 50, 22, hWnd, (HMENU)IDC_STATIC_CD_TIME_COLOR, g_hInstance, NULL));

    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"课表设置", WS_CHILD | BS_GROUPBOX, 15, 255, 670, 200, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"标题字体:", WS_CHILD | SS_RIGHT, 30, 285, 120, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_NUMBER, 160, 285, 60, 22, hWnd, (HMENU)IDC_EDIT_TT_HEADER_FONTSIZE, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"", WS_CHILD | WS_BORDER | SS_NOTIFY, 230, 285, 50, 22, hWnd, (HMENU)IDC_STATIC_TT_HEADER_COLOR, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"内容字体:", WS_CHILD | SS_RIGHT, 30, 315, 120, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_NUMBER, 160, 315, 60, 22, hWnd, (HMENU)IDC_EDIT_TT_ENTRY_FONTSIZE, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"", WS_CHILD | WS_BORDER | SS_NOTIFY, 230, 315, 50, 22, hWnd, (HMENU)IDC_STATIC_TT_ENTRY_COLOR, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"时间颜色:", WS_CHILD | SS_RIGHT, 30, 345, 120, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"", WS_CHILD | WS_BORDER | SS_NOTIFY, 160, 345, 50, 22, hWnd, (HMENU)IDC_STATIC_TT_TIME_COLOR, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"当前课程高亮:", WS_CHILD | SS_RIGHT, 30, 375, 120, 20, hWnd, NULL, g_hInstance, NULL));
    g_tabControls[4].push_back(CreateWindowW(L"STATIC", L"", WS_CHILD | WS_BORDER | SS_NOTIFY, 160, 375, 50, 22, hWnd, (HMENU)IDC_STATIC_TT_HIGHLIGHT_COLOR, g_hInstance, NULL));

    HWND hSaveBtn = CreateWindowW(L"BUTTON", L"保存所有配置", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, SCREEN_WIDTH - 180, SCREEN_HEIGHT - 85, 150, 40, hWnd, (HMENU)IDC_BTN_SAVE, g_hInstance, NULL);

    for (int i = 0; i < 5; ++i) {
        for (HWND hCtrl : g_tabControls[i]) {
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        }
    }
    SendMessage(hSaveBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    for (int i = 0; i < 5; ++i) {
        for (HWND hCtrl : g_tabControls[i]) {
            ShowWindow(hCtrl, SW_HIDE);
        }
    }
}

void SwitchTab(int tabIndex) {
    for (int i = 0; i < 5; ++i) {
        for (HWND hCtrl : g_tabControls[i]) {
            ShowWindow(hCtrl, SW_HIDE);
        }
    }
    if (tabIndex >= 0 && tabIndex < 5) {
        for (HWND hCtrl : g_tabControls[tabIndex]) {
            ShowWindow(hCtrl, SW_SHOW);
        }
    }
}

void SelectDay(int dayIndex) {
    if (g_currentDay == dayIndex) return;

    EnableWindow(g_tabControls[1][g_currentDay], TRUE);
    EnableWindow(g_tabControls[2][g_currentDay], TRUE);

    g_currentDay = dayIndex;

    EnableWindow(g_tabControls[1][g_currentDay], FALSE);
    EnableWindow(g_tabControls[2][g_currentDay], FALSE); // FIX: Corrected g__tabControls to g_tabControls

    PopulateTimetableList();
    PopulateTemplateList();
}


void GenerateAndFillTimetable() {
    g_templateStartH[g_currentDay] = GetDlgItemInt(g_hMainWnd, IDC_EDIT_TEMPLATE_START_H, NULL, FALSE);
    g_templateStartM[g_currentDay] = GetDlgItemInt(g_hMainWnd, IDC_EDIT_TEMPLATE_START_M, NULL, FALSE);

    if (g_timetableTemplates[g_currentDay].empty()) {
        MessageBoxW(g_hMainWnd, L"当前日期的课表模板为空，无法生成。", L"提示", MB_OK);
        return;
    }

    vector<TimetableEntry> generatedSlots;
    int currentHour = g_templateStartH[g_currentDay];
    int currentMinute = g_templateStartM[g_currentDay];
    int classCounter = 1;

    for (const auto& item : g_timetableTemplates[g_currentDay]) {
        if (item.type == TemplateItemType::BREAK_BLOCK) {
            wchar_t timeBuf[50];
            swprintf_s(timeBuf, L"%02d:%02d-%02d:%02d", item.startHour, item.startMinute, item.endHour, item.endMinute);
            generatedSlots.push_back({ item.breakName, timeBuf, 0 });
            currentHour = item.endHour;
            currentMinute = item.endMinute;
        }
        else { // COURSE_BLOCK
            for (int i = 0; i < item.consecutiveClasses; ++i) {
                int startH = currentHour;
                int startM = currentMinute;
                int totalMinutes = startH * 60 + startM + item.classDuration;
                int endH = totalMinutes / 60;
                int endM = totalMinutes % 60;

                wchar_t timeBuf[50];
                swprintf_s(timeBuf, L"%02d:%02d-%02d:%02d", startH, startM, endH, endM);

                // **智能填充逻辑**
                wstring subject = L"待定课程";
                for (const auto& existingEntry : g_timetables[g_currentDay]) {
                    if (existingEntry.time == timeBuf) {
                        subject = existingEntry.subject;
                        break;
                    }
                }
                generatedSlots.push_back({ subject, timeBuf, classCounter++ });

                if (i < item.consecutiveClasses - 1) {
                    totalMinutes += item.breakDuration;
                    currentHour = totalMinutes / 60;
                    currentMinute = totalMinutes % 60;
                }
                else {
                    currentHour = endH;
                    currentMinute = endM;
                }
            }
        }
    }

    if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_FILL_COURSES_DIALOG), g_hMainWnd, FillCoursesDlgProc, (LPARAM)&generatedSlots) == IDOK) {
        g_timetables[g_currentDay] = generatedSlots;
        PopulateTimetableList();
        TabCtrl_SetCurSel(g_hTabCtrl, 1);
        SwitchTab(1);
    }
}

HICON LoadAppIcon(HINSTANCE hInstance) {
    HICON hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (hIcon) {
        return hIcon;
    }
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);
    PathAppendW(szPath, ICON_FILE_NAME);

    if (GetFileAttributesW(szPath) == INVALID_FILE_ATTRIBUTES) {
        URLDownloadToFileW(NULL, ICON_DOWNLOAD_URL, szPath, 0, NULL);
    }

    return (HICON)LoadImageW(NULL, szPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
}

void ChooseColorFor(HWND hWnd, COLORREF* colorRef, HWND hStaticSample) {
    CHOOSECOLOR cc = { sizeof(cc) };
    static COLORREF acrCustClr[16];
    cc.hwndOwner = hWnd;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = *colorRef;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc) == TRUE) {
        *colorRef = cc.rgbResult;
        InvalidateRect(hStaticSample, NULL, TRUE);
    }
}

// --- 主程序入口 ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInstance = hInstance;
    InitCommonControls();

    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName = L"ConfiguratorWindowClass";
    g_hAppIcon = LoadAppIcon(hInstance);
    if (g_hAppIcon) {
        wcex.hIcon = g_hAppIcon;
        wcex.hIconSm = g_hAppIcon;
    }
    RegisterClassExW(&wcex);

    g_hMainWnd = CreateWindowW(L"ConfiguratorWindowClass", L"新闻调度器配置 v7.8", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, 0, SCREEN_WIDTH, SCREEN_HEIGHT, nullptr, nullptr, hInstance, nullptr);

    if (!g_hMainWnd) return FALSE;

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(g_hMainWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

// --- 对话框过程 ---
INT_PTR CALLBACK EventDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static ScheduledEvent* currentEvent = nullptr;

    switch (message) {
    case WM_INITDIALOG: {
        currentEvent = (ScheduledEvent*)lParam;
        SendMessage(hDlg, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lParam) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
            }, (LPARAM)g_hFont);

        SetWindowTextW(hDlg, (L"编辑事件: " + currentEvent->sectionName).c_str());

        HWND hType = GetDlgItem(hDlg, IDD_EDIT_EVENT_TYPE);
        SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"打开网页");
        SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"系统操作");
        SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"关闭浏览器");
        SendMessageW(hType, CB_SETCURSEL, (int)currentEvent->type - 1, 0);

        HWND hSys = GetDlgItem(hDlg, IDD_COMBO_EVENT_SYS_PARAMS);
        SendMessageW(hSys, CB_ADDSTRING, 0, (LPARAM)L"Shutdown");
        SendMessageW(hSys, CB_ADDSTRING, 0, (LPARAM)L"Reboot");
        SendMessageW(hSys, CB_ADDSTRING, 0, (LPARAM)L"Logoff");

        SetDlgItemInt(hDlg, IDD_EDIT_EVENT_HOUR, currentEvent->hour, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_EVENT_MINUTE, currentEvent->minute, FALSE);
        SetDlgItemTextW(hDlg, IDD_EDIT_EVENT_PARAMS, currentEvent->parameters.c_str());

        CheckDlgButton(hDlg, IDD_CHK_VOLUME_FADE, currentEvent->enableVolumeFade ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hDlg, IDD_EDIT_VOL_START, currentEvent->startVolume, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_VOL_END, currentEvent->endVolume, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_FADE_DUR, currentEvent->fadeDuration, FALSE);

        CheckDlgButton(hDlg, IDD_CHK_MOUSE_CLICK, currentEvent->enableMouseClick ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hDlg, IDD_EDIT_CLICK_X, currentEvent->clickX, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_CLICK_Y, currentEvent->clickY, FALSE);

        SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDD_EDIT_EVENT_TYPE, CBN_SELCHANGE), (LPARAM)GetDlgItem(hDlg, IDD_EDIT_EVENT_TYPE));
        SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDD_CHK_VOLUME_FADE, BN_CLICKED), 0);
        SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDD_CHK_MOUSE_CLICK, BN_CLICKED), 0);

        return (INT_PTR)TRUE;
    }
    case WM_COMMAND: {
        WORD wmId = LOWORD(wParam);
        WORD wmEvent = HIWORD(wParam);

        if (wmId == IDD_EDIT_EVENT_TYPE && wmEvent == CBN_SELCHANGE) {
            int sel = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
            EventType type = (EventType)(sel + 1);
            bool isUrl = (type == EventType::OpenURL);
            bool isSys = (type == EventType::SystemAction);
            ShowWindow(GetDlgItem(hDlg, IDC_STATIC_PARAM_LABEL), isUrl || isSys);
            ShowWindow(GetDlgItem(hDlg, IDD_EDIT_EVENT_PARAMS), isUrl);
            ShowWindow(GetDlgItem(hDlg, IDD_COMBO_EVENT_SYS_PARAMS), isSys);
            const int volumeControls[] = { IDC_GROUP_VOLUME, IDD_CHK_VOLUME_FADE, IDC_STATIC_VOL_LABEL_1, IDD_EDIT_VOL_START, IDC_STATIC_VOL_ARROW, IDD_EDIT_VOL_END, IDC_STATIC_VOL_LABEL_2, IDD_EDIT_FADE_DUR };
            const int clickControls[] = { IDC_GROUP_CLICK, IDD_CHK_MOUSE_CLICK, IDC_STATIC_CLICK_LABEL, IDD_EDIT_CLICK_X, IDD_EDIT_CLICK_Y };
            for (int idc : volumeControls) ShowWindow(GetDlgItem(hDlg, idc), isUrl);
            for (int idc : clickControls) ShowWindow(GetDlgItem(hDlg, idc), isUrl);
            if (!isUrl) {
                CheckDlgButton(hDlg, IDD_CHK_VOLUME_FADE, BST_UNCHECKED);
                CheckDlgButton(hDlg, IDD_CHK_MOUSE_CLICK, BST_UNCHECKED);
                SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDD_CHK_VOLUME_FADE, BN_CLICKED), 0);
                SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDD_CHK_MOUSE_CLICK, BN_CLICKED), 0);
            }
        }
        else if (wmId == IDD_CHK_VOLUME_FADE) {
            BOOL checked = IsDlgButtonChecked(hDlg, IDD_CHK_VOLUME_FADE);
            EnableWindow(GetDlgItem(hDlg, IDD_EDIT_VOL_START), checked);
            EnableWindow(GetDlgItem(hDlg, IDD_EDIT_VOL_END), checked);
            EnableWindow(GetDlgItem(hDlg, IDD_EDIT_FADE_DUR), checked);
        }
        else if (wmId == IDD_CHK_MOUSE_CLICK) {
            BOOL checked = IsDlgButtonChecked(hDlg, IDD_CHK_MOUSE_CLICK);
            EnableWindow(GetDlgItem(hDlg, IDD_EDIT_CLICK_X), checked);
            EnableWindow(GetDlgItem(hDlg, IDD_EDIT_CLICK_Y), checked);
        }
        else if (wmId == IDOK) {
            currentEvent->type = (EventType)(SendMessage(GetDlgItem(hDlg, IDD_EDIT_EVENT_TYPE), CB_GETCURSEL, 0, 0) + 1);
            currentEvent->hour = GetDlgItemInt(hDlg, IDD_EDIT_EVENT_HOUR, NULL, FALSE);
            currentEvent->minute = GetDlgItemInt(hDlg, IDD_EDIT_EVENT_MINUTE, NULL, FALSE);
            wchar_t buffer[2048] = { 0 };
            if (currentEvent->type == EventType::SystemAction) {
                int sel = SendMessage(GetDlgItem(hDlg, IDD_COMBO_EVENT_SYS_PARAMS), CB_GETCURSEL, 0, 0);
                SendMessage(GetDlgItem(hDlg, IDD_COMBO_EVENT_SYS_PARAMS), CB_GETLBTEXT, sel, (LPARAM)buffer);
            }
            else if (currentEvent->type == EventType::OpenURL) {
                GetDlgItemTextW(hDlg, IDD_EDIT_EVENT_PARAMS, buffer, 2048);
            }
            currentEvent->parameters = buffer;
            currentEvent->enableVolumeFade = IsDlgButtonChecked(hDlg, IDD_CHK_VOLUME_FADE);
            currentEvent->startVolume = GetDlgItemInt(hDlg, IDD_EDIT_VOL_START, NULL, FALSE);
            currentEvent->endVolume = GetDlgItemInt(hDlg, IDD_EDIT_VOL_END, NULL, FALSE);
            currentEvent->fadeDuration = GetDlgItemInt(hDlg, IDD_EDIT_FADE_DUR, NULL, FALSE);
            currentEvent->enableMouseClick = IsDlgButtonChecked(hDlg, IDD_CHK_MOUSE_CLICK);
            currentEvent->clickX = GetDlgItemInt(hDlg, IDD_EDIT_CLICK_X, NULL, FALSE);
            currentEvent->clickY = GetDlgItemInt(hDlg, IDD_EDIT_CLICK_Y, NULL, FALSE);
            EndDialog(hDlg, IDOK);
        }
        else if (wmId == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
        }
        return (INT_PTR)TRUE;
    }
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK TimetableDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static TimetableEntry* currentEntry = nullptr;
    switch (message) {
    case WM_INITDIALOG: {
        currentEntry = (TimetableEntry*)lParam;
        SendMessage(hDlg, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lParam) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
            }, (LPARAM)g_hFont);
        HWND hCombo = GetDlgItem(hDlg, IDD_COMBO_CLASS_SUBJECT);
        for (const auto& course : g_availableCourses) {
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)course.c_str());
        }
        int sel = SendMessage(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)currentEntry->subject.c_str());
        SendMessage(hCombo, CB_SETCURSEL, sel, 0);
        SetDlgItemTextW(hDlg, IDD_EDIT_CLASS_TIME, currentEntry->time.c_str());
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDOK) {
            wchar_t buffer[256];
            int sel = SendMessage(GetDlgItem(hDlg, IDD_COMBO_CLASS_SUBJECT), CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) {
                SendMessage(GetDlgItem(hDlg, IDD_COMBO_CLASS_SUBJECT), CB_GETLBTEXT, sel, (LPARAM)buffer);
                currentEntry->subject = buffer;
            }
            GetDlgItemTextW(hDlg, IDD_EDIT_CLASS_TIME, buffer, 256);
            currentEntry->time = buffer;
            EndDialog(hDlg, IDOK);
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
        }
        return (INT_PTR)TRUE;
    }
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK CourseBlockDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static TemplateItem* currentItem = nullptr;
    switch (message) {
    case WM_INITDIALOG: {
        currentItem = (TemplateItem*)lParam;
        SendMessage(hDlg, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lParam) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
            }, (LPARAM)g_hFont);
        SetDlgItemInt(hDlg, IDD_EDIT_CONSECUTIVE_COUNT, currentItem->consecutiveClasses, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_CLASS_DURATION, currentItem->classDuration, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_BREAK_DURATION, currentItem->breakDuration, FALSE);
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDOK) {
            currentItem->consecutiveClasses = GetDlgItemInt(hDlg, IDD_EDIT_CONSECUTIVE_COUNT, NULL, FALSE);
            currentItem->classDuration = GetDlgItemInt(hDlg, IDD_EDIT_CLASS_DURATION, NULL, FALSE);
            currentItem->breakDuration = GetDlgItemInt(hDlg, IDD_EDIT_BREAK_DURATION, NULL, FALSE);
            currentItem->displayName = L"按此规则课程数: " + to_wstring(currentItem->consecutiveClasses)
                + L", 每节" + to_wstring(currentItem->classDuration) + L"分, 休息" + to_wstring(currentItem->breakDuration) + L"分";
            EndDialog(hDlg, IDOK);
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
        }
        return (INT_PTR)TRUE;
    }
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK BreakBlockDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static TemplateItem* currentItem = nullptr;
    switch (message) {
    case WM_INITDIALOG: {
        currentItem = (TemplateItem*)lParam;
        SendMessage(hDlg, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lParam) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
            }, (LPARAM)g_hFont);
        HWND hCombo = GetDlgItem(hDlg, IDD_COMBO_BREAK_TYPE);
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"早饭");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"大课间");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"午休");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"晚饭");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"晚自习");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"自定义");
        int sel = SendMessage(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)currentItem->breakName.c_str());
        if (sel == CB_ERR) {
            SendMessage(hCombo, CB_SETCURSEL, 5, 0);
            SetDlgItemTextW(hDlg, IDD_EDIT_CUSTOM_BREAK_NAME, currentItem->breakName.c_str());
        }
        else {
            SendMessage(hCombo, CB_SETCURSEL, sel, 0);
        }
        SetDlgItemInt(hDlg, IDD_EDIT_BREAK_START_H, currentItem->startHour, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_BREAK_START_M, currentItem->startMinute, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_BREAK_END_H, currentItem->endHour, FALSE);
        SetDlgItemInt(hDlg, IDD_EDIT_BREAK_END_M, currentItem->endMinute, FALSE);
        SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDD_COMBO_BREAK_TYPE, CBN_SELCHANGE), (LPARAM)hCombo);
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND: {
        WORD wmId = LOWORD(wParam);
        if (wmId == IDD_COMBO_BREAK_TYPE && HIWORD(wParam) == CBN_SELCHANGE) {
            int sel = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
            EnableWindow(GetDlgItem(hDlg, IDD_EDIT_CUSTOM_BREAK_NAME), sel == 5);
        }
        else if (wmId == IDOK) {
            int sel = SendMessage(GetDlgItem(hDlg, IDD_COMBO_BREAK_TYPE), CB_GETCURSEL, 0, 0);
            wchar_t buffer[100];
            if (sel == 5) {
                GetDlgItemTextW(hDlg, IDD_EDIT_CUSTOM_BREAK_NAME, buffer, 100);
            }
            else {
                SendMessage(GetDlgItem(hDlg, IDD_COMBO_BREAK_TYPE), CB_GETLBTEXT, sel, (LPARAM)buffer);
            }
            currentItem->breakName = buffer;
            currentItem->startHour = GetDlgItemInt(hDlg, IDD_EDIT_BREAK_START_H, NULL, FALSE);
            currentItem->startMinute = GetDlgItemInt(hDlg, IDD_EDIT_BREAK_START_M, NULL, FALSE);
            currentItem->endHour = GetDlgItemInt(hDlg, IDD_EDIT_BREAK_END_H, NULL, FALSE);
            currentItem->endMinute = GetDlgItemInt(hDlg, IDD_EDIT_BREAK_END_M, NULL, FALSE);
            wchar_t timeBuf[100];
            swprintf_s(timeBuf, L"%s (%02d:%02d - %02d:%02d)", currentItem->breakName.c_str(), currentItem->startHour, currentItem->startMinute, currentItem->endHour, currentItem->endMinute);
            currentItem->displayName = timeBuf;
            EndDialog(hDlg, IDOK);
        }
        else if (wmId == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
        }
        return (INT_PTR)TRUE;
    }
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK FillCoursesDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static vector<TimetableEntry>* slots = nullptr;
    static HWND hList = NULL;
    static HWND hEditCombo = NULL;
    static int editItem = -1;

    auto SaveAndDestroyCombo = [&]() {
        if (hEditCombo && editItem != -1) {
            int sel = SendMessage(hEditCombo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) {
                wchar_t buffer[100];
                SendMessage(hEditCombo, CB_GETLBTEXT, sel, (LPARAM)buffer);
                (*slots)[editItem].subject = buffer;
                ListView_SetItemText(hList, editItem, 2, buffer);
            }
            DestroyWindow(hEditCombo);
            hEditCombo = NULL;
            editItem = -1;
        }
        };

    switch (message) {
    case WM_INITDIALOG: {
        slots = (vector<TimetableEntry>*)lParam;
        hList = GetDlgItem(hDlg, IDC_LIST_FILL_COURSES);
        hEditCombo = NULL;
        editItem = -1;
        SendMessage(hDlg, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lParam) -> BOOL {
            SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
            }, (LPARAM)g_hFont);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LVCOLUMNW lvc = { 0 }; lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = const_cast<LPWSTR>(L"课程序号"); lvc.cx = 80; ListView_InsertColumn(hList, 0, &lvc);
        lvc.pszText = const_cast<LPWSTR>(L"时间段"); lvc.cx = 100; ListView_InsertColumn(hList, 1, &lvc);
        lvc.pszText = const_cast<LPWSTR>(L"选择课程 (单击编辑)"); lvc.cx = 150; ListView_InsertColumn(hList, 2, &lvc);
        for (size_t i = 0; i < slots->size(); ++i) {
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_PARAM;
            lvi.iItem = (int)i;
            lvi.lParam = (LPARAM)i;
            wchar_t classNumBuf[20] = L"";
            if ((*slots)[i].classNumber > 0) {
                swprintf_s(classNumBuf, L"第 %d 节", (*slots)[i].classNumber);
            }
            lvi.pszText = classNumBuf;
            ListView_InsertItem(hList, &lvi);
            ListView_SetItemText(hList, i, 1, const_cast<LPWSTR>((*slots)[i].time.c_str()));
            ListView_SetItemText(hList, i, 2, const_cast<LPWSTR>((*slots)[i].subject.c_str()));
        }
        return (INT_PTR)TRUE;
    }
    case WM_NOTIFY: {
        LPNMHDR lpnm = (LPNMHDR)lParam;
        if (lpnm->idFrom == IDC_LIST_FILL_COURSES && lpnm->code == NM_CLICK) {
            LVHITTESTINFO ht = { 0 };
            GetCursorPos(&ht.pt);
            ScreenToClient(hList, &ht.pt);
            ListView_SubItemHitTest(hList, &ht);
            if (editItem != -1 && editItem != ht.iItem) {
                SaveAndDestroyCombo();
            }
            if (ht.iSubItem == 2 && ht.iItem != -1 && (*slots)[ht.iItem].classNumber > 0) {
                if (hEditCombo && editItem == ht.iItem) {
                    return (INT_PTR)TRUE;
                }
                editItem = ht.iItem;
                RECT rc;
                ListView_GetSubItemRect(hList, editItem, 2, LVIR_BOUNDS, &rc);
                hEditCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | WS_VSCROLL,
                    rc.left, rc.top, rc.right - rc.left, 150,
                    hList, (HMENU)IDC_COMBO_FILL_COURSE_SELECT, g_hInstance, NULL);
                SendMessage(hEditCombo, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                for (const auto& course : g_availableCourses) {
                    SendMessageW(hEditCombo, CB_ADDSTRING, 0, (LPARAM)course.c_str());
                }
                int sel = SendMessage(hEditCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)(*slots)[editItem].subject.c_str());
                SendMessage(hEditCombo, CB_SETCURSEL, sel, 0);
                SetFocus(hEditCombo);
                SendMessage(hEditCombo, CB_SHOWDROPDOWN, TRUE, 0);
            }
            else {
                SaveAndDestroyCombo();
            }
        }
        else if (lpnm->code == LVN_ITEMCHANGED) {
            LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
            if (!(pnmv->uChanged & LVIF_STATE)) break;
            if (!(pnmv->uNewState & LVIS_SELECTED)) {
                if (pnmv->iItem == editItem) SaveAndDestroyCombo();
            }
        }
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND: {
        WORD wmId = LOWORD(wParam);
        WORD wmEvent = HIWORD(wParam);
        if (wmId == IDC_COMBO_FILL_COURSE_SELECT) {
            if (wmEvent == CBN_KILLFOCUS) SaveAndDestroyCombo();
        }
        else if (wmId == IDOK) {
            SaveAndDestroyCombo();
            bool allSelected = true;
            for (const auto& slot : *slots) {
                if (slot.classNumber > 0 && (slot.subject == L"待定课程" || slot.subject == L"请选择...")) {
                    allSelected = false;
                    break;
                }
            }
            if (!allSelected) {
                MessageBoxW(hDlg, L"请为所有课程选择一个科目。\n(单击“选择课程”单元格进行编辑)", L"提示", MB_OK | MB_ICONWARNING);
                return (INT_PTR)TRUE;
            }
            EndDialog(hDlg, IDOK);
        }
        else if (wmId == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
        }
        return (INT_PTR)TRUE;
    }
    case WM_DESTROY: {
        SaveAndDestroyCombo();
        break;
    }
    }
    return (INT_PTR)FALSE;
}
