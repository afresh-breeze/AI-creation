/*
  hint.cpp - BFS 提示与路线引导实现

  核心流程：encode -> canonicalKey -> 哈希去重 -> BFS(slideBlockToStop) -> 回溯路径
  UI 层：activateHint* 调用 bfsSolve；玩家走子后 hintOnBoardChanged 推进或清除提示
 */
#include "hint.h"
#include "ui.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>

#define VISIT_HASH_SIZE  131072  // 访问表槽位数，须为 2 的幂以便位与取模
#define VISIT_EMPTY      (-1)    // 空槽标记

/* BFS 搜索树节点：存原始局面、父节点下标、到达本节点的那一步 */
typedef struct {
    BoardState state;   // 原始编码（保留 blockId 与坐标的对应关系）
    int parent;         // 父节点在 s_nodes 中的下标，根为 -1
    HintMove move;      // 从父到本节点的移动；根节点无效
} BfsNode;

static BfsNode s_nodes[BFS_MAX_NODES];      // 节点池
static int s_queue[BFS_MAX_NODES];          // BFS 环形队列（此处用 head/tail 下标）
static int s_visitSlots[VISIT_HASH_SIZE];   // 开放寻址：槽位 -> 节点下标

/***********************************************************
		void setHintHighlight(GameContext* ctx, int blockId, Direction dir)
		参数：ctx 游戏上下文；blockId 方块 id；dir 建议滑动方向
		功能：设置 UI 高亮
		返回：无
***********************************************************/
static void setHintHighlight(GameContext* ctx, int blockId, Direction dir)
{
    ctx->hintBlockId = blockId;
    ctx->hintDir = dir;
}

/***********************************************************
		void clearHint(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：清除全部提示状态与高亮
		返回：无
***********************************************************/
void clearHint(GameContext* ctx)
{
    // 重置全部提示相关字段，UI 不再绘制高亮
    ctx->hintMode = HINT_NONE;
    ctx->hintBlockId = -1;
    ctx->hintPathLen = 0;
    ctx->hintPathIndex = 0;
}

/***********************************************************
		void encodeBoardState(const Block blocks[BLOCK_COUNT], BoardState* s)
		参数：blocks 方块数组；s 输出状态
		功能：将方块坐标编码为 BoardState
		返回：无
***********************************************************/
void encodeBoardState(const Block blocks[BLOCK_COUNT], BoardState* s)
{
    for (int i = 0; i < BLOCK_COUNT; i++) {
        s->bx[i] = blocks[i].x;
        s->by[i] = blocks[i].y;
    }
}

/***********************************************************
		bool boardStateEqual(const BoardState* a, const BoardState* b)
		参数：a、b 两个棋盘状态
		功能：比较两状态是否完全相同
		返回：相同返回 true，否则 false
***********************************************************/
bool boardStateEqual(const BoardState* a, const BoardState* b)
{
    for (int i = 0; i < BLOCK_COUNT; i++) {
        if (a->bx[i] != b->bx[i] || a->by[i] != b->by[i]) {
            return false;
        }
    }
    return true;
}

/***********************************************************
		void applyBoardState(const BoardState* s, Block blocks[BLOCK_COUNT])
		参数：s 棋盘状态；blocks 输出方块数组
		功能：将状态中的坐标写回 blocks
		返回：无
***********************************************************/
void applyBoardState(const BoardState* s, Block blocks[BLOCK_COUNT])
{
    for (int i = 0; i < BLOCK_COUNT; i++) {
        blocks[i].x = s->bx[i];
        blocks[i].y = s->by[i];
    }
}

/***********************************************************
		void sortBlockPairs(int bx[], int by[], int count)
		参数：bx、by 坐标数组；count 对数
		功能：按 (y,x) 字典序排序，用于规范化相同棋子
		返回：无
***********************************************************/
static void sortBlockPairs(int bx[], int by[], int count)
{
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (by[i] > by[j] || (by[i] == by[j] && bx[i] > bx[j])) {
                int tx = bx[i];
                bx[i] = bx[j];
                bx[j] = tx;
                int ty = by[i];
                by[i] = by[j];
                by[j] = ty;
            }
        }
    }
}

/***********************************************************
		void canonicalKey(const BoardState* raw, BoardState* key)
		参数：raw 原始状态；key 输出规范键
		功能：生成用于去重的规范键（四将、四兵坐标排序）
		返回：无
***********************************************************/
static void canonicalKey(const BoardState* raw, BoardState* key)
{
    *key = *raw;
    sortBlockPairs(&key->bx[2], &key->by[2], 4);
    sortBlockPairs(&key->bx[6], &key->by[6], 4);
}

