/*
  main.cpp - 华容道游戏入口

  程序结构：
    1. initgraph 创建 EasyX 窗口
    2. GameContext 保存当前页面、关卡、步数等状态
    3. 主循环：绘制当前页 -> 处理鼠标/键盘输入 -> 更新 page

  页面状态机：
    MENU <-> SELECT / RULES / LEADERBOARD
    SELECT -> PLAYING -> (延迟) -> WIN
 */
#include <graphics.h>
#include <conio.h>
#include <Windows.h>
#include <stdlib.h>
#include "ui.h"
#include "game.h"
#include "file_io.h"
#include "hint.h"
#include "audio.h"

/***********************************************************
		void restartLevel(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：重玩当前关：恢复布局、步数、计时与悔棋栈
		返回：无
***********************************************************/
static void restartLevel(GameContext* ctx)
{
    clearGameSave(ctx->currentLevel);
    loadLevel(ctx->currentLevel, ctx->blocks);
    ctx->steps = 0;
    ctx->pendingWin = 0;
    ctx->winShowUntil = 0;
    ctx->winTimeSec = -1;
    ctx->levelStartTick = GetTickCount();
    undoStackInit(&ctx->undo);
    clearHint(ctx);
    resetPlayingState(ctx);
    ctx->page = PAGE_PLAYING;
}

/***********************************************************
		void enterLevel(GameContext* ctx, int levelIndex)
		参数：ctx 游戏上下文；levelIndex 关卡索引
		功能：进入指定关卡：加载方块布局并重置步数与输入状态
		返回：无
***********************************************************/
static void enterLevel(GameContext* ctx, int levelIndex)
{
    ctx->currentLevel = levelIndex;
    // 有有效存档则恢复进度，否则从初始布局重开
    if (tryRestoreGameProgress(levelIndex, ctx)) {
        ctx->page = PAGE_PLAYING;
    } else {
        restartLevel(ctx);
    }
}

/***********************************************************
		void handleClick(GameContext* ctx, int mx, int my)
		参数：ctx 游戏上下文；mx/my 鼠标左键按下时的客户区坐标
		功能：根据当前页面检测按钮点击，切换 GameContext 状态
		返回：无
***********************************************************/
static void handleClick(GameContext* ctx, int mx, int my)
{
    ButtonId btn = BTN_NONE;

    switch (ctx->page) {
    case PAGE_MENU:
        btn = handleMenuClick(mx, my);
        if (btn == BTN_START) {
            ctx->page = PAGE_SELECT;
        } else if (btn == BTN_RULES) {
            ctx->page = PAGE_RULES;
        } else if (btn == BTN_LEADERBOARD) {
            ctx->leaderboardLevel = 0;  // 默认展示第一关
            ctx->page = PAGE_LEADERBOARD;
        } else if (btn == BTN_EXIT) {
            ctx->running = 0;
        }
        break;

    case PAGE_SELECT:
        btn = handleSelectClick(mx, my);
        if (btn == BTN_LEVEL_0) {
            enterLevel(ctx, 0);
        } else if (btn == BTN_LEVEL_1) {
            enterLevel(ctx, 1);
        } else if (btn == BTN_LEVEL_2) {
            enterLevel(ctx, 2);
        } else if (btn == BTN_BACK) {
            ctx->page = PAGE_MENU;
        }
        break;

    case PAGE_RULES:
        btn = handleRulesClick(mx, my);
        if (btn == BTN_BACK) {
            ctx->page = PAGE_MENU;
        }
        break;

    case PAGE_LEADERBOARD:
        btn = handleLeaderboardClick(mx, my);
        if (btn == BTN_LB_PREV) {
            // 循环切换展示的关卡索引
            ctx->leaderboardLevel =
                (ctx->leaderboardLevel + LEVEL_COUNT - 1) % LEVEL_COUNT;
        } else if (btn == BTN_LB_NEXT) {
            ctx->leaderboardLevel =
                (ctx->leaderboardLevel + 1) % LEVEL_COUNT;
        } else if (btn == BTN_LB_BACK) {
            ctx->page = PAGE_MENU;
        }
        break;

    case PAGE_WIN:
        btn = handleWinClick(ctx, mx, my);
        if (btn == BTN_NEXT) {
            enterLevel(ctx, ctx->currentLevel + 1);
        } else if (btn == BTN_REPLAY) {
            restartLevel(ctx);
        } else if (btn == BTN_BACK_SELECT_WIN) {
            ctx->page = PAGE_SELECT;
        }
        break;

    default:
        break;
    }

    if (btn != BTN_NONE && ctx->sfxButtonOn) {
        audioPlayButton();
    }
}

