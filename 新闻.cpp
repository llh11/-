// =================================================================================
//  新闻调度器 v8.3 (日志上传修复版)
//  作者: Gemini
//  更新日期: 2024-07-29
//
//  功能更新:
//  - [修复] 实现了日志内容的JSON转义，确保包含特殊字符（如引号、路径斜杠）的日志能够被服务器正确解析和记录，解决了本地与服务器日志不一致的问题。
//  - [修复] 添加了 Log 函数的前向声明，解决了 C3861 "找不到标识符" 编译错误。
//  - [修复] 修正了与服务器的HTTP通信逻辑，解决了心跳和日志发送失败的问题。
//  - [修复] 修正了程序更新的下载URL，使其指向正确的 `download_handler.php` 脚本。
// =================================================================================

// --- 核心包含文件 ---
#include <Windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <memory>
#include <functional>
#include <fstream>
#include <iomanip> // [NEW] Added for JSON escaping
#include <rpcdce.h>
#include <urlmon.h>
#include <mmsystem.h>
#include "resource1.h"

// --- 链接器依赖 ---
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "version.lib") // Needed for GetFileVersionInfo

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// --- 命名空间 ---
using namespace std;
using namespace Gdiplus;
using namespace std::chrono_literals;

// --- 全局常量与配置 ---
const wchar_t* const USER_AGENT = L"NewsSchedulerApp/8.3.0"; // 版本更新
const wstring TARGET_DIR = L"D:\\news\\";
const wstring CONFIG_FILE_NAME = L"config.ini";
const wstring LOG_FILE_NAME = L"scheduler.log";
const wstring CONFIG_PATH = TARGET_DIR + CONFIG_FILE_NAME;
const wstring LOG_PATH = TARGET_DIR + LOG_FILE_NAME;
const wstring DEVICE_ID_FILE = TARGET_DIR + L"device.id";
const wstring IGNORED_UPDATE_VERSION_PATH = TARGET_DIR + L"update_ignored.txt";
const wstring UPDATE_PACKAGE_NAME = L"news_update_package.exe";

// API URLs
const wstring API_BASE_URL = L"http://gemini.lcez.top/api";
const wstring HEARTBEAT_URL = API_BASE_URL + L"/heartbeat.php";
const wstring LOG_API_URL = API_BASE_URL + L"/log.php";
const wstring UPLOAD_PHP_URL = API_BASE_URL + L"/upload.php"; // For version check
const wstring DOWNLOAD_HANDLER_URL = L"http://gemini.lcez.top/management/download_handler.php";


// --- 窗口类名 ---
const wchar_t* const MAIN_WND_CLASS = L"NewsSchedulerMainWindowClass";
const wchar_t* const COUNTDOWN_WND_CLASS = L"NewsSchedulerCountdownWidgetClass";
const wchar_t* const TIMETABLE_WND_CLASS = L"NewsSchedulerTimetableWidgetClass";

// --- 托盘图标消息与菜单ID ---
#define WM_APP_TRAY_MSG (WM_APP + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW_COUNTDOWN 1002
#define ID_TRAY_SHOW_TIMETABLE 1003
#define ID_TRAY_OPEN_CONFIG 1006
#define ID_TRAY_CHECK_UPDATE 1007
#define ID_TRAY_OPEN_LOG 1008
#define ID_TRAY_VALIDATE_EVENT 1009
// 字体大小调整ID
#define ID_TRAY_CD_TITLE_FONT_INC 1010
#define ID_TRAY_CD_TITLE_FONT_DEC 1011
#define ID_TRAY_CD_TIME_FONT_INC 1012
#define ID_TRAY_CD_TIME_FONT_DEC 1013
#define ID_TRAY_TT_FONT_INC 1014
#define ID_TRAY_TT_FONT_DEC 1015
// 任务对话框ID
#define ID_TASK_UPDATE_NOW 2001
#define ID_TASK_NOT_NOW 2002
#define ID_TASK_IGNORE_VERSION 2003
#define ID_VALIDATE_EVENT_START 3000


// --- 全局状态变量 ---
atomic_bool g_bRunning(true);
mutex g_logMutex;
wstring g_deviceID;
HINSTANCE g_hInstance = nullptr;
HWND g_hMainWnd = nullptr;
HWND g_hCountdownWnd = nullptr;
HWND g_hTimetableWnd = nullptr;
atomic_bool g_bCheckUpdateNow = false;
wstring g_currentVersion; // Loaded from version resource
wstring g_appName;        // Loaded from string table

// --- GDI+ 全局变量 ---
ULONG_PTR g_gdiplusToken;

// --- 事件与课表结构体 ---
enum class EventType { Unknown, OpenURL, SystemAction, CloseBrowsers };
struct ScheduledEvent {
    wstring sectionName;
    EventType type = EventType::Unknown;
    int hour = -1, minute = -1;
    wstring parameters;
    bool triggeredToday = false;
    bool enableVolumeFade = false;
    int startVolume = 0, endVolume = 100, fadeDuration = 5;
    bool enableMouseClick = false;
    int clickX = 0, clickY = 0;
};

struct TimetableEntry {
    wstring subject;
    wstring time;
    int startHour, startMinute, endHour, endMinute;
};

struct WidgetConfig {
    POINT position = { 100, 100 };
    bool visible = true;
};

