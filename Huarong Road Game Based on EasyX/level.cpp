/*
  level.cpp - 关卡数据与棋盘绘制

  三关初始布局来自经典华容道名局；提供 loadLevel 与 drawBoard。
  绘制顺序：背景网格 -> 非选中方块 -> 选中方块+黄框 -> 提示覆盖层（最顶）。
 */
#include "level.h"
#include "ui.h"
#include <string.h>

// 关卡中文名称，与 LEVEL_0/1/2 下标一一对应
static const TCHAR* LEVEL_NAMES[LEVEL_COUNT] = {
    _T("横刀立马"),
    _T("齐头并进"),
    _T("兵分三路")
};

// 各方块默认配色（关卡静态表里也可内联 RGB，此处供 makeBlock 等复用）
static const COLORREF COLOR_CAOCAO = RGB(200, 70, 60);
static const COLORREF COLOR_GUANYU = RGB(70, 140, 80);
static const COLORREF COLOR_ZHANGFEI = RGB(70, 100, 180);
static const COLORREF COLOR_ZHAOYUN = RGB(60, 150, 160);
static const COLORREF COLOR_MACHAO = RGB(130, 80, 160);
static const COLORREF COLOR_HUANGZHONG = RGB(180, 120, 60);
static const COLORREF COLOR_SOLDIER = RGB(160, 150, 140);

/***********************************************************
		static Block makeBlock(int id, const TCHAR* label,
		                       int x, int y, int w, int h, COLORREF color)
		参数：id 方块 id；label 名称；x/y/w/h 位置与尺寸；color 颜色
		功能：构造一个 Block 值
		返回：Block 结构体
***********************************************************/
static Block makeBlock(int id, const TCHAR* label, int x, int y, int w, int h, COLORREF color)
{
    Block b;
    b.id = id;
    _tcscpy_s(b.label, label);
    b.x = x;
    b.y = y;
    b.w = w;
    b.h = h;
    b.color = color;
    return b;
}

/*
  关卡1：横刀立马
     0    1    2    3
  0  张   曹   曹   赵
  1  张   曹   曹   赵
  2  马   关   关   黄
  3  马   兵   兵   黄
  4  兵   ·   ·   兵
 */
static const Block LEVEL_0[BLOCK_COUNT] = {
    { 0, _T("曹操"), 1, 0, 2, 2, RGB(200, 70, 60) },
    { 1, _T("关羽"), 1, 2, 2, 1, RGB(70, 140, 80) },
    { 2, _T("张飞"), 0, 0, 1, 2, RGB(70, 100, 180) },
    { 3, _T("赵云"), 3, 0, 1, 2, RGB(60, 150, 160) },
    { 4, _T("马超"), 0, 2, 1, 2, RGB(130, 80, 160) },
    { 5, _T("黄忠"), 3, 2, 1, 2, RGB(180, 120, 60) },
    { 6, _T("卒"), 1, 3, 1, 1, RGB(160, 150, 140) },
    { 7, _T("卒"), 2, 3, 1, 1, RGB(160, 150, 140) },
    { 8, _T("卒"), 0, 4, 1, 1, RGB(160, 150, 140) },
    { 9, _T("卒"), 3, 4, 1, 1, RGB(160, 150, 140) }
};

/*
  关卡2：齐头并进
     0    1    2    3
  0  张   曹   曹   赵
  1  张   曹   曹   赵
  2  兵   兵   兵   兵
  3  马   关   关   黄
  4  马   ·   ·   黄
 */
static const Block LEVEL_1[BLOCK_COUNT] = {
    { 0, _T("曹操"), 1, 0, 2, 2, RGB(200, 70, 60) },
    { 1, _T("关羽"), 1, 3, 2, 1, RGB(70, 140, 80) },
    { 2, _T("张飞"), 0, 0, 1, 2, RGB(70, 100, 180) },
    { 3, _T("赵云"), 3, 0, 1, 2, RGB(60, 150, 160) },
    { 4, _T("马超"), 0, 3, 1, 2, RGB(130, 80, 160) },
    { 5, _T("黄忠"), 3, 3, 1, 2, RGB(180, 120, 60) },
    { 6, _T("卒"), 0, 2, 1, 1, RGB(160, 150, 140) },
    { 7, _T("卒"), 1, 2, 1, 1, RGB(160, 150, 140) },
    { 8, _T("卒"), 2, 2, 1, 1, RGB(160, 150, 140) },
    { 9, _T("卒"), 3, 2, 1, 1, RGB(160, 150, 140) }
};

/*
  关卡3：兵分三路
     0    1    2    3
  0  兵   曹   曹   兵
  1  张   曹   曹   赵
  2  张   关   关   赵
  3  马   兵   兵   黄
  4  马   ·   ·   黄
 */
