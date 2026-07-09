/*
  ui.h - 界面模块头文件
 */
#ifndef UI_H
#define UI_H

#include <graphics.h>
#include <tchar.h>
#include <Windows.h>
#include "config.h"
#include "level.h"
#include "game.h"
#include "file_io.h"
#include "hint.h"

/* 页面状态：主菜单 / 选关 / 规则 / 排行榜 / 对局中 / 通关结算 */
typedef enum {
    PAGE_MENU,          // 主菜单
    PAGE_SELECT,        // 选关
    PAGE_RULES,         // 游戏规则
    PAGE_LEADERBOARD,   // 排行榜
    PAGE_PLAYING,       // 游戏对局
    PAGE_WIN            // 通关结算
} PageState;

/* UI 按钮：矩形区域、显示文字、对应 ButtonId */
typedef struct {
    int x, y, w, h;
    const TCHAR* label;
    ButtonId id;
} Button;

/*
  GameContext - 全局游戏状态
  页面：page / currentLevel / leaderboardLevel / running
  走子：steps / selectedBlockId / session* / isDragging / drag*
  通关：pendingWin / winShowUntil / levelStartTick / winTimeSec
  悔棋：undo；记录：levelRecords / leaderboards
  提示：hintMode / hintPath* / hintBlockId / hintDir
  棋盘：blocks；音效：sfxBgmOn / sfxMoveOn / sfxButtonOn / sfxWinOn
 */
typedef struct GameContext {
    PageState page;                 // 当前页面
    int currentLevel;
    int leaderboardLevel;
    int steps;
    int running;
    int selectedBlockId;            // 走子会话与拖拽
    int sessionActive;
    int sessionBlockId;
    int sessionStartX;
    int sessionStartY;
    int isDragging;
    int dragBlockId;
    int dragLastMx;
    int dragLastMy;
    int pendingWin;                 // 通关延迟与计时
    DWORD winShowUntil;
    DWORD levelStartTick;
    int winTimeSec;
    UndoStack undo;
    LevelRecord levelRecords[LEVEL_COUNT];
    LevelLeaderboard leaderboards[LEVEL_COUNT];
    HintMode hintMode;              // BFS 提示路径
    int hintBlockId;
    Direction hintDir;
    HintMove hintPath[HINT_PATH_MAX];
    BoardState hintPathStates[HINT_PATH_MAX + 1];
    int hintPathLen;
    int hintPathIndex;
    Block blocks[BLOCK_COUNT];
    int sfxBgmOn;                   // 音效开关
    int sfxMoveOn;
    int sfxButtonOn;
    int sfxWinOn;
} GameContext;

/***********************************************************
		void initUI(void)
		功能：初始化界面默认字体
		返回：无
***********************************************************/
void initUI(void);

/***********************************************************
		void uiSetMousePos(int mx, int my)
		参数：mx/my 鼠标客户区坐标
		功能：更新当前鼠标位置，供按钮悬停高亮判定
		返回：无
***********************************************************/
void uiSetMousePos(int mx, int my);

/***********************************************************
		void clearBackground(void)
		功能：清空窗口并填充背景色
		返回：无
***********************************************************/
void clearBackground(void);

/***********************************************************
		void setUIFont(int size, bool bold)
		参数：size 字号；bold 是否加粗
		功能：设置中文 TrueType 字体
		返回：无
***********************************************************/
void setUIFont(int size, bool bold);

/***********************************************************
		void drawMenuPage(void)
		功能：绘制主菜单页面
		返回：无
***********************************************************/
void drawMenuPage(void);

/***********************************************************
		void drawSelectPage(void)
		功能：绘制选关页面
		返回：无
***********************************************************/
void drawSelectPage(void);

/***********************************************************
		void drawRulesPage(void)
		功能：绘制游戏规则页面
		返回：无
***********************************************************/
void drawRulesPage(void);

/***********************************************************
		void drawLeaderboardPage(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：绘制排行榜页面
		返回：无
***********************************************************/
void drawLeaderboardPage(const GameContext* ctx);

/***********************************************************
		void drawPlayingPage(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：绘制游戏对局页面
		返回：无
***********************************************************/
void drawPlayingPage(const GameContext* ctx);

/***********************************************************
		void drawWinPage(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：绘制通关结算页面
		返回：无
***********************************************************/
void drawWinPage(const GameContext* ctx);

/***********************************************************
		ButtonId handleMenuClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测主菜单按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleMenuClick(int mx, int my);

/***********************************************************
		ButtonId handleSelectClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测选关页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleSelectClick(int mx, int my);

/***********************************************************
		ButtonId handleRulesClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测规则页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleRulesClick(int mx, int my);

/***********************************************************
		ButtonId handleLeaderboardClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测排行榜页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleLeaderboardClick(int mx, int my);

/***********************************************************
		ButtonId handlePlayingClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测游戏页工具栏按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handlePlayingClick(int mx, int my);

/***********************************************************
		ButtonId handleWinClick(const GameContext* ctx, int mx, int my)
		参数：ctx 游戏上下文；mx/my 鼠标坐标
		功能：检测通关页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleWinClick(const GameContext* ctx, int mx, int my);

/***********************************************************
		void drawSoundToggleBar(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：各页右上角绘制 2×2 音效开关按钮
		返回：无
***********************************************************/
void drawSoundToggleBar(const GameContext* ctx);

/***********************************************************
		ButtonId handleSoundToggleClick(const GameContext* ctx, int mx, int my)
		参数：ctx 游戏上下文；mx/my 鼠标坐标
		功能：检测右上角音效开关按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleSoundToggleClick(const GameContext* ctx, int mx, int my);

/***********************************************************
		void toggleSoundSetting(GameContext* ctx, ButtonId btn)
		参数：ctx 游戏上下文；btn 被点击的音效开关按钮
		功能：切换 BGM/移动/按键/通关开关；BGM 联动 audioUpdateBgm
		返回：无
***********************************************************/
void toggleSoundSetting(GameContext* ctx, ButtonId btn);

#endif