struct CountdownSettings {
    wstring title = L"距离高考还有";
    SYSTEMTIME targetTime = { 2025, 6, 0, 7, 9, 0, 0, 0 };
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

// --- 全局数据容器 ---
vector<ScheduledEvent> g_scheduledEvents;
mutex g_eventsMutex;
vector<TimetableEntry> g_timetables[7];
mutex g_timetableMutex;

// --- 小组件配置 ---
WidgetConfig g_countdownConfig;
WidgetConfig g_timetableConfig;
CountdownSettings g_countdownSettings;
TimetableSettings g_timetableSettings;

// --- 函数前向声明 ---
void Log(const string& message_utf8);

// =================================================================================
// SECTION: 实用工具函数
// =================================================================================
string wstring_to_utf8(const wstring& wstr) {
    if (wstr.empty()) return string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

wstring utf8_to_wstring(const string& str) {
    if (str.empty()) return wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// [NEW] Escapes a string for use in a JSON string value.
string EscapeJsonString(const string& input) {
    stringstream ss;
    for (char c : input) {
        switch (c) {
        case '\"': ss << "\\\""; break;
        case '\\': ss << "\\\\"; break;
        case '\b': ss << "\\b"; break;
        case '\f': ss << "\\f"; break;
        case '\n': ss << "\\n"; break;
        case '\r': ss << "\\r"; break;
        case '\t': ss << "\\t"; break;
        default:
            // Escape control characters
            if (c >= 0 && c < 32) {
                ss << "\\u" << hex << setfill('0') << setw(4) << (int)(unsigned char)c;
            }
            else {
                ss << c;
            }
            break;
        }
    }
    return ss.str();
}


class InternetHandle {
    HINTERNET h;
public:
    explicit InternetHandle(HINTERNET handle = nullptr) : h(handle) {}
    ~InternetHandle() { if (h) InternetCloseHandle(h); }
    HINTERNET get() const { return h; }
    HINTERNET release() { HINTERNET temp = h; h = nullptr; return temp; }
};

wstring GetCurrentAppVersion() {
    wchar_t filename[MAX_PATH];
    GetModuleFileNameW(NULL, filename, MAX_PATH);
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(filename, &handle);
    if (size == 0) return L"0.0.0.0";

    vector<BYTE> buffer(size);
    if (!GetFileVersionInfoW(filename, handle, size, buffer.data())) return L"0.0.0.0";

    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT fileInfoLen = 0;
    if (VerQueryValueW(buffer.data(), L"\\", (LPVOID*)&fileInfo, &fileInfoLen) && fileInfoLen > 0) {
        wchar_t versionStr[100];
        swprintf_s(versionStr, L"%d.%d.%d",
            HIWORD(fileInfo->dwProductVersionMS), LOWORD(fileInfo->dwProductVersionMS),
            HIWORD(fileInfo->dwProductVersionLS));
        return wstring(versionStr);
    }
    return L"0.0.0.0";
}


// =================================================================================
// SECTION: 日志系统 (本地与远程)
// =================================================================================
void HttpPost(const wstring& url, const string& json_body) {
    URL_COMPONENTSW urlComp = { sizeof(urlComp) };
    wchar_t hostName[256] = { 0 };
    wchar_t urlPath[2048] = { 0 };
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = ARRAYSIZE(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = ARRAYSIZE(urlPath);

    if (!InternetCrackUrlW(url.c_str(), 0, 0, &urlComp)) {
        // We can't log here if Log calls HttpPost, to avoid infinite recursion.
        // So we just return. The caller can log the error.
        return;
    }

    InternetHandle hInternet(InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0));
    if (!hInternet.get()) return;

    InternetHandle hConnect(InternetConnectW(hInternet.get(), hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1));
    if (!hConnect.get()) return;

    const wchar_t* acceptTypes[] = { L"*/*", NULL };
    DWORD requestFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;

    InternetHandle hRequest(HttpOpenRequestW(hConnect.get(), L"POST", urlPath, NULL, NULL, acceptTypes, requestFlags, 1));
    if (!hRequest.get()) return;

    wstring headers = L"Content-Type: application/json; charset=utf-8";
    HttpSendRequestA(hRequest.get(), wstring_to_utf8(headers).c_str(), (DWORD)headers.length(), (LPVOID)json_body.c_str(), (DWORD)json_body.length());
}


void SendLogToServer(const string& message_utf8) {
    if (g_deviceID.empty() || LOG_API_URL.empty()) return;

    thread([message_utf8]() {
        // [FIXED] Escape the log message to create valid JSON
        string escaped_message = EscapeJsonString(message_utf8);
        string json_body = "{\"device_id\":\"" + wstring_to_utf8(g_deviceID) + "\", \"message\":\"" + escaped_message + "\"}";
        HttpPost(LOG_API_URL, json_body);
        }).detach();
}

void Log(const string& message_utf8) {
    time_t now = time(nullptr);
    tm tm_info;
    localtime_s(&tm_info, &now);
    char timestamp_cstr[64];
    strftime(timestamp_cstr, sizeof(timestamp_cstr), "[%Y-%m-%d %H:%M:%S]", &tm_info);
    string full_message = string(timestamp_cstr) + " " + message_utf8;

    // Send to server first
    SendLogToServer(full_message);

    // Then write to local file
    {
        lock_guard<mutex> lock(g_logMutex);
        ofstream log_file(LOG_PATH, ios_base::app);
        if (log_file.is_open()) {
            log_file << full_message << endl;
        }
    }
}

// =================================================================================
// SECTION: 配置管理
// =================================================================================
void LoadConfiguration() {
    Log("重新加载配置文件...");
    wchar_t buffer[4096];

    {
        lock_guard<mutex> lock(g_eventsMutex);
        g_scheduledEvents.clear();
        wchar_t allSections[16384];
        GetPrivateProfileSectionNamesW(allSections, 16384, CONFIG_PATH.c_str());
        for (wchar_t* pSection = allSections; *pSection != L'\0'; pSection += wcslen(pSection) + 1) {
            if (wcsstr(pSection, L"Event") == pSection) {
                ScheduledEvent event;
                event.sectionName = pSection;
                GetPrivateProfileStringW(pSection, L"Type", L"", buffer, 2048, CONFIG_PATH.c_str());
                wstring typeStr = buffer;
                if (_wcsicmp(typeStr.c_str(), L"OpenURL") == 0) event.type = EventType::OpenURL;
                else if (_wcsicmp(typeStr.c_str(), L"SystemAction") == 0) event.type = EventType::SystemAction;
                else if (_wcsicmp(typeStr.c_str(), L"CloseBrowsers") == 0) event.type = EventType::CloseBrowsers;
                else continue;
                GetPrivateProfileStringW(pSection, L"Time", L"", buffer, 2048, CONFIG_PATH.c_str());
                if (swscanf_s(buffer, L"%d:%d", &event.hour, &event.minute) != 2) continue;
                GetPrivateProfileStringW(pSection, L"Parameters", L"", buffer, 2048, CONFIG_PATH.c_str());
                event.parameters = buffer;
                event.enableVolumeFade = GetPrivateProfileIntW(pSection, L"EnableVolumeFade", 0, CONFIG_PATH.c_str());
                event.startVolume = GetPrivateProfileIntW(pSection, L"StartVolume", 0, CONFIG_PATH.c_str());
                event.endVolume = GetPrivateProfileIntW(pSection, L"EndVolume", 100, CONFIG_PATH.c_str());
                event.fadeDuration = GetPrivateProfileIntW(pSection, L"FadeDuration", 5, CONFIG_PATH.c_str());
                event.enableMouseClick = GetPrivateProfileIntW(pSection, L"EnableMouseClick", 0, CONFIG_PATH.c_str());
                event.clickX = GetPrivateProfileIntW(pSection, L"ClickX", 0, CONFIG_PATH.c_str());
                event.clickY = GetPrivateProfileIntW(pSection, L"ClickY", 0, CONFIG_PATH.c_str());
                g_scheduledEvents.push_back(event);
            }
        }
        Log("加载了 " + to_string(g_scheduledEvents.size()) + " 个计划事件。");
    }

    {
        lock_guard<mutex> lock(g_timetableMutex);
        for (int i = 0; i < 7; ++i) {
            g_timetables[i].clear();
            wstring sectionName = L"Timetable_" + to_wstring(i);
            for (int j = 1; j <= 30; ++j) {
                TimetableEntry entry;
                GetPrivateProfileStringW(sectionName.c_str(), (L"Class" + to_wstring(j)).c_str(), L"", buffer, 256, CONFIG_PATH.c_str());
                entry.subject = buffer;
                GetPrivateProfileStringW(sectionName.c_str(), (L"Time" + to_wstring(j)).c_str(), L"", buffer, 256, CONFIG_PATH.c_str());
                entry.time = buffer;
                if (entry.subject.empty()) break;
                if (swscanf_s(entry.time.c_str(), L"%d:%d-%d:%d", &entry.startHour, &entry.startMinute, &entry.endHour, &entry.endMinute) == 4) {
                    g_timetables[i].push_back(entry);
                }
            }
        }
        Log("已为7天加载课表。");
    }

    g_countdownConfig.position.x = GetPrivateProfileIntW(L"CountdownWidget", L"PosX", 100, CONFIG_PATH.c_str());
    g_countdownConfig.position.y = GetPrivateProfileIntW(L"CountdownWidget", L"PosY", 100, CONFIG_PATH.c_str());
    g_countdownConfig.visible = GetPrivateProfileIntW(L"CountdownWidget", L"Visible", 1, CONFIG_PATH.c_str()) == 1;

    g_timetableConfig.position.x = GetPrivateProfileIntW(L"TimetableWidget", L"PosX", 100, CONFIG_PATH.c_str());
    g_timetableConfig.position.y = GetPrivateProfileIntW(L"TimetableWidget", L"PosY", 300, CONFIG_PATH.c_str());
    g_timetableConfig.visible = GetPrivateProfileIntW(L"TimetableWidget", L"Visible", 1, CONFIG_PATH.c_str()) == 1;

    GetPrivateProfileStringW(L"Countdown", L"Title", L"距离高考还有", buffer, 256, CONFIG_PATH.c_str());
    g_countdownSettings.title = buffer;
    g_countdownSettings.targetTime.wYear = GetPrivateProfileIntW(L"Countdown", L"Year", 2025, CONFIG_PATH.c_str());
    g_countdownSettings.targetTime.wMonth = GetPrivateProfileIntW(L"Countdown", L"Month", 6, CONFIG_PATH.c_str());
    g_countdownSettings.targetTime.wDay = GetPrivateProfileIntW(L"Countdown", L"Day", 7, CONFIG_PATH.c_str());
    g_countdownSettings.targetTime.wHour = GetPrivateProfileIntW(L"Countdown", L"Hour", 9, CONFIG_PATH.c_str());
    g_countdownSettings.targetTime.wMinute = GetPrivateProfileIntW(L"Countdown", L"Minute", 0, CONFIG_PATH.c_str());
    g_countdownSettings.targetTime.wSecond = 0;
    g_countdownSettings.titleFontSize = GetPrivateProfileIntW(L"Countdown", L"TitleFontSize", 24, CONFIG_PATH.c_str());
    g_countdownSettings.timeFontSize = GetPrivateProfileIntW(L"Countdown", L"TimeFontSize", 48, CONFIG_PATH.c_str());
    g_countdownSettings.timeFontColor = GetPrivateProfileIntW(L"Countdown", L"FontColor", RGB(255, 255, 255), CONFIG_PATH.c_str());
    g_countdownSettings.titleFontColor = GetPrivateProfileIntW(L"Countdown", L"TitleFontColor", g_countdownSettings.timeFontColor, CONFIG_PATH.c_str());

    g_timetableSettings.headerFontSize = GetPrivateProfileIntW(L"Timetable", L"HeaderFontSize", 20, CONFIG_PATH.c_str());
    g_timetableSettings.entryFontSize = GetPrivateProfileIntW(L"Timetable", L"EntryFontSize", 18, CONFIG_PATH.c_str());
    g_timetableSettings.headerFontColor = GetPrivateProfileIntW(L"Timetable", L"HeaderFontColor", RGB(255, 255, 255), CONFIG_PATH.c_str());
    g_timetableSettings.entryFontColor = GetPrivateProfileIntW(L"Timetable", L"EntryFontColor", RGB(255, 255, 255), CONFIG_PATH.c_str());
    g_timetableSettings.timeFontColor = GetPrivateProfileIntW(L"Timetable", L"TimeFontColor", RGB(200, 220, 255), CONFIG_PATH.c_str());
    g_timetableSettings.highlightColor = GetPrivateProfileIntW(L"Timetable", L"HighlightColor", RGB(255, 255, 0), CONFIG_PATH.c_str());
}

void SaveWidgetConfig(const wstring& section, const WidgetConfig& config) {
    WritePrivateProfileStringW(section.c_str(), L"PosX", to_wstring(config.position.x).c_str(), CONFIG_PATH.c_str());
    WritePrivateProfileStringW(section.c_str(), L"PosY", to_wstring(config.position.y).c_str(), CONFIG_PATH.c_str());
    WritePrivateProfileStringW(section.c_str(), L"Visible", config.visible ? L"1" : L"0", CONFIG_PATH.c_str());
}

void SaveFontConfig() {
    WritePrivateProfileStringW(L"Countdown", L"TitleFontSize", to_wstring(g_countdownSettings.titleFontSize).c_str(), CONFIG_PATH.c_str());
    WritePrivateProfileStringW(L"Countdown", L"TimeFontSize", to_wstring(g_countdownSettings.timeFontSize).c_str(), CONFIG_PATH.c_str());
    WritePrivateProfileStringW(L"Timetable", L"EntryFontSize", to_wstring(g_timetableSettings.entryFontSize).c_str(), CONFIG_PATH.c_str());
}

wstring GetDeviceID() {
    ifstream idFile(DEVICE_ID_FILE);
    if (idFile.is_open()) {
        string id_utf8;
        getline(idFile, id_utf8);
        idFile.close();
        if (!id_utf8.empty()) return utf8_to_wstring(id_utf8);
    }
    Log("未找到设备ID文件，正在生成新的ID...");
    UUID uuid;
    if (UuidCreate(&uuid) == RPC_S_OK) {
        wchar_t* str_uuid = nullptr;
        if (UuidToStringW(&uuid, (RPC_WSTR*)&str_uuid) == RPC_S_OK) {
            wstring new_id(str_uuid);
            RpcStringFreeW((RPC_WSTR*)&str_uuid);
            ofstream outFile(DEVICE_ID_FILE);
            if (outFile.is_open()) {
                outFile << wstring_to_utf8(new_id);
                outFile.close();
            }
            Log("已生成并保存新设备ID: " + wstring_to_utf8(new_id));
            return new_id;
        }
    }
    return L"unknown_device";
}

// =================================================================================
// SECTION: 桌面小组件核心逻辑
// =================================================================================
void RenderCountdown(HWND hWnd) {
    tm target_tm = {};
    target_tm.tm_year = g_countdownSettings.targetTime.wYear - 1900;
    target_tm.tm_mon = g_countdownSettings.targetTime.wMonth - 1;
    target_tm.tm_mday = g_countdownSettings.targetTime.wDay;
    target_tm.tm_hour = g_countdownSettings.targetTime.wHour;
    target_tm.tm_min = g_countdownSettings.targetTime.wMinute;
    target_tm.tm_isdst = -1;
    time_t target_t = mktime(&target_tm);
    time_t now_t = time(nullptr);
    long long total_seconds = (long long)difftime(target_t, now_t);

    wstring countdownText;
    if (total_seconds > 0) {
        long long days = total_seconds / (24 * 3600);
        total_seconds %= (24 * 3600);
        long long hours = total_seconds / 3600;
        total_seconds %= 3600;
        long long minutes = total_seconds / 60;
        long long seconds = total_seconds % 60;
        wchar_t buf[200];
        swprintf_s(buf, L"%lld天 %02lld:%02lld:%02lld", days, hours, minutes, seconds);
        countdownText = buf;
    }
    else {
        countdownText = L"时间到！";
    }

    Bitmap bmp(1, 1);
    Graphics g(&bmp);
    FontFamily fontFamily(L"Microsoft YaHei UI");
    Font titleFont(&fontFamily, (REAL)g_countdownSettings.titleFontSize, FontStyleRegular, UnitPixel);
    Font timeFont(&fontFamily, (REAL)g_countdownSettings.timeFontSize, FontStyleBold, UnitPixel);
    RectF titleBox, timeBox;
    g.MeasureString(g_countdownSettings.title.c_str(), -1, &titleFont, PointF(0, 0), &titleBox);
    g.MeasureString(countdownText.c_str(), -1, &timeFont, PointF(0, 0), &timeBox);

    int width = (int)max(titleBox.Width, timeBox.Width) + 40;
    int height = (int)(titleBox.Height + timeBox.Height) + 30;

    RECT currentRect;
    GetWindowRect(hWnd, &currentRect);
    if (currentRect.right - currentRect.left != width || currentRect.bottom - currentRect.top != height) {
        SetWindowPos(hWnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    Bitmap finalBmp(width, height, PixelFormat32bppARGB);
    Graphics graphics(&finalBmp);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics.Clear(Color(0, 0, 0, 0));

    StringFormat strFormat;
    strFormat.SetAlignment(StringAlignmentCenter);
    strFormat.SetLineAlignment(StringAlignmentCenter);

    Color gdiTitleColor(255, GetRValue(g_countdownSettings.titleFontColor), GetGValue(g_countdownSettings.titleFontColor), GetBValue(g_countdownSettings.titleFontColor));
    Color gdiTimeColor(255, GetRValue(g_countdownSettings.timeFontColor), GetGValue(g_countdownSettings.timeFontColor), GetBValue(g_countdownSettings.timeFontColor));
    SolidBrush titleBrush(gdiTitleColor);
    SolidBrush timeBrush(gdiTimeColor);
    SolidBrush shadowBrush(Color(100, 0, 0, 0));

    float titleY = height / 2.0f - timeBox.Height / 2.0f;
    float timeY = height / 2.0f + titleBox.Height / 2.0f;

    graphics.DrawString(g_countdownSettings.title.c_str(), -1, &titleFont, PointF((REAL)width / 2 + 1, titleY + 1), &strFormat, &shadowBrush);
    graphics.DrawString(g_countdownSettings.title.c_str(), -1, &titleFont, PointF((REAL)width / 2, titleY), &strFormat, &titleBrush);
    graphics.DrawString(countdownText.c_str(), -1, &timeFont, PointF((REAL)width / 2 + 2, timeY + 2), &strFormat, &shadowBrush);
    graphics.DrawString(countdownText.c_str(), -1, &timeFont, PointF((REAL)width / 2, timeY), &strFormat, &timeBrush);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = NULL;
    if (finalBmp.GetHBITMAP(Color(0, 0, 0, 0), &hBitmap) == Ok && hBitmap != NULL) {
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hBitmap);
        GetWindowRect(hWnd, &currentRect);
        POINT ptDst = { currentRect.left, currentRect.top };
        SIZE sizeWnd = { width, height };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hBitmap);
    }
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void RenderTimetable(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width == 0 || height == 0) return;

    Bitmap bmp(width, height, PixelFormat32bppARGB);
    Graphics graphics(&bmp);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics.Clear(Color(0, 0, 0, 0));

    SolidBrush bgBrush(Color(150, 20, 20, 40));
    graphics.FillRectangle(&bgBrush, 0, 0, width, height);

    FontFamily fontFamily(L"微软雅黑");
    Font headerFont(&fontFamily, (REAL)g_timetableSettings.headerFontSize, FontStyleBold, UnitPixel);
    Font entryFont(&fontFamily, (REAL)g_timetableSettings.entryFontSize, FontStyleRegular, UnitPixel);
    Font entryBoldFont(&fontFamily, (REAL)g_timetableSettings.entryFontSize, FontStyleBold, UnitPixel);

    Color gdiHeaderColor(255, GetRValue(g_timetableSettings.headerFontColor), GetGValue(g_timetableSettings.headerFontColor), GetBValue(g_timetableSettings.headerFontColor));
    Color gdiEntryColor(255, GetRValue(g_timetableSettings.entryFontColor), GetGValue(g_timetableSettings.entryFontColor), GetBValue(g_timetableSettings.entryFontColor));
    Color gdiTimeColor(255, GetRValue(g_timetableSettings.timeFontColor), GetGValue(g_timetableSettings.timeFontColor), GetBValue(g_timetableSettings.timeFontColor));
    Color gdiHighlightColor(255, GetRValue(g_timetableSettings.highlightColor), GetGValue(g_timetableSettings.highlightColor), GetBValue(g_timetableSettings.highlightColor));

    SolidBrush headerBrush(gdiHeaderColor);
    SolidBrush entryBrush(gdiEntryColor);
    SolidBrush timeBrush(gdiTimeColor);
    SolidBrush highlightBrush(gdiHighlightColor);
    Pen separatorPen(Color(100, 255, 255, 255), 1.0f);

    StringFormat strFormat;
    strFormat.SetAlignment(StringAlignmentNear);
    StringFormat centerStrFormat;
    centerStrFormat.SetAlignment(StringAlignmentCenter);

    SYSTEMTIME st;
    GetLocalTime(&st);
    int nowTotalMinutes = st.wHour * 60 + st.wMinute;
    int dayIndex = (st.wDayOfWeek == 0) ? 6 : st.wDayOfWeek - 1;
    const wchar_t* weekdays[] = { L"星期一", L"星期二", L"星期三", L"星期四", L"星期五", L"星期六", L"星期日" };
    wchar_t dateStr[100];
    swprintf_s(dateStr, L"%d年%d月%d日 %s", st.wYear, st.wMonth, st.wDay, weekdays[dayIndex]);

    int yPos = 15;
    graphics.DrawString(dateStr, -1, &headerFont, PointF((REAL)width / 2, (REAL)yPos), &centerStrFormat, &headerBrush);
    yPos += g_timetableSettings.headerFontSize + 15;

    {
        lock_guard<mutex> lock(g_timetableMutex);
        const auto& todayTimetable = g_timetables[dayIndex];
        for (const auto& entry : todayTimetable) {
            int startTotalMinutes = entry.startHour * 60 + entry.startMinute;
            int endTotalMinutes = entry.endHour * 60 + entry.endMinute;
            bool isCurrent = (nowTotalMinutes >= startTotalMinutes && nowTotalMinutes < endTotalMinutes);

            if (isCurrent) {
                RectF highlightRect(5.0f, (REAL)yPos - 5, (REAL)width - 10, (REAL)g_timetableSettings.entryFontSize + 10);
                graphics.FillRectangle(&highlightBrush, highlightRect);
            }

            graphics.DrawString(entry.subject.c_str(), -1, isCurrent ? &entryBoldFont : &entryFont, PointF(10, (REAL)yPos), &strFormat, &entryBrush);
            graphics.DrawString(entry.time.c_str(), -1, isCurrent ? &entryBoldFont : &entryFont, PointF(150, (REAL)yPos), &strFormat, &timeBrush);
            yPos += g_timetableSettings.entryFontSize + 5;
            graphics.DrawLine(&separatorPen, 10, yPos, width - 10, yPos);
            yPos += 5;
        }
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = NULL;
    if (bmp.GetHBITMAP(Color(0, 0, 0, 0), &hBitmap) == Ok && hBitmap != NULL) {
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hBitmap);
        GetWindowRect(hWnd, &rc);
        POINT ptDst = { rc.left, rc.top };
        SIZE sizeWnd = { width, height };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hBitmap);
    }
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK WidgetWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, WidgetConfig& config, function<void(HWND)> renderFunc) {
    static POINT lastMousePos;
    switch (message) {
    case WM_LBUTTONDOWN:
        SetCapture(hWnd);
        GetCursorPos(&lastMousePos);
        {
            RECT rcWindow;
            GetWindowRect(hWnd, &rcWindow);
            lastMousePos.x -= rcWindow.left;
            lastMousePos.y -= rcWindow.top;
        }
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == hWnd) {
            POINT currentPos;
            GetCursorPos(&currentPos);
            config.position.x = currentPos.x - lastMousePos.x;
            config.position.y = currentPos.y - lastMousePos.y;
            SetWindowPos(hWnd, NULL, config.position.x, config.position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        if (hWnd == g_hCountdownWnd) SaveWidgetConfig(L"CountdownWidget", config);
        if (hWnd == g_hTimetableWnd) SaveWidgetConfig(L"TimetableWidget", config);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        renderFunc(hWnd);
        return 0;
    }
    case WM_TIMER:
        renderFunc(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK CountdownWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return WidgetWndProc(hWnd, message, wParam, lParam, g_countdownConfig, RenderCountdown);
}
LRESULT CALLBACK TimetableWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return WidgetWndProc(hWnd, message, wParam, lParam, g_timetableConfig, RenderTimetable);
}

// =================================================================================
// SECTION: 主窗口与托盘图标
// =================================================================================

void VolumeFade(int startVol, int endVol, int durationSec);
void ClickAt(int x, int y);
void CloseBrowserWindows();
bool SystemAction(const wstring& action);

void ShowTrayMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    HMENU hFontMenu = CreatePopupMenu();
    if (hMenu && hFontMenu) {
        InsertMenuW(hMenu, -1, MF_BYPOSITION | (g_countdownConfig.visible ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_SHOW_COUNTDOWN, L"显示倒计时");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | (g_timetableConfig.visible ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_SHOW_TIMETABLE, L"显示课表");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hFontMenu, L"调整字体大小");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_TRAY_VALIDATE_EVENT, L"验证计划事件...");
        InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_TRAY_CHECK_UPDATE, L"检查更新");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_TRAY_OPEN_CONFIG, L"打开配置...");
        InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_TRAY_OPEN_LOG, L"打开日志文件...");
        InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_TRAY_EXIT, L"退出");

        AppendMenuW(hFontMenu, MF_STRING, ID_TRAY_CD_TITLE_FONT_INC, L"增大倒计时标题字体");
        AppendMenuW(hFontMenu, MF_STRING, ID_TRAY_CD_TITLE_FONT_DEC, L"减小倒计时标题字体");
        AppendMenuW(hFontMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFontMenu, MF_STRING, ID_TRAY_CD_TIME_FONT_INC, L"增大倒计时时间字体");
        AppendMenuW(hFontMenu, MF_STRING, ID_TRAY_CD_TIME_FONT_DEC, L"减小倒计时时间字体");
        AppendMenuW(hFontMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFontMenu, MF_STRING, ID_TRAY_TT_FONT_INC, L"增大课表字体");
        AppendMenuW(hFontMenu, MF_STRING, ID_TRAY_TT_FONT_DEC, L"减小课表字体");

        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(hMenu);
    }
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_APP_TRAY_MSG:
        if (lParam == WM_RBUTTONUP) ShowTrayMenu(hWnd);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_EXIT:
            g_bRunning = false;
            DestroyWindow(g_hCountdownWnd);
            DestroyWindow(g_hTimetableWnd);
            DestroyWindow(hWnd);
            break;
        case ID_TRAY_SHOW_COUNTDOWN:
            g_countdownConfig.visible = !g_countdownConfig.visible;
            ShowWindow(g_hCountdownWnd, g_countdownConfig.visible ? SW_SHOW : SW_HIDE);
            SaveWidgetConfig(L"CountdownWidget", g_countdownConfig);
            break;
        case ID_TRAY_SHOW_TIMETABLE:
            g_timetableConfig.visible = !g_timetableConfig.visible;
            ShowWindow(g_hTimetableWnd, g_timetableConfig.visible ? SW_SHOW : SW_HIDE);
            SaveWidgetConfig(L"TimetableWidget", g_timetableConfig);
            break;
        case ID_TRAY_CD_TITLE_FONT_INC:
            g_countdownSettings.titleFontSize = min(128, g_countdownSettings.titleFontSize + 2);
            RenderCountdown(g_hCountdownWnd);
            SaveFontConfig();
            break;
        case ID_TRAY_CD_TITLE_FONT_DEC:
            g_countdownSettings.titleFontSize = max(8, g_countdownSettings.titleFontSize - 2);
            RenderCountdown(g_hCountdownWnd);
            SaveFontConfig();
            break;
        case ID_TRAY_CD_TIME_FONT_INC:
            g_countdownSettings.timeFontSize = min(128, g_countdownSettings.timeFontSize + 4);
            RenderCountdown(g_hCountdownWnd);
            SaveFontConfig();
            break;
        case ID_TRAY_CD_TIME_FONT_DEC:
            g_countdownSettings.timeFontSize = max(12, g_countdownSettings.timeFontSize - 4);
            RenderCountdown(g_hCountdownWnd);
            SaveFontConfig();
            break;
        case ID_TRAY_TT_FONT_INC:
            g_timetableSettings.headerFontSize = min(64, g_timetableSettings.headerFontSize + 2);
            g_timetableSettings.entryFontSize = min(64, g_timetableSettings.entryFontSize + 2);
            RenderTimetable(g_hTimetableWnd);
            SaveFontConfig();
            break;
        case ID_TRAY_TT_FONT_DEC:
            g_timetableSettings.headerFontSize = max(8, g_timetableSettings.headerFontSize - 2);
            g_timetableSettings.entryFontSize = max(8, g_timetableSettings.entryFontSize - 2);
            RenderTimetable(g_hTimetableWnd);
            SaveFontConfig();
            break;
        case ID_TRAY_CHECK_UPDATE:
            g_bCheckUpdateNow = true;
            MessageBoxW(hWnd, L"已在后台启动更新检查。", L"检查更新", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_TRAY_OPEN_CONFIG:
            ShellExecuteW(NULL, L"open", (TARGET_DIR + L"Configurator.exe").c_str(), NULL, TARGET_DIR.c_str(), SW_SHOWNORMAL);
            break;
        case ID_TRAY_OPEN_LOG:
            ShellExecuteW(NULL, L"open", LOG_PATH.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_VALIDATE_EVENT:
        {
            vector<TASKDIALOG_BUTTON> buttons;
            vector<ScheduledEvent> validatableEvents;
            {
                lock_guard<mutex> lock(g_eventsMutex);
                for (const auto& event : g_scheduledEvents) {
                    if (event.type == EventType::OpenURL || event.type == EventType::CloseBrowsers) {
                        validatableEvents.push_back(event);
                    }
                }
            }
            if (validatableEvents.empty()) {
                MessageBoxW(hWnd, L"没有可供验证的事件 (打开网页/关闭浏览器)。", L"验证事件", MB_OK | MB_ICONINFORMATION);
                break;
            }
            for (size_t i = 0; i < validatableEvents.size(); ++i) {
                buttons.push_back({ (int)(ID_VALIDATE_EVENT_START + i), validatableEvents[i].sectionName.c_str() });
            }

            TASKDIALOGCONFIG cfg = { sizeof(cfg) };
            cfg.hwndParent = hWnd;
            cfg.dwFlags = TDF_USE_COMMAND_LINKS;
            cfg.pszWindowTitle = L"验证计划事件";
            cfg.pszMainInstruction = L"请选择一个要立即执行的事件进行测试。";
            cfg.pButtons = buttons.data();
            cfg.cButtons = (UINT)buttons.size();
            int btnPressed = 0;
            if (SUCCEEDED(TaskDialogIndirect(&cfg, &btnPressed, NULL, NULL)) && btnPressed >= ID_VALIDATE_EVENT_START) {
                int eventIndex = btnPressed - ID_VALIDATE_EVENT_START;
                if (eventIndex < (int)validatableEvents.size()) {
                    const auto& event = validatableEvents[eventIndex];
                    Log("正在手动验证事件: [" + wstring_to_utf8(event.sectionName) + "]");
                    thread([event] {
                        if (event.type == EventType::OpenURL) {
                            if (event.enableVolumeFade) thread(VolumeFade, event.startVolume, event.endVolume, event.fadeDuration).detach();
                            ShellExecuteW(nullptr, L"open", event.parameters.c_str(), nullptr, nullptr, SW_SHOWMAXIMIZED);
                            if (event.enableMouseClick) {
                                this_thread::sleep_for(5s);
                                ClickAt(event.clickX, event.clickY);
                            }
                        }
                        else if (event.type == EventType::CloseBrowsers) {
                            CloseBrowserWindows();
                        }
                        }).detach();
                }
            }
        }
        break;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}


// =================================================================================
// SECTION: 后台工作线程 (调度, 更新, 心跳)
// =================================================================================

void SetMasterVolume(int level) {
    HMIXER hMixer = NULL;
    if (mixerOpen(&hMixer, 0, 0, 0, MIXER_OBJECTF_MIXER) != MMSYSERR_NOERROR) {
        // Log("SetMasterVolume: mixerOpen failed."); // Can't log from here
        return;
    }
    MIXERLINE ml = { 0 };
    ml.cbStruct = sizeof(MIXERLINE);
    ml.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    if (mixerGetLineInfo((HMIXEROBJ)hMixer, &ml, MIXER_GETLINEINFOF_COMPONENTTYPE) != MMSYSERR_NOERROR) {
        mixerClose(hMixer);
        return;
    }
    MIXERLINECONTROLS mlc = { 0 };
    MIXERCONTROL mc = { 0 };
    mlc.cbStruct = sizeof(MIXERLINECONTROLS);
    mlc.dwLineID = ml.dwLineID;
    mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    mlc.cControls = 1;
    mlc.cbmxctrl = sizeof(MIXERCONTROL);
    mlc.pamxctrl = &mc;
    if (mixerGetLineControls((HMIXEROBJ)hMixer, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE) != MMSYSERR_NOERROR) {
        mixerClose(hMixer);
        return;
    }
    double newVolume = mc.Bounds.dwMinimum + (mc.Bounds.dwMaximum - mc.Bounds.dwMinimum) * (level / 100.0);
    MIXERCONTROLDETAILS_UNSIGNED mxdu = { (DWORD)newVolume };
    MIXERCONTROLDETAILS mcd = { 0 };
    mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
    mcd.dwControlID = mc.dwControlID;
    mcd.cChannels = 1;
    mcd.cMultipleItems = 0;
    mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
    mcd.paDetails = &mxdu;
    mixerSetControlDetails((HMIXEROBJ)hMixer, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);
    mixerClose(hMixer);
}

void VolumeFade(int startVol, int endVol, int durationSec) {
    Log("开始音量渐变: 从 " + to_string(startVol) + " 到 " + to_string(endVol) + " 在 " + to_string(durationSec) + " 秒内");
    int steps = durationSec * 20;
    if (steps == 0) {
        SetMasterVolume(endVol);
        return;
    }
    double volStep = (double)(endVol - startVol) / steps;
    for (int i = 0; i <= steps; ++i) {
        SetMasterVolume((int)(startVol + volStep * i));
        this_thread::sleep_for(50ms);
    }
    SetMasterVolume(endVol);
    Log("音量渐变完成。");
}

void ClickAt(int x, int y) {
    Log("在坐标 (" + to_string(x) + ", " + to_string(y) + ") 执行点击");
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dx = (x * 65535) / GetSystemMetrics(SM_CXSCREEN);
    inputs[0].mi.dy = (y * 65535) / GetSystemMetrics(SM_CYSCREEN);
    inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;
    inputs[1] = inputs[0];
    inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void CloseBrowserWindows() {
    Log("正在尝试关闭浏览器窗口...");
    const vector<wstring> browserClasses = { L"Chrome_WidgetWin_1", L"MozillaWindowClass" };
    for (const auto& className : browserClasses) {
        HWND hwnd = nullptr;
        while ((hwnd = FindWindowExW(nullptr, hwnd, className.c_str(), nullptr)) != nullptr) {
            if (IsWindowVisible(hwnd) && GetParent(hwnd) == NULL) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
        }
    }
    Log("已完成向已知浏览器窗口发送 WM_CLOSE。");
}

bool SystemAction(const wstring& action) {
    Log("准备执行系统操作: " + wstring_to_utf8(action));
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp = { 0 };
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return false;
    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) { CloseHandle(hToken); return false; }
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
    CloseHandle(hToken);
    if (GetLastError() != ERROR_SUCCESS) return false;
    UINT exitFlag = 0;
    if (_wcsicmp(action.c_str(), L"Shutdown") == 0) exitFlag = EWX_SHUTDOWN | EWX_FORCEIFHUNG;
    else if (_wcsicmp(action.c_str(), L"Reboot") == 0) exitFlag = EWX_REBOOT | EWX_FORCEIFHUNG;
    else if (_wcsicmp(action.c_str(), L"Logoff") == 0) exitFlag = EWX_LOGOFF | EWX_FORCEIFHUNG;
    else { Log("未知的系统操作: " + wstring_to_utf8(action)); return false; }
    Log("正在执行 ExitWindowsEx...");
    if (!ExitWindowsEx(exitFlag, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED)) {
        Log("ExitWindowsEx 失败，错误码: " + to_string(GetLastError()));
        return false;
    }
    return true;
}

void SchedulerThread() {
    Log("调度线程已启动。");
    int last_day = -1;
    while (g_bRunning) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        if (last_day != st.wDay) {
            Log("新的一天，重置每日事件触发器。");
            last_day = st.wDay;
            LoadConfiguration();
            {
                lock_guard<mutex> lock(g_eventsMutex);
                for (auto& event : g_scheduledEvents) {
                    event.triggeredToday = false;
                }
            }
            InvalidateRect(g_hTimetableWnd, NULL, TRUE);
        }
        vector<ScheduledEvent> eventsToRun;
        {
            lock_guard<mutex> lock(g_eventsMutex);
            for (auto& event : g_scheduledEvents) {
                if (!event.triggeredToday && event.hour == st.wHour && event.minute == st.wMinute) {
                    event.triggeredToday = true;
                    eventsToRun.push_back(event);
                }
            }
        }
        for (const auto& event : eventsToRun) {
            Log("正在执行计划事件: [" + wstring_to_utf8(event.sectionName) + "]");
            thread([event] {
                if (event.type == EventType::OpenURL) {
                    if (event.enableVolumeFade) thread(VolumeFade, event.startVolume, event.endVolume, event.fadeDuration).detach();
                    ShellExecuteW(nullptr, L"open", event.parameters.c_str(), nullptr, nullptr, SW_SHOWMAXIMIZED);
                    if (event.enableMouseClick) {
                        this_thread::sleep_for(5s);
                        ClickAt(event.clickX, event.clickY);
                    }
                }
                else if (event.type == EventType::SystemAction) {
                    SystemAction(event.parameters);
                }
                else if (event.type == EventType::CloseBrowsers) {
                    CloseBrowserWindows();
                }
                }).detach();
        }
        this_thread::sleep_for(1s);
    }
    Log("调度线程已退出。");
}

