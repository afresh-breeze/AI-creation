/*
  config.h - 华容道游戏全局常量

  职责：集中定义窗口尺寸、游戏参数、UI 布局、字体颜色、按钮 ID。
  被 ui / game / file_io / main 等模块引用，不含可执行逻辑。
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <tchar.h>
#include <graphics.h>

// ---------- 窗口尺寸 ----------
#define WIN_WIDTH   800
#define WIN_HEIGHT  600

// ---------- 游戏参数 ----------
#define LEVEL_COUNT 3           // 关卡总数（横刀立马 / 齐头并进 / 兵分三路）
#define LEADERBOARD_MAX 10      // 每关排行榜最多保留条数
#define PLAYER_NAME_MAX 32      // 排行榜玩家姓名最大字符数（含结尾 \0）
#define WIN_DELAY_MS  1000      // 通关后延迟跳转胜利页（毫秒）
#define UNDO_MAX      32        // 悔棋栈最大深度

// ---------- 按钮尺寸与间距 ----------
#define BTN_WIDTH       200
#define BTN_WIDTH_WIN   160     // 通关页横向按钮略窄
#define BTN_HEIGHT      44
#define BTN_X           300     // 菜单/选关等纵向按钮列的水平中心
#define BTN_GAP         24      // 按钮之间的垂直或水平间距

// ---------- 音效开关按钮（右上角 2×2） ----------
#define SFX_TOGGLE_W        90
#define SFX_TOGGLE_H        28
#define SFX_TOGGLE_GAP      6
#define SFX_TOGGLE_MARGIN   10

// ---------- 字体 ----------
#define FONT_NAME         _T("微软雅黑")
#define FONT_BTN_SIZE     20
#define FONT_TITLE_SIZE   40
#define FONT_BODY_SIZE    18

// ---------- 颜色（RGB） ----------
#define COLOR_BG            RGB(240, 220, 180)  // 页面背景
#define COLOR_BTN           RGB(210, 180, 140)  // 按钮默认
#define COLOR_BTN_HOVER     RGB(190, 160, 120)  // 按钮悬停
#define COLOR_BTN_TEXT      RGB(60, 40, 20)
#define COLOR_TITLE         RGB(80, 50, 20)
#define COLOR_PLACEHOLDER   RGB(200, 190, 170)

/*
  ButtonId - 全游戏可点击控件的统一标识
  handle*Click 返回此枚举，由 main.cpp 根据当前页面分发逻辑
 */
typedef enum {
    BTN_NONE = 0,
    // 主菜单
    BTN_START,              // 开始游戏 -> 选关页
    BTN_RULES,              // 游戏规则
    BTN_LEADERBOARD,        // 排行榜
    BTN_EXIT,               // 退出游戏
    // 选关 / 规则
    BTN_BACK,               // 返回主菜单
    BTN_LEVEL_0,            // 选择关卡 1
    BTN_LEVEL_1,            // 选择关卡 2
    BTN_LEVEL_2,            // 选择关卡 3
    BTN_BACK_SELECT,        // 游戏中返回选关
    BTN_DEBUG_WIN,          // 调试用（预留）
    // 游戏页工具栏
    BTN_UNDO,               // 悔棋
    BTN_REPLAY_LEVEL,       // 重玩本关
    BTN_HINT_ONE,           // 一步提示
    BTN_HINT_FULL,          // 完整提示
    // 排行榜页
    BTN_LB_PREV,            // 上一关
    BTN_LB_NEXT,            // 下一关
    BTN_LB_BACK,            // 返回主菜单
    // 通关页
    BTN_NEXT,               // 下一关
    BTN_REPLAY,             // 重玩
    BTN_BACK_SELECT_WIN,    // 返回选关
    // 音效开关（右上角）
    BTN_SFX_BGM,            // 背景音乐开/关
    BTN_SFX_MOVE,           // 移动音效开/关
    BTN_SFX_BUTTON,         // 按键音效开/关
    BTN_SFX_WIN             // 通关音效开/关
} ButtonId;

#endif