static const Block LEVEL_2[BLOCK_COUNT] = {
    { 0, _T("曹操"), 1, 0, 2, 2, RGB(200, 70, 60) },
    { 1, _T("关羽"), 1, 2, 2, 1, RGB(70, 140, 80) },
    { 2, _T("张飞"), 0, 1, 1, 2, RGB(70, 100, 180) },
    { 3, _T("赵云"), 3, 1, 1, 2, RGB(60, 150, 160) },
    { 4, _T("马超"), 0, 3, 1, 2, RGB(130, 80, 160) },
    { 5, _T("黄忠"), 3, 3, 1, 2, RGB(180, 120, 60) },
    { 6, _T("卒"), 0, 0, 1, 1, RGB(160, 150, 140) },
    { 7, _T("卒"), 3, 0, 1, 1, RGB(160, 150, 140) },
    { 8, _T("卒"), 1, 3, 1, 1, RGB(160, 150, 140) },
    { 9, _T("卒"), 2, 3, 1, 1, RGB(160, 150, 140) }
};

/***********************************************************
		const TCHAR* getLevelName(int levelIndex)
		参数：levelIndex 关卡索引
		功能：返回关卡中文名称
		返回：关卡名字符串；越界返回「未知」
***********************************************************/
const TCHAR* getLevelName(int levelIndex)
{
    // 越界返回「未知」，避免 UI 显示空指针
    if (levelIndex >= 0 && levelIndex < LEVEL_COUNT) {
        return LEVEL_NAMES[levelIndex];
    }
    return _T("未知");
}

/***********************************************************
		void loadLevel(int levelIndex, Block blocks[BLOCK_COUNT])
		参数：levelIndex 关卡索引；blocks 输出方块数组
		功能：将指定关卡初始布局拷贝到 blocks
		返回：无
***********************************************************/
void loadLevel(int levelIndex, Block blocks[BLOCK_COUNT])
{
    const Block* src = NULL;
    switch (levelIndex) {
    case 0: src = LEVEL_0; break;
    case 1: src = LEVEL_1; break;
    case 2: src = LEVEL_2; break;
    default: src = LEVEL_0; break;
    }
    memcpy(blocks, src, sizeof(Block) * BLOCK_COUNT);
}

/***********************************************************
		static void drawBlock(const Block* b, int originX, int originY)
		参数：b 方块指针；originX/Y 棋盘原点像素坐标
		功能：绘制单个方块（填充、黑边、居中文字）
		返回：无
***********************************************************/
static void drawBlock(const Block* b, int originX, int originY)
{
    int px = originX + b->x * CELL_SIZE;
    int py = originY + b->y * CELL_SIZE;
    int pw = b->w * CELL_SIZE;
    int ph = b->h * CELL_SIZE;

    setfillcolor(b->color);
    setlinecolor(COLOR_BTN_TEXT);
    setlinestyle(PS_SOLID, 2);
    fillrectangle(px, py, px + pw, py + ph);
    rectangle(px, py, px + pw, py + ph);

    int fontSize = FONT_BTN_SIZE;
    if (b->w == 1 && b->h == 2) {
        fontSize = 16;  // 竖长将旗字号略小，避免溢出
    }
    setUIFont(fontSize, true);
    settextcolor(RGB(255, 255, 255));
    setbkmode(TRANSPARENT);
    int tw = textwidth(b->label);
    int th = textheight(b->label);
    outtextxy(px + (pw - tw) / 2, py + (ph - th) / 2, b->label);
}

/***********************************************************
		static void drawSelectionOutline(const Block* b, int originX, int originY)
		参数：b 方块指针；originX/Y 棋盘原点像素坐标
		功能：绘制选中方块黄色外框
		返回：无
***********************************************************/
static void drawSelectionOutline(const Block* b, int originX, int originY)
{
    int px = originX + b->x * CELL_SIZE;
    int py = originY + b->y * CELL_SIZE;
    int pw = b->w * CELL_SIZE;
    int ph = b->h * CELL_SIZE;

    setlinecolor(RGB(255, 200, 0));
    setlinestyle(PS_SOLID, 4);
    rectangle(px - 2, py - 2, px + pw + 2, py + ph + 2);
}