void UpdateCheckThread() {
    Log("更新检查线程已启动。");
    this_thread::sleep_for(15s);
    while (g_bRunning) {
        if (g_bCheckUpdateNow.exchange(false)) {
            Log("手动触发更新检查...");
            wstring serverVersion, serverFilename;
            wstring apiUrl = UPLOAD_PHP_URL + L"?action=latest&type=newsapp";
            InternetHandle hInternet(InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0));
            if (hInternet.get()) {
                InternetHandle hConnect(InternetOpenUrlW(hInternet.get(), apiUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0));
                if (hConnect.get()) {
                    string jsonResponse; char buffer[2048]; DWORD bytesRead = 0;
                    while (InternetReadFile(hConnect.get(), buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                        jsonResponse.append(buffer, bytesRead);
                    }
                    if (!jsonResponse.empty()) {
                        try {
                            size_t verPos = jsonResponse.find("\"version\":\"");
                            if (verPos != string::npos) {
                                verPos += 11;
                                serverVersion = utf8_to_wstring(jsonResponse.substr(verPos, jsonResponse.find('"', verPos) - verPos));
                            }
                            size_t filePos = jsonResponse.find("\"filename\":\"");
                            if (filePos != string::npos) {
                                filePos += 12;
                                serverFilename = utf8_to_wstring(jsonResponse.substr(filePos, jsonResponse.find('"', filePos) - filePos));
                            }
                        }
                        catch (...) { Log("解析版本JSON时出错。"); }
                    }
                }
            }
            if (!serverVersion.empty() && serverVersion > g_currentVersion) {
                wstring ignoredVersion;
                wifstream ifs(IGNORED_UPDATE_VERSION_PATH);
                if (ifs.is_open()) getline(ifs, ignoredVersion);
                ifs.close();
                if (serverVersion != ignoredVersion) {
                    TASKDIALOGCONFIG cfg = { sizeof(cfg) };
                    cfg.hwndParent = g_hMainWnd;
                    cfg.dwFlags = TDF_USE_COMMAND_LINKS;
                    cfg.pszWindowTitle = L"发现新版本！";
                    cfg.pszMainIcon = TD_INFORMATION_ICON;
                    wstring mainInst = L"发现新版本 (" + serverVersion + L")！您当前为 " + g_currentVersion + L"。";
                    cfg.pszMainInstruction = mainInst.c_str();
                    cfg.pszContent = L"建议您更新到最新版本以获取新功能和修复。";
                    TASKDIALOG_BUTTON btns[] = {
                        {ID_TASK_UPDATE_NOW, L"立即更新"},
                        {ID_TASK_NOT_NOW, L"稍后提醒"},
                        {ID_TASK_IGNORE_VERSION, L"忽略此版本"}
                    };
                    cfg.pButtons = btns;
                    cfg.cButtons = ARRAYSIZE(btns);
                    cfg.nDefaultButton = ID_TASK_UPDATE_NOW;
                    int btnPressed = 0;
                    if (SUCCEEDED(TaskDialogIndirect(&cfg, &btnPressed, NULL, NULL))) {
                        if (btnPressed == ID_TASK_UPDATE_NOW) {
                            wstring downloadUrl = DOWNLOAD_HANDLER_URL + L"?type=program&file=" + serverFilename;
                            wstring tempFile = TARGET_DIR + UPDATE_PACKAGE_NAME;
                            Log("正在从URL下载更新: " + wstring_to_utf8(downloadUrl));
                            if (S_OK == URLDownloadToFileW(NULL, downloadUrl.c_str(), tempFile.c_str(), 0, NULL)) {
                                Log("更新包下载成功，准备执行更新...");
                                wchar_t currentExePath[MAX_PATH]; GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
                                wstring batFilePath = TARGET_DIR + L"update.bat";
                                wofstream batFile(batFilePath);
                                if (batFile.is_open()) {
                                    batFile << L"@echo off\r\n"
                                        << L"chcp 65001 > nul\r\n"
                                        << L"echo 正在等待旧程序关闭...\r\n"
                                        << L"timeout /t 3 /nobreak > nul\r\n"
                                        << L"echo 正在替换文件...\r\n"
                                        << L"del \"" << currentExePath << L"\"\r\n"
                                        << L"move /Y \"" << tempFile << L"\" \"" << currentExePath << L"\"\r\n"
                                        << L"echo 正在重启程序...\r\n"
                                        << L"start \"\" \"" << currentExePath << L"\"\r\n"
                                        << L"del \"%~f0\"\r\n";
                                    batFile.close();
                                    ShellExecuteW(NULL, L"open", batFilePath.c_str(), NULL, TARGET_DIR.c_str(), SW_HIDE);
                                    PostMessage(g_hMainWnd, WM_COMMAND, ID_TRAY_EXIT, 0);
                                }
                            }
                            else {
                                Log("下载更新失败。");
                                MessageBoxW(g_hMainWnd, L"下载更新失败。", L"错误", MB_OK | MB_ICONERROR);
                            }
                        }
                        else if (btnPressed == ID_TASK_IGNORE_VERSION) {
                            wofstream ofs(IGNORED_UPDATE_VERSION_PATH);
                            if (ofs.is_open()) ofs << serverVersion;
                        }
                    }
                }
            }
            else {
                Log("当前已是最新版本。");
            }
        }
        this_thread::sleep_for(chrono::hours(1));
    }
    Log("更新检查线程已退出。");
}

