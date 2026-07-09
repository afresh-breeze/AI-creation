/*
  hint.h - BFS 提示与路线引导
 */
#ifndef HINT_H
#define HINT_H

#include "level.h"
#include "game.h"

#define BFS_MAX_NODES     65536   // BFS 节点池上限
#define HINT_PATH_MAX     512     // 提示路径最大步数
#define BFS_NO_SOLUTION   (-1)    // 当前局面无解
#define BFS_LIMIT_HIT     (-2)    // 搜索空间超限

/* 棋盘局面编码：仅存各方块左上角坐标，供 BFS 去重与路径校验 */
typedef struct {
    int bx[BLOCK_COUNT];
    int by[BLOCK_COUNT];
} BoardState;

/* 提示路径中的一步：滑动某方块至撞停的方向 */
typedef struct {
    int blockId;
    Direction dir;
} HintMove;

/* 提示模式：无 / 仅高亮下一步 / 按路径逐步推进 */
typedef enum {
    HINT_NONE,
    HINT_ONE_STEP,
    HINT_FULL
} HintMode;

typedef struct GameContext GameContext;

/***********************************************************
		void encodeBoardState(const Block blocks[BLOCK_COUNT], BoardState* s)
		参数：blocks 方块数组；s 输出状态
		功能：将方块坐标编码为 BoardState
		返回：无
***********************************************************/
void encodeBoardState(const Block blocks[BLOCK_COUNT], BoardState* s);

/***********************************************************
		bool boardStateEqual(const BoardState* a, const BoardState* b)
		参数：a、b 两个棋盘状态
		功能：比较两状态是否完全相同
		返回：相同返回 true，否则 false
***********************************************************/
bool boardStateEqual(const BoardState* a, const BoardState* b);

/***********************************************************
		void applyBoardState(const BoardState* s, Block blocks[BLOCK_COUNT])
		参数：s 棋盘状态；blocks 输出方块数组
		功能：将状态中的坐标写回 blocks
		返回：无
***********************************************************/
void applyBoardState(const BoardState* s, Block blocks[BLOCK_COUNT]);

/***********************************************************
		int bfsSolve(const Block blocks[BLOCK_COUNT], HintMove path[HINT_PATH_MAX])
		参数：blocks 当前棋盘；path 输出最短路径
		功能：BFS 求解最少步数通关路径
		返回：路径长度；无解 BFS_NO_SOLUTION；超限 BFS_LIMIT_HIT
***********************************************************/
int bfsSolve(const Block blocks[BLOCK_COUNT], HintMove path[HINT_PATH_MAX]);

/***********************************************************
		void buildPathStates(const Block blocks[BLOCK_COUNT],
		                     const HintMove* path, int len,
		                     BoardState pathStates[HINT_PATH_MAX + 1])
		参数：blocks 起点；path 路径；len 步数；pathStates 输出各步局面
		功能：模拟执行路径，重建每步后的棋盘状态
		返回：无
***********************************************************/
void buildPathStates(const Block blocks[BLOCK_COUNT], const HintMove* path, int len,
                     BoardState pathStates[HINT_PATH_MAX + 1]);

/***********************************************************
		void clearHint(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：清除全部提示状态与高亮
		返回：无
***********************************************************/
void clearHint(GameContext* ctx);

/***********************************************************
		int activateHintOneStep(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：BFS 求解并激活一步提示
		返回：路径长度或错误码（同 bfsSolve）
***********************************************************/
int activateHintOneStep(GameContext* ctx);

/***********************************************************
		int activateHintFull(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：BFS 求解并激活完整路径提示
		返回：路径长度或错误码（同 bfsSolve）
***********************************************************/
int activateHintFull(GameContext* ctx);

/***********************************************************
		void hintAfterSessionCommit(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：走子会话提交后校验是否仍符合提示路径
		返回：无
***********************************************************/
void hintAfterSessionCommit(GameContext* ctx);

/***********************************************************
		void hintOnBoardChanged(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：棋盘变化后更新或清除提示
		返回：无
***********************************************************/
void hintOnBoardChanged(GameContext* ctx);

/***********************************************************
		bool hintSelfTest(void)
		功能：自检三关初始局面 BFS 是否均可解
		返回：全部通过返回 true，否则 false
***********************************************************/
bool hintSelfTest(void);

#endif
