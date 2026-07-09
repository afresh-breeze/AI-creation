/*
  game.h - 华容道游戏逻辑
 */
#ifndef GAME_H
#define GAME_H

#include "level.h"
#include "config.h"

/* 前向声明：完整定义在 ui.h */
typedef struct GameContext GameContext;

/* 悔棋快照：走子前的棋盘布局与当时步数 */
typedef struct {
    Block blocks[BLOCK_COUNT];
    int steps;
} GameSnapshot;

/* 悔棋栈：最多 UNDO_MAX 个快照，count 为当前栈深 */
typedef struct {
    GameSnapshot items[UNDO_MAX];
    int count;
} UndoStack;

/* 方块移动/滑动方向 */
typedef enum {
    DIR_UP,     // 向上
    DIR_DOWN,   // 向下
    DIR_LEFT,   // 向左
    DIR_RIGHT   // 向右
} Direction;

/***********************************************************
		bool canMove(const Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
		参数：blocks 棋盘；blockIndex 方块索引；dir 移动方向
		功能：判断方块能否向指定方向移动一格
		返回：可移动返回 true，否则 false
***********************************************************/
bool canMove(const Block blocks[BLOCK_COUNT], int blockIndex, Direction dir);

/***********************************************************
		bool moveBlock(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
		参数：blocks 棋盘；blockIndex 方块索引；dir 移动方向
		功能：合法则移动方块一格
		返回：成功返回 true，否则 false
***********************************************************/
bool moveBlock(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir);

/***********************************************************
		bool slideBlockToStop(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir)
		参数：blocks 棋盘；blockIndex 方块索引；dir 滑动方向
		功能：沿方向滑到撞停，至少移动一格才视为有效
		返回：发生移动返回 true，否则 false
***********************************************************/
bool slideBlockToStop(Block blocks[BLOCK_COUNT], int blockIndex, Direction dir);

/***********************************************************
		bool checkWin(const Block blocks[BLOCK_COUNT])
		参数：blocks 当前棋盘
		功能：判断曹操是否到达出口 (x=1,y=3)
		返回：胜利返回 true，否则 false
***********************************************************/
bool checkWin(const Block blocks[BLOCK_COUNT]);

/***********************************************************
		int hitTestBlock(const Block blocks[BLOCK_COUNT], int mx, int my,
		                 int originX, int originY)
		参数：blocks 棋盘；mx/my 鼠标坐标；originX/Y 棋盘原点
		功能：将鼠标坐标映射到方块 id
		返回：命中的方块 id；未命中返回 -1
***********************************************************/
int hitTestBlock(const Block blocks[BLOCK_COUNT], int mx, int my, int originX, int originY);

/***********************************************************
		void undoStackInit(UndoStack* stack)
		参数：stack 悔棋栈
		功能：初始化悔棋栈为空
		返回：无
***********************************************************/
void undoStackInit(UndoStack* stack);

/***********************************************************
		void undoStackPush(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：在走子前压入当前棋盘快照
		返回：无
***********************************************************/
void undoStackPush(GameContext* ctx);

/***********************************************************
		bool undoStackPop(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：弹出并恢复上一快照
		返回：成功返回 true，栈空返回 false
***********************************************************/
bool undoStackPop(GameContext* ctx);

/***********************************************************
		bool canUndo(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：判断是否可悔棋
		返回：可悔棋返回 true，否则 false
***********************************************************/
bool canUndo(const GameContext* ctx);

/***********************************************************
		void resetPlayingState(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：清除选中、拖拽、会话等输入状态
		返回：无
***********************************************************/
void resetPlayingState(GameContext* ctx);

/***********************************************************
		void beginMoveSession(GameContext* ctx, int blockId)
		参数：ctx 游戏上下文；blockId 移动的方块
		功能：开始走子会话并计步
		返回：无
***********************************************************/
void beginMoveSession(GameContext* ctx, int blockId);

/***********************************************************
		void commitMoveSession(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：结束走子会话，清除会话标记
		返回：无
***********************************************************/
void commitMoveSession(GameContext* ctx);

/***********************************************************
		bool nudgeBlock(GameContext* ctx, int blockId, Direction dir)
		参数：ctx 游戏上下文；blockId 方块；dir 推动方向
		功能：推动方块一格，管理会话、悔棋与提示联动
		返回：实际移动返回 true，否则 false
***********************************************************/
bool nudgeBlock(GameContext* ctx, int blockId, Direction dir);

#endif