void HeartbeatThread() {
    Log("心跳线程已启动。");
    while (g_bRunning) {
        if (!g_deviceID.empty()) {
            string post_data = "{\"device_id\":\"" + wstring_to_utf8(g_deviceID) + "\", \"version\":\"" + wstring_to_utf8(g_currentVersion) + "\"}";
            HttpPost(HEARTBEAT_URL, post_data);
            // Don't log every heartbeat to avoid spamming the log
        }
        this_thread::sleep_for(chrono::minutes(5));
    }
    Log("心跳线程已退出。");
}

// =================================================================================
// SECTION: 主程序入口 (WinMain) & 桌面小组件初始化
// =================================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInstance = hInstance;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    wchar_t mutexName[256];
    LoadStringW(hInstance, IDS_MUTEX_NAME, mutexName, 256);
    HANDLE hMutex = CreateMutexW(NULL, TRUE, mutexName);

    wchar_t appNameBuffer[256];
    LoadStringW(hInstance, IDS_APP_NAME, appNameBuffer, 256);
    g_appName = appNameBuffer;

    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"程序已在运行。", g_appName.c_str(), MB_OK | MB_ICONWARNING);
        if (hMutex) CloseHandle(hMutex);
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_DATE_CLASSES };
    InitCommonControlsEx(&icex);

    g_currentVersion = GetCurrentAppVersion();
    g_deviceID = GetDeviceID();
    Log("=================================================");
    Log("程序启动 - 版本 " + wstring_to_utf8(g_currentVersion));
    Log("设备 ID: " + wstring_to_utf8(g_deviceID));
    Log("=================================================");

    LoadConfiguration();

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = MAIN_WND_CLASS;
    wcex.lpfnWndProc = MainWndProc;
    RegisterClassExW(&wcex);

    wcex.lpszClassName = COUNTDOWN_WND_CLASS;
    wcex.lpfnWndProc = CountdownWndProc;
    RegisterClassExW(&wcex);

    wcex.lpszClassName = TIMETABLE_WND_CLASS;
    wcex.lpfnWndProc = TimetableWndProc;
    RegisterClassExW(&wcex);

    g_hMainWnd = CreateWindowW(MAIN_WND_CLASS, g_appName.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    g_hCountdownWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW, COUNTDOWN_WND_CLASS, L"倒计时", WS_POPUP,
        g_countdownConfig.position.x, g_countdownConfig.position.y, 300, 150,
        NULL, NULL, hInstance, NULL);

    g_hTimetableWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW, TIMETABLE_WND_CLASS, L"课表", WS_POPUP,
        g_timetableConfig.position.x, g_timetableConfig.position.y, 280, 450,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd || !g_hCountdownWnd || !g_hTimetableWnd) {
        Log("窗口创建失败!");
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(hMutex);
        return 1;
    }

    if (g_countdownConfig.visible) {
        ShowWindow(g_hCountdownWnd, SW_SHOWNA);
        RenderCountdown(g_hCountdownWnd);
    }
    if (g_timetableConfig.visible) {
        ShowWindow(g_hTimetableWnd, SW_SHOWNA);
        RenderTimetable(g_hTimetableWnd);
    }

    SetTimer(g_hCountdownWnd, 1, 1000, NULL);
    SetTimer(g_hTimetableWnd, 2, 30000, NULL);

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hMainWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY_MSG;
    nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wcscpy_s(nid.szTip, g_appName.c_str());
    Shell_NotifyIconW(NIM_ADD, &nid);

    thread(SchedulerThread).detach();
    thread(UpdateCheckThread).detach();
    thread(HeartbeatThread).detach();

    MSG msg;
    while (g_bRunning && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIconW(NIM_DELETE, &nid);
    GdiplusShutdown(g_gdiplusToken);
    CloseHandle(hMutex);
    Log("程序正常退出。");
    return (int)msg.wParam;
}
