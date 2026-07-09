/*
  game.cpp - 华容道游戏逻辑实现

  负责：移动合法性、碰撞、胜负、走子会话计步、悔棋栈。
  与 main 协作：拖拽/键盘多次调用 nudgeBlock 逐格移动；
  与 hint 协作：移动后 hintOnBoardChanged / hintAfterSessionCommit。
 */
#include "game.h"
#include "ui.h"
#include "hint.h"
#include "audio.h"
#include <Windows.h>
#include <string.h>

#define CAOCAO_ID 0   // 曹操方块 id，胜利判定用
#define WIN_X     1   // 出口左上角列（曹操 2x2 左上角需到此）
#define WIN_Y     3   // 出口行（棋盘第 4 行，0-based）

/***********************************************************
		static bool isCellOccupied(const Block blocks[BLOCK_COUNT],
		                           int cx, int cy, int excludeIndex)
		参数：blocks 棋盘；cx/cy 格子坐标；excludeIndex 排除的方块索引
		功能：判断格子是否被除 excludeIndex 外的方块占据
		返回：被占据返回 true，否则 false
***********************************************************/
static bool isCellOccupied(const Block blocks[BLOCK_COUNT], int cx, int cy, int excludeIndex)
{
    for (int i = 0; i < BLOCK_COUNT; i++) {
        if (i == excludeIndex) {
            continue;
        }
        const Block* b = &blocks[i];
        if (cx >= b->x && cx < b->x + b->w &&
            cy >= b->y && cy < b->y + b->h) {
            return true;
        }
    }
    return false;
}