/***********************************************************
		void handlePlayingMouseDown(GameContext* ctx, int mx, int my)
		参数：ctx 游戏上下文；mx/my 鼠标坐标
		功能：游戏页鼠标按下：优先处理工具栏按钮，否则进入选中/拖拽
		返回：无
***********************************************************/
static void handlePlayingMouseDown(GameContext* ctx, int mx, int my)
{
    if (ctx->pendingWin) {
        return;  // 通关动画期间忽略输入
    }

    ButtonId btn = handlePlayingClick(mx, my);
    if (btn != BTN_NONE && ctx->sfxButtonOn) {
        audioPlayButton();
    }
    if (btn == BTN_HINT_ONE) {
        int result = activateHintOneStep(ctx);
        if (result == BFS_NO_SOLUTION) {
            MessageBox(GetHWnd(), _T("当前局面无法求解"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        } else if (result == BFS_LIMIT_HIT) {
            MessageBox(GetHWnd(), _T("搜索空间过大，无法生成提示"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        }
        return;
    }
    if (btn == BTN_HINT_FULL) {
        int result = activateHintFull(ctx);
        if (result == BFS_NO_SOLUTION) {
            MessageBox(GetHWnd(), _T("当前局面无法求解"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        } else if (result == BFS_LIMIT_HIT) {
            MessageBox(GetHWnd(), _T("搜索空间过大，无法生成提示"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        }
        return;
    }
    if (btn == BTN_UNDO) {
        undoStackPop(ctx);
        return;
    }
    if (btn == BTN_REPLAY_LEVEL) {
        restartLevel(ctx);
        return;
    }
    if (btn == BTN_BACK_SELECT) {
        commitMoveSession(ctx);  // 返回前结束未提交的走子会话
        int elapsedSec = (int)((GetTickCount() - ctx->levelStartTick) / 1000);
        saveGameProgress(ctx->currentLevel, ctx->steps, elapsedSec, ctx->blocks);
        clearHint(ctx);
        resetPlayingState(ctx);
        ctx->page = PAGE_SELECT;
        return;
    }
    if (btn != BTN_NONE) {
        return;
    }

    int hit = hitTestBlock(ctx->blocks, mx, my, BOARD_ORIGIN_X, BOARD_ORIGIN_Y);
    if (hit >= 0) {
        ctx->selectedBlockId = hit;
        ctx->isDragging = 1;
        ctx->dragBlockId = hit;
        ctx->dragLastMx = mx;
        ctx->dragLastMy = my;
    } else {
        ctx->selectedBlockId = -1;
        ctx->isDragging = 0;
        ctx->dragBlockId = -1;
    }
}

/***********************************************************
		void handlePlayingMouseMove(GameContext* ctx, int mx, int my)
		参数：ctx 游戏上下文；mx/my 鼠标坐标
		功能：游戏页拖拽：累计位移超过半格时逐格推动（支持折线）
		返回：无
***********************************************************/
static void handlePlayingMouseMove(GameContext* ctx, int mx, int my)
{
    if (ctx->pendingWin) {
        return;
    }
    if (!ctx->isDragging || ctx->dragBlockId < 0) {
        return;
    }

    int threshold = CELL_SIZE / 2;  // 累计半格位移触发一次 nudgeBlock
    int dx = mx - ctx->dragLastMx;
    int dy = my - ctx->dragLastMy;

    while (abs(dx) >= threshold || abs(dy) >= threshold) {
        Direction dir;
        if (abs(dx) >= abs(dy)) {
            dir = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
        } else {
            dir = (dy > 0) ? DIR_DOWN : DIR_UP;
        }

        if (!nudgeBlock(ctx, ctx->dragBlockId, dir)) {
            break;  // 撞墙则停止本帧继续滑动
        }

        ctx->dragLastMx = mx;
        ctx->dragLastMy = my;
        dx = 0;
        dy = 0;
    }
}

/***********************************************************
		void handlePlayingMouseUp(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：游戏页鼠标松开：仅结束拖拽，不结算走子会话
		返回：无
***********************************************************/
static void handlePlayingMouseUp(GameContext* ctx)
{
    ctx->isDragging = 0;
}

/***********************************************************
		void handleKeyDown(GameContext* ctx, const ExMessage* msg)
		参数：ctx 游戏上下文；msg 键盘消息
		功能：游戏页键盘：方向键与 WASD 逐格推动，过滤按键连发
		返回：无
***********************************************************/
static void handleKeyDown(GameContext* ctx, const ExMessage* msg)
{
    if (ctx->page != PAGE_PLAYING || ctx->selectedBlockId < 0 || ctx->pendingWin) {
        return;
    }
    if (msg->prevdown) {
        return;  // 过滤按键按住连发
    }

    Direction dir;
    switch (msg->vkcode) {
    case VK_UP:    dir = DIR_UP;    break;
    case VK_DOWN:  dir = DIR_DOWN;  break;
    case VK_LEFT:  dir = DIR_LEFT;  break;
    case VK_RIGHT: dir = DIR_RIGHT; break;
    case 'W': case 'w': dir = DIR_UP;    break;
    case 'S': case 's': dir = DIR_DOWN;  break;
    case 'A': case 'a': dir = DIR_LEFT;  break;
    case 'D': case 'd': dir = DIR_RIGHT; break;
    default: return;
    }

    nudgeBlock(ctx, ctx->selectedBlockId, dir);
}

/***********************************************************
		void drawCurrentPage(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：根据 ctx.page 调用对应的 draw 函数
		返回：无
***********************************************************/
static void drawCurrentPage(const GameContext* ctx)
{
    switch (ctx->page) {
    case PAGE_MENU:
        drawMenuPage();
        break;
    case PAGE_SELECT:
        drawSelectPage();
        break;
    case PAGE_RULES:
        drawRulesPage();
        break;
    case PAGE_LEADERBOARD:
        drawLeaderboardPage(ctx);
        break;
    case PAGE_PLAYING:
        drawPlayingPage(ctx);
        break;
    case PAGE_WIN:
        drawWinPage(ctx);
        break;
    }
    drawSoundToggleBar(ctx);
}

/***********************************************************
		int main()
		功能：程序入口：初始化窗口与状态，运行主循环
		返回：0 表示正常退出
***********************************************************/
int main()
{
    initgraph(WIN_WIDTH, WIN_HEIGHT);
    initUI();
    audioInit();

#ifdef _DEBUG
    // 启动时验证三关 BFS 均可解，结果输出到调试器而非弹窗
    if (hintSelfTest()) {
        OutputDebugStringA("hintSelfTest: pass\n");
    } else {
        OutputDebugStringA("hintSelfTest: fail\n");
    }
#endif

    GameContext ctx;
    ctx.page = PAGE_MENU;
    ctx.currentLevel = -1;
    ctx.steps = 0;
    ctx.running = 1;
    ctx.levelStartTick = GetTickCount();
    ctx.winTimeSec = -1;
    ctx.leaderboardLevel = 0;
    ctx.sfxBgmOn = 1;
    ctx.sfxMoveOn = 1;
    ctx.sfxButtonOn = 1;
    ctx.sfxWinOn = 1;
    resetPlayingState(&ctx);
    undoStackInit(&ctx.undo);
    clearHint(&ctx);
    loadRecords(ctx.levelRecords);
    loadLeaderboard(ctx.leaderboards);
    seedLeaderboardFromRecords(ctx.leaderboards, ctx.levelRecords);
    saveLeaderboard(ctx.leaderboards);  // 迁移后写回，保证 leaderboard.txt 存在

    audioUpdateBgm(ctx.sfxBgmOn != 0);
    BeginBatchDraw();  // 双缓冲，减少闪烁

    while (ctx.running) {
        // 主循环：绘制当前页 -> 处理输入消息 -> 通关延迟结算
        drawCurrentPage(&ctx);
        FlushBatchDraw();

        ExMessage msg;
        while (peekmessage(&msg, EX_MOUSE | EX_KEY)) {
            if (msg.message == WM_LBUTTONDOWN) {
                // 音效栏全局优先；游戏页走专用鼠标处理，其余页走按钮分发
                ButtonId sfxBtn = handleSoundToggleClick(&ctx, msg.x, msg.y);
                if (sfxBtn != BTN_NONE) {
                    toggleSoundSetting(&ctx, sfxBtn);
                    if (ctx.sfxButtonOn) {
                        audioPlayButton();
                    }
                } else if (ctx.page == PAGE_PLAYING) {
                    handlePlayingMouseDown(&ctx, msg.x, msg.y);
                } else {
                    handleClick(&ctx, msg.x, msg.y);  // 非游戏页统一走按钮分发
                }
            } else if (msg.message == WM_LBUTTONUP) {
                if (ctx.page == PAGE_PLAYING) {
                    handlePlayingMouseUp(&ctx);
                }
            } else if (msg.message == WM_MOUSEMOVE) {
                uiSetMousePos(msg.x, msg.y);
                if (ctx.page == PAGE_PLAYING) {
                    handlePlayingMouseMove(&ctx, msg.x, msg.y);
                }
            } else if (msg.message == WM_KEYDOWN) {
                handleKeyDown(&ctx, &msg);
            }
        }

        if (ctx.pendingWin && GetTickCount() >= ctx.winShowUntil) {
            // InputBox 为模态对话框，须先 EndBatchDraw 再弹窗
            EndBatchDraw();

            TCHAR playerName[PLAYER_NAME_MAX] = _T("");
            bool inputOk = InputBox(playerName, PLAYER_NAME_MAX,
                _T("恭喜通关"), _T("请输入您的姓名："), _T(""));

            BeginBatchDraw();

            const TCHAR* nameToSave = playerName;
            if (!inputOk || playerName[0] == _T('\0')) {
                nameToSave = NULL;
            }

            // 写通关记录与排行榜后进入结算页
            clearGameSave(ctx.currentLevel);
            updateRecordOnWin(ctx.currentLevel, ctx.steps, ctx.winTimeSec,
                              ctx.levelRecords);
            saveRecords(ctx.levelRecords);
            addLeaderboardEntry(ctx.currentLevel, ctx.steps, ctx.winTimeSec,
                                nameToSave, ctx.leaderboards);
            saveLeaderboard(ctx.leaderboards);
            ctx.pendingWin = 0;
            ctx.page = PAGE_WIN;
        }

        Sleep(10);  // 降低空转 CPU 占用
    }

    EndBatchDraw();
    audioShutdown();
    closegraph();
    return 0;
}