/***********************************************************
		void buildPathStates(const Block blocks[BLOCK_COUNT],
		                     const HintMove* path, int len,
		                     BoardState pathStates[HINT_PATH_MAX + 1])
		参数：blocks 起点；path 路径；len 步数；pathStates 输出各步局面
		功能：模拟执行路径，重建每步后的棋盘状态
		返回：无
***********************************************************/
void buildPathStates(const Block blocks[BLOCK_COUNT], const HintMove* path, int len,
                     BoardState pathStates[HINT_PATH_MAX + 1])
{
    Block temp[BLOCK_COUNT];
    memcpy(temp, blocks, sizeof(Block) * BLOCK_COUNT);
    encodeBoardState(temp, &pathStates[0]);

    for (int i = 0; i < len; i++) {
        slideBlockToStop(temp, path[i].blockId, path[i].dir);
        encodeBoardState(temp, &pathStates[i + 1]);
    }
}

/***********************************************************
		unsigned int boardHash(const BoardState* s)
		参数：s 棋盘状态
		功能：FNV-1a 风格哈希
		返回：哈希值
***********************************************************/
static unsigned int boardHash(const BoardState* s)
{
    unsigned int h = 2166136261u;
    for (int i = 0; i < BLOCK_COUNT; i++) {
        h ^= (unsigned int)(s->bx[i] + (s->by[i] << 4));
        h *= 16777619u;
    }
    return h;
}

/***********************************************************
		void visitClear(void)
		功能：清空访问表
		返回：无
***********************************************************/
static void visitClear(void)
{
    memset(s_visitSlots, 0xFF, sizeof(s_visitSlots));
}

/***********************************************************
		void visitInsertFailedDebug(int nodeIndex)
		参数：nodeIndex 节点下标
		功能：哈希表插槽满时仅 Debug 输出
		返回：无
***********************************************************/
static void visitInsertFailedDebug(int nodeIndex)
{
#ifdef _DEBUG
    char buf[128];
    sprintf_s(buf, sizeof(buf), "hint: visitInsert failed at node %d\n", nodeIndex);
    OutputDebugStringA(buf);
#else
    (void)nodeIndex;
#endif
}

/***********************************************************
		int visitFindKey(const BoardState* key)
		参数：key 规范键
		功能：开放寻址查找是否已访问过等价局面
		返回：节点下标，未找到返回 -1
***********************************************************/
static int visitFindKey(const BoardState* key)
{
    unsigned int h = boardHash(key);
    for (int probe = 0; probe < VISIT_HASH_SIZE; probe++) {
        int slot = (int)((h + (unsigned int)probe) & (VISIT_HASH_SIZE - 1));
        int nodeIdx = s_visitSlots[slot];
        if (nodeIdx == VISIT_EMPTY) {
            return -1;
        }
        BoardState nodeKey;
        canonicalKey(&s_nodes[nodeIdx].state, &nodeKey);
        if (boardStateEqual(&nodeKey, key)) {
            return nodeIdx;
        }
    }
    return -1;
}

/***********************************************************
		bool visitInsert(int nodeIndex)
		参数：nodeIndex 节点下标
		功能：将节点插入访问表
		返回：成功 true，表满 false
***********************************************************/
static bool visitInsert(int nodeIndex)
{
    BoardState key;
    canonicalKey(&s_nodes[nodeIndex].state, &key);
    unsigned int h = boardHash(&key);
    for (int probe = 0; probe < VISIT_HASH_SIZE; probe++) {
        int slot = (int)((h + (unsigned int)probe) & (VISIT_HASH_SIZE - 1));
        if (s_visitSlots[slot] == VISIT_EMPTY) {
            s_visitSlots[slot] = nodeIndex;
            return true;
        }
    }
    visitInsertFailedDebug(nodeIndex);
    return false;
}

/***********************************************************
		void showHintCalculating(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：BFS 计算期间刷新游戏页并显示「正在计算提示...」
		返回：无
***********************************************************/
static void showHintCalculating(const GameContext* ctx)
{
    drawPlayingPage(ctx);
    setUIFont(FONT_BODY_SIZE, false);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);
    outtextxy(BOARD_ORIGIN_X, BOARD_ORIGIN_Y - 30, _T("正在计算提示..."));
    FlushBatchDraw();
}