/***********************************************************
		bool canMove(const Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
		参数：blocks 棋盘；blockIndex 方块索引；dir 移动方向
		功能：判断方块能否向指定方向移动一格
		返回：可移动返回 true，否则 false
***********************************************************/
bool canMove(const Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
{
    if (blockIndex < 0 || blockIndex >= BLOCK_COUNT) {
        return false;
    }

    const Block* b = &blocks[blockIndex];
    int nx = b->x;
    int ny = b->y;

    switch (dir) {
    case DIR_UP:    ny--; break;
    case DIR_DOWN:  ny++; break;
    case DIR_LEFT:  nx--; break;
    case DIR_RIGHT: nx++; break;
    }

    if (nx < 0 || ny < 0 || nx + b->w > BOARD_COLS || ny + b->h > BOARD_ROWS) {
        return false;
    }

    // 逐格检测移动后矩形是否与邻块重叠
    for (int cy = ny; cy < ny + b->h; cy++) {
        for (int cx = nx; cx < nx + b->w; cx++) {
            if (isCellOccupied(blocks, cx, cy, blockIndex)) {
                return false;
            }
        }
    }
    return true;
}

/***********************************************************
		bool moveBlock(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
		参数：blocks 棋盘；blockIndex 方块索引；dir 移动方向
		功能：合法则移动方块一格
		返回：成功返回 true，否则 false
***********************************************************/
bool moveBlock(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
{
    if (!canMove(blocks, blockIndex, dir)) {
        return false;
    }

    Block* b = &blocks[blockIndex];
    switch (dir) {
    case DIR_UP:    b->y--; break;
    case DIR_DOWN:  b->y++; break;
    case DIR_LEFT:  b->x--; break;
    case DIR_RIGHT: b->x++; break;
    }
    return true;
}

/***********************************************************
		bool slideBlockToStop(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
		参数：blocks 棋盘；blockIndex 方块索引；dir 滑动方向
		功能：沿方向滑到撞停，至少移动一格才视为有效
		返回：发生移动返回 true，否则 false
***********************************************************/
bool slideBlockToStop(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
{
    bool moved = false;
    while (canMove(blocks, blockIndex, dir)) {
        moveBlock(blocks, blockIndex, dir);
        moved = true;
    }
    return moved;
}

/***********************************************************
		bool checkWin(const Block blocks[BLOCK_COUNT])
		参数：blocks 当前棋盘
		功能：判断曹操是否到达出口 (x=1,y=3)
		返回：胜利返回 true，否则 false
***********************************************************/
bool checkWin(const Block blocks[BLOCK_COUNT])
{
    const Block* caocao = &blocks[CAOCAO_ID];
    return caocao->x == WIN_X && caocao->y == WIN_Y;
}

/***********************************************************
		int hitTestBlock(const Block blocks[BLOCK_COUNT], int mx, int my,
		                 int originX, int originY)
		参数：blocks 棋盘；mx/my 鼠标坐标；originX/Y 棋盘原点
		功能：将鼠标坐标映射到方块 id
		返回：命中的方块 id；未命中返回 -1
***********************************************************/
int hitTestBlock(const Block blocks[BLOCK_COUNT], int mx, int my, int originX, int originY)
{
    int boardW = BOARD_COLS * CELL_SIZE;
    int boardH = BOARD_ROWS * CELL_SIZE;

    if (mx < originX || my < originY ||
        mx >= originX + boardW || my >= originY + boardH) {
        return -1;
    }

    int gx = (mx - originX) / CELL_SIZE;
    int gy = (my - originY) / CELL_SIZE;

    // 从后往前也可；此处顺序遍历，后绘制的方块在上层，命中逻辑与绘制顺序无关
    for (int i = 0; i < BLOCK_COUNT; i++) {
        const Block* b = &blocks[i];
        if (gx >= b->x && gx < b->x + b->w &&
            gy >= b->y && gy < b->y + b->h) {
            return i;
        }
    }
    return -1;
}

/***********************************************************
		void undoStackInit(UndoStack* stack)
		参数：stack 悔棋栈
		功能：初始化悔棋栈为空
		返回：无
***********************************************************/
void undoStackInit(UndoStack* stack)
{
    stack->count = 0;
}

/***********************************************************
		void undoStackPush(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：在走子前压入当前棋盘快照
		返回：无
***********************************************************/
void undoStackPush(GameContext* ctx)
{
    UndoStack* stack = &ctx->undo;
    // 栈满时丢弃最旧快照，腾出位置（FIFO 式滑动窗口）
    if (stack->count >= UNDO_MAX) {
        for (int i = 0; i < UNDO_MAX - 1; i++) {
            stack->items[i] = stack->items[i + 1];
        }
        stack->count = UNDO_MAX - 1;
    }

    GameSnapshot* snap = &stack->items[stack->count];
    memcpy(snap->blocks, ctx->blocks, sizeof(Block) * BLOCK_COUNT);
    snap->steps = ctx->steps;
    stack->count++;
}

/***********************************************************
		bool canUndo(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：判断是否可悔棋
		返回：可悔棋返回 true，否则 false
***********************************************************/
bool canUndo(const GameContext* ctx)
{
    return ctx->undo.count > 0;
}

/***********************************************************
		bool undoStackPop(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：弹出并恢复上一快照
		返回：成功返回 true，栈空返回 false
***********************************************************/
bool undoStackPop(GameContext* ctx)
{
    UndoStack* stack = &ctx->undo;
    if (stack->count <= 0) {
        return false;
    }

    stack->count--;
    const GameSnapshot* snap = &stack->items[stack->count];
    memcpy(ctx->blocks, snap->blocks, sizeof(Block) * BLOCK_COUNT);
    ctx->steps = snap->steps;

    commitMoveSession(ctx);
    ctx->selectedBlockId = -1;
    ctx->isDragging = 0;
    ctx->dragBlockId = -1;
    ctx->dragLastMx = 0;
    ctx->dragLastMy = 0;
    clearHint(ctx);  // 局面已变，清除提示避免与悔棋后状态不一致
    return true;
}

/***********************************************************
		void resetPlayingState(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：清除选中、拖拽、会话等输入状态
		返回：无
***********************************************************/
void resetPlayingState(GameContext* ctx)
{
    ctx->selectedBlockId = -1;
    ctx->sessionActive = 0;
    ctx->sessionBlockId = -1;
    ctx->sessionStartX = 0;
    ctx->sessionStartY = 0;
    ctx->isDragging = 0;
    ctx->dragBlockId = -1;
    ctx->dragLastMx = 0;
    ctx->dragLastMy = 0;
    ctx->pendingWin = 0;
    ctx->winShowUntil = 0;
}

/***********************************************************
		void beginMoveSession(GameContext* ctx, int blockId)
		参数：ctx 游戏上下文；blockId 移动的方块
		功能：开始走子会话并计步
		返回：无
***********************************************************/
void beginMoveSession(GameContext* ctx, int blockId)
{
    ctx->sessionActive = 1;
    ctx->sessionBlockId = blockId;
    ctx->sessionStartX = ctx->blocks[blockId].x;
    ctx->sessionStartY = ctx->blocks[blockId].y;
    ctx->steps++;
}

/***********************************************************
		void commitMoveSession(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：结束走子会话，清除会话标记
		返回：无
***********************************************************/
void commitMoveSession(GameContext* ctx)
{
    if (!ctx->sessionActive) {
        return;
    }

    ctx->sessionActive = 0;
    ctx->sessionBlockId = -1;
}

/***********************************************************
		bool nudgeBlock(GameContext* ctx, int blockId, Direction dir)
		参数：ctx 游戏上下文；blockId 方块；dir 推动方向
		功能：推动方块一格，管理会话、悔棋与提示联动
		返回：实际移动返回 true，否则 false
***********************************************************/
bool nudgeBlock(GameContext* ctx, int blockId, Direction dir)
{
    if (blockId < 0 || blockId >= BLOCK_COUNT) {
        return false;
    }

    if (ctx->sessionActive && ctx->sessionBlockId != blockId) {
        // 切换移动方块时先结束上一会话并校验提示，保证一步计步边界正确
        commitMoveSession(ctx);
        hintAfterSessionCommit(ctx);
    }

    if (!canMove(ctx->blocks, blockId, dir)) {
        return false;
    }

    if (!ctx->sessionActive) {
        // 本会话首次有效移动前压栈；同一会话连续 nudge 只计一步
        undoStackPush(ctx);
        beginMoveSession(ctx, blockId);
    }

    moveBlock(ctx->blocks, blockId, dir);

    if (ctx->sfxMoveOn) {
        audioPlayMove();
    }

    // 移动了非提示要求的方块则取消提示，否则由 hint 模块校验进度
    if (ctx->hintMode == HINT_ONE_STEP &&
        blockId != ctx->hintPath[0].blockId) {
        clearHint(ctx);
    } else if (ctx->hintMode == HINT_FULL &&
               blockId != ctx->hintPath[ctx->hintPathIndex].blockId) {
        clearHint(ctx);
    } else {
        hintOnBoardChanged(ctx);
    }

    if (checkWin(ctx->blocks)) {
        commitMoveSession(ctx);
        ctx->winTimeSec = (int)((GetTickCount() - ctx->levelStartTick) / 1000);
        ctx->pendingWin = 1;
        ctx->winShowUntil = GetTickCount() + WIN_DELAY_MS;
        if (ctx->sfxWinOn) {
            audioPlayWin();
        }
    }
    return true;
}
