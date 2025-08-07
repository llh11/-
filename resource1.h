#pragma once

// --- 资源 ID 定义 ---
#define IDS_APP_NAME                102
#define IDS_MUTEX_NAME              103
// 主图标
#define IDI_APPICON                 101

// 主窗口控件
#define IDC_TAB_CONTROL             1000
#define IDC_BTN_SAVE                1001

// Tab 1: 事件
#define IDC_LIST_EVENTS             1101
#define IDC_BTN_ADD_EVENT           1102
#define IDC_BTN_EDIT_EVENT          1103
#define IDC_BTN_DEL_EVENT           1104

// Tab 2: 课表预览
#define IDC_LIST_TIMETABLE          1201
#define IDC_BTN_EDIT_CLASS          1202
#define IDC_BTN_DEL_CLASS           1203
#define IDC_BTN_CLASS_UP            1204
#define IDC_BTN_CLASS_DOWN          1205
// #define IDC_DAY_TAB_PREVIEW         1206 // 已移除
#define IDC_BTN_DAY_PREVIEW_START   1210 // 新增：预览选项卡的星期按钮起始ID

// Tab 3: 课表模板
#define IDC_LIST_TEMPLATE           1301
#define IDC_BTN_ADD_COURSE_BLOCK    1302
#define IDC_BTN_ADD_BREAK_BLOCK     1303
#define IDC_BTN_EDIT_BLOCK          1304
#define IDC_BTN_DEL_BLOCK           1305
#define IDC_BTN_BLOCK_UP            1306
#define IDC_BTN_BLOCK_DOWN          1307
#define IDC_BTN_GENERATE_TIMETABLE  1308
// #define IDC_DAY_TAB_TEMPLATE        1309 // 已移除
#define IDC_EDIT_TEMPLATE_START_H   1310
#define IDC_EDIT_TEMPLATE_START_M   1311
#define IDC_BTN_DAY_TEMPLATE_START  1320 // 新增：模板选项卡的星期按钮起始ID


// Tab 4: 程序设置
#define IDC_LIST_AVAILABLE_COURSES  1401
#define IDC_EDIT_NEW_COURSE_NAME    1402
#define IDC_BTN_ADD_COURSE_TO_LIB   1403
#define IDC_BTN_DEL_COURSE_FROM_LIB 1404
#define IDC_CHK_AUTORUN             1405

// Tab 5: 小组件设置
#define IDC_EDIT_CD_TITLE           1501
#define IDC_DTP_CD_DATE             1502
#define IDC_DTP_CD_TIME             1503
#define IDC_EDIT_CD_FONTSIZE        1504
#define IDC_STATIC_CD_COLOR         1505
#define IDC_STATIC_TT_HIGHLIGHT_COLOR 1506

// 对话框及内部控件
#define IDD_EVENT_DIALOG            2000
#define IDD_EDIT_EVENT_TYPE         2001
#define IDD_EDIT_EVENT_HOUR         2002
#define IDD_EDIT_EVENT_MINUTE       2003
#define IDD_EDIT_EVENT_PARAMS       2004
#define IDD_COMBO_EVENT_SYS_PARAMS  2005
#define IDD_CHK_VOLUME_FADE         2006
#define IDD_EDIT_VOL_START          2007
#define IDD_EDIT_VOL_END            2008
#define IDD_EDIT_FADE_DUR           2009
#define IDD_CHK_MOUSE_CLICK         2010
#define IDD_EDIT_CLICK_X            2011
#define IDD_EDIT_CLICK_Y            2012
#define IDC_STATIC_PARAM_LABEL      2013
#define IDC_GROUP_VOLUME            2014
#define IDC_STATIC_VOL_LABEL_1      2015
#define IDC_STATIC_VOL_ARROW        2016
#define IDC_STATIC_VOL_LABEL_2      2017
#define IDC_GROUP_CLICK             2018
#define IDC_STATIC_CLICK_LABEL      2019
#define IDD_TIMETABLE_DIALOG        3000
#define IDD_COMBO_CLASS_SUBJECT     3001
#define IDD_EDIT_CLASS_TIME         3002
#define IDD_COURSE_BLOCK_DIALOG     4000
#define IDD_EDIT_CONSECUTIVE_COUNT  4001
#define IDD_EDIT_CLASS_DURATION     4002
#define IDD_EDIT_BREAK_DURATION     4003
#define IDD_BREAK_BLOCK_DIALOG      5000
#define IDD_COMBO_BREAK_TYPE        5001
#define IDD_EDIT_CUSTOM_BREAK_NAME  5002
#define IDD_EDIT_BREAK_START_H      5003
#define IDD_EDIT_BREAK_START_M      5004
#define IDD_EDIT_BREAK_END_H        5005
#define IDD_EDIT_BREAK_END_M        5006
#define IDD_FILL_COURSES_DIALOG     6000
#define IDC_LIST_FILL_COURSES       6001
#define IDC_COMBO_FILL_COURSE_SELECT 6100