/***********************************************************
		void copyBlocksWithState(const Block templateBlocks[BLOCK_COUNT],
		                         const BoardState* s, Block blocks[BLOCK_COUNT])
		参数：templateBlocks 模板方块；s 棋盘状态；blocks 输出方块数组
		功能：用模板标签/颜色叠加状态坐标，得到可 slide 的 Block 数组
		返回：无
***********************************************************/
static void copyBlocksWithState(const Block templateBlocks[BLOCK_COUNT],
                                const BoardState* s, Block blocks[BLOCK_COUNT])
{
    memcpy(blocks, templateBlocks, sizeof(Block) * BLOCK_COUNT);
    applyBoardState(s, blocks);
}

/***********************************************************
		int bfsSolve(const Block blocks[BLOCK_COUNT], HintMove path[HINT_PATH_MAX])
		参数：blocks 当前棋盘；path 输出最短路径
		功能：BFS 求解最少步数通关路径
		返回：路径长度；无解 BFS_NO_SOLUTION；超限 BFS_LIMIT_HIT
***********************************************************/
int bfsSolve(const Block blocks[BLOCK_COUNT], HintMove path[HINT_PATH_MAX])
{
    int nodeCount = 0;
    int head = 0;
    int tail = 0;

    visitClear();

    Block templateBlocks[BLOCK_COUNT];
    memcpy(templateBlocks, blocks, sizeof(Block) * BLOCK_COUNT);

    if (checkWin(blocks)) {
        return 0;  // 已在胜利状态，路径长度为 0
    }

    s_nodes[0].parent = -1;
    encodeBoardState(blocks, &s_nodes[0].state);
    s_nodes[0].move.blockId = -1;
    if (!visitInsert(0)) {
        return BFS_NO_SOLUTION;
    }
    nodeCount = 1;
    s_queue[tail++] = 0;

    int goalIndex = -1;

    while (head < tail) {
        int cur = s_queue[head++];
        Block curBlocks[BLOCK_COUNT];
        copyBlocksWithState(templateBlocks, &s_nodes[cur].state, curBlocks);

        if (checkWin(curBlocks)) {
            goalIndex = cur;
            break;
        }

        // 枚举：每个方块 x 四个方向，能滑动则生成子状态
        for (int i = 0; i < BLOCK_COUNT; i++) {
            for (int d = DIR_UP; d <= DIR_RIGHT; d++) {
                Block nextBlocks[BLOCK_COUNT];
                memcpy(nextBlocks, curBlocks, sizeof(Block) * BLOCK_COUNT);
                if (!slideBlockToStop(nextBlocks, i, (Direction)d)) {
                    continue;
                }

                BoardState nextState;
                BoardState key;
                encodeBoardState(nextBlocks, &nextState);
                canonicalKey(&nextState, &key);
                if (visitFindKey(&key) >= 0) {
                    continue;
                }

                if (nodeCount >= BFS_MAX_NODES) {
                    return BFS_LIMIT_HIT;
                }

                s_nodes[nodeCount].state = nextState;
                s_nodes[nodeCount].parent = cur;
                s_nodes[nodeCount].move.blockId = i;
                s_nodes[nodeCount].move.dir = (Direction)d;
                if (!visitInsert(nodeCount)) {
                    continue;
                }
                s_queue[tail++] = nodeCount;

                if (checkWin(nextBlocks)) {
                    goalIndex = nodeCount;
                }
                nodeCount++;

                if (goalIndex >= 0) {
                    break;
                }
            }
            if (goalIndex >= 0) {
                break;
            }
        }
        if (goalIndex >= 0) {
            break;
        }
    }

    if (goalIndex < 0) {
        return BFS_NO_SOLUTION;
    }

    // 从目标沿 parent 收集边，再反转为从起点到终点的顺序
    HintMove rev[HINT_PATH_MAX];
    int revLen = 0;
    int idx = goalIndex;
    while (s_nodes[idx].parent >= 0) {
        if (revLen >= HINT_PATH_MAX) {
            return BFS_LIMIT_HIT;
        }
        rev[revLen++] = s_nodes[idx].move;
        idx = s_nodes[idx].parent;
    }

    for (int i = 0; i < revLen; i++) {
        path[i] = rev[revLen - 1 - i];
    }
    return revLen;
}