/***********************************************************
		static void drawHintOverlay(const Block* b, int originX, int originY,
		                            int hintDir)
		参数：b 方块指针；originX/Y 棋盘原点；hintDir 提示方向
		功能：绘制提示高亮层（铺色、重绘文字、青色外框、方向箭头）
		返回：无
***********************************************************/
static void drawHintOverlay(const Block* b, int originX, int originY, int hintDir)
{
    Direction dir = (Direction)hintDir;
    int px = originX + b->x * CELL_SIZE;
    int py = originY + b->y * CELL_SIZE;
    int pw = b->w * CELL_SIZE;
    int ph = b->h * CELL_SIZE;
    int cx = px + pw / 2;
    int cy = py + ph / 2;

    setfillcolor(b->color);
    setlinecolor(b->color);
    setlinestyle(PS_SOLID, 1);
    fillrectangle(px, py, px + pw, py + ph);

    int fontSize = FONT_BTN_SIZE;
    if (b->w == 1 && b->h == 2) {
        fontSize = 16;
    }
    setUIFont(fontSize, true);
    settextcolor(RGB(255, 255, 255));
    setbkmode(TRANSPARENT);
    int tw = textwidth(b->label);
    int th = textheight(b->label);
    outtextxy(px + (pw - tw) / 2, py + (ph - th) / 2, b->label);

    setlinecolor(RGB(0, 180, 220));
    setlinestyle(PS_SOLID, 4);
    rectangle(px - 2, py - 2, px + pw + 2, py + ph + 2);

    setlinestyle(PS_SOLID, 3);
    int arrow = 18;  // 箭头半长（像素）
    switch (dir) {
    case DIR_UP:
        line(cx, cy - arrow, cx, cy + arrow);
        line(cx, cy - arrow, cx - 8, cy - arrow + 10);
        line(cx, cy - arrow, cx + 8, cy - arrow + 10);
        break;
    case DIR_DOWN:
        line(cx, cy - arrow, cx, cy + arrow);
        line(cx, cy + arrow, cx - 8, cy + arrow - 10);
        line(cx, cy + arrow, cx + 8, cy + arrow - 10);
        break;
    case DIR_LEFT:
        line(cx - arrow, cy, cx + arrow, cy);
        line(cx - arrow, cy, cx - arrow + 10, cy - 8);
        line(cx - arrow, cy, cx - arrow + 10, cy + 8);
        break;
    case DIR_RIGHT:
        line(cx - arrow, cy, cx + arrow, cy);
        line(cx + arrow, cy, cx + arrow - 10, cy - 8);
        line(cx + arrow, cy, cx + arrow - 10, cy + 8);
        break;
    }
    setlinestyle(PS_SOLID, 1);
}

/***********************************************************
		void drawBoard(const Block blocks[BLOCK_COUNT], int originX,
		               int originY, int selectedIndex, int hintBlockId,
		               int hintDir, bool showHint)
		参数：blocks 方块数组；originX/Y 棋盘左上角像素坐标；
		      selectedIndex 选中方块 id；hintBlockId/hintDir/showHint 提示高亮
		功能：绘制 4x5 棋盘、方块及选中/提示效果
		返回：无
***********************************************************/
void drawBoard(const Block blocks[BLOCK_COUNT], int originX, int originY,
               int selectedIndex, int hintBlockId, int hintDir, bool showHint)
{
    int boardW = BOARD_COLS * CELL_SIZE;
    int boardH = BOARD_ROWS * CELL_SIZE;

    setfillcolor(RGB(220, 200, 170));
    setlinecolor(COLOR_BTN_TEXT);
    setlinestyle(PS_SOLID, 1);
    fillrectangle(originX, originY, originX + boardW, originY + boardH);
    rectangle(originX, originY, originX + boardW, originY + boardH);

    // 竖线、横线划分 4x5 格子
    for (int c = 1; c < BOARD_COLS; c++) {
        int x = originX + c * CELL_SIZE;
        line(x, originY, x, originY + boardH);
    }
    for (int r = 1; r < BOARD_ROWS; r++) {
        int y = originY + r * CELL_SIZE;
        line(originX, y, originX + boardW, y);
    }

    // 底部出口：第 1~2 列（曹操 2 格宽）下方画开口，提示逃脱位置
    int exitX1 = originX + 1 * CELL_SIZE;
    int exitX2 = originX + 3 * CELL_SIZE;
    int exitY = originY + boardH;
    setlinestyle(PS_SOLID, 3);
    line(exitX1, exitY, exitX1, exitY + 12);
    line(exitX2, exitY, exitX2, exitY + 12);
    line(exitX1, exitY + 12, exitX2, exitY + 12);
    setlinestyle(PS_SOLID, 1);

    // 先画未选中块，再画选中块，保证选中黄框不被邻块遮挡
    for (int i = 0; i < BLOCK_COUNT; i++) {
        if (i != selectedIndex) {
            drawBlock(&blocks[i], originX, originY);
        }
    }
    if (selectedIndex >= 0 && selectedIndex < BLOCK_COUNT) {
        drawBlock(&blocks[selectedIndex], originX, originY);
        drawSelectionOutline(&blocks[selectedIndex], originX, originY);
    }
    if (showHint && hintBlockId >= 0 && hintBlockId < BLOCK_COUNT) {
        drawHintOverlay(&blocks[hintBlockId], originX, originY, hintDir);
    }
}