/***********************************************************
		bool applyBfsResult(GameContext* ctx, HintMove* path, int len, HintMode mode)
		参数：ctx 游戏上下文；path 路径；len 步数；mode 提示模式
		功能：将 BFS 结果写入 ctx，并高亮第一步
		返回：成功 true，否则 false
***********************************************************/
static bool applyBfsResult(GameContext* ctx, HintMove* path, int len, HintMode mode)
{
    if (len < 0) {
        return false;
    }

    ctx->hintPathLen = len;
    memcpy(ctx->hintPath, path, sizeof(HintMove) * len);
    buildPathStates(ctx->blocks, ctx->hintPath, len, ctx->hintPathStates);
    ctx->hintPathIndex = 0;
    ctx->hintMode = mode;
    setHintHighlight(ctx, ctx->hintPath[0].blockId, ctx->hintPath[0].dir);
    return true;
}

/***********************************************************
		int activateHintOneStep(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：BFS 求解并激活一步提示
		返回：路径长度或错误码（同 bfsSolve）
***********************************************************/
int activateHintOneStep(GameContext* ctx)
{
    HintMove path[HINT_PATH_MAX];
    showHintCalculating(ctx);
    int len = bfsSolve(ctx->blocks, path);
    if (len <= 0) {
        clearHint(ctx);
        return len;
    }
    applyBfsResult(ctx, path, len, HINT_ONE_STEP);
    return len;
}

/***********************************************************
		int activateHintFull(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：BFS 求解并激活完整路径提示
		返回：路径长度或错误码（同 bfsSolve）
***********************************************************/
int activateHintFull(GameContext* ctx)
{
    HintMove path[HINT_PATH_MAX];
    showHintCalculating(ctx);
    int len = bfsSolve(ctx->blocks, path);
    if (len <= 0) {
        clearHint(ctx);
        return len;
    }
    applyBfsResult(ctx, path, len, HINT_FULL);
    return len;
}

/***********************************************************
		bool hintSelfTest(void)
		功能：自检三关初始局面 BFS 是否均可解
		返回：全部通过返回 true，否则 false
***********************************************************/
bool hintSelfTest(void)
{
    Block blocks[BLOCK_COUNT];
    HintMove path[HINT_PATH_MAX];

    for (int level = 0; level < LEVEL_COUNT; level++) {
        loadLevel(level, blocks);
        int len = bfsSolve(blocks, path);
        if (len <= 0) {
            return false;
        }
    }
    return true;
}

/***********************************************************
		void clearOneStepIfCompleted(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：一步提示：若当前局面已等于执行第一步后的 pathStates[1]，则清除高亮
		返回：无
***********************************************************/
static void clearOneStepIfCompleted(GameContext* ctx)
{
    if (ctx->hintMode != HINT_ONE_STEP || ctx->hintPathLen < 1) {
        return;
    }

    BoardState cur;
    encodeBoardState(ctx->blocks, &cur);
    if (boardStateEqual(&cur, &ctx->hintPathStates[1])) {
        clearHint(ctx);
    }
}

/***********************************************************
		void advanceFullHintIfMatched(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：完整提示推进；全部走完则 clearHint
		返回：无
***********************************************************/
static void advanceFullHintIfMatched(GameContext* ctx)
{
    BoardState cur;
    encodeBoardState(ctx->blocks, &cur);
    int next = ctx->hintPathIndex + 1;

    if (!boardStateEqual(&cur, &ctx->hintPathStates[next])) {
        return;
    }

    ctx->hintPathIndex++;
    if (ctx->hintPathIndex >= ctx->hintPathLen) {
        clearHint(ctx);
        return;
    }

    setHintHighlight(ctx,
        ctx->hintPath[ctx->hintPathIndex].blockId,
        ctx->hintPath[ctx->hintPathIndex].dir);
}

/***********************************************************
		void hintAfterSessionCommit(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：走子会话提交后校验是否仍符合提示路径
		返回：无
***********************************************************/
void hintAfterSessionCommit(GameContext* ctx)
{
    if (ctx->hintMode == HINT_NONE) {
        return;
    }

    BoardState cur;
    encodeBoardState(ctx->blocks, &cur);

    if (ctx->hintMode == HINT_ONE_STEP) {
        if (!boardStateEqual(&cur, &ctx->hintPathStates[1])) {
            clearHint(ctx);
        }
        return;
    }

    if (!boardStateEqual(&cur, &ctx->hintPathStates[ctx->hintPathIndex])) {
        clearHint(ctx);
    }
}

/***********************************************************
		void hintOnBoardChanged(GameContext* ctx)
		参数：ctx 游戏上下文
		功能：棋盘变化后更新或清除提示
		返回：无
***********************************************************/
void hintOnBoardChanged(GameContext* ctx)
{
    if (ctx->hintMode == HINT_FULL) {
        advanceFullHintIfMatched(ctx);
    }
    clearOneStepIfCompleted(ctx);
}
