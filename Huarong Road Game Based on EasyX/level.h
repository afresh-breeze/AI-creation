/*
  level.h - 关卡与棋盘模块
  坐标约定：x 为列 (0~3)，y 为行 (0~4)
  像素：棋盘每格 CELL_SIZE 像素，原点 BOARD_ORIGIN_X/Y 为左上角
 */
#ifndef LEVEL_H
#define LEVEL_H

#include <graphics.h>
#include <tchar.h>

#define BOARD_COLS      4       // 棋盘列数（逻辑坐标 x 范围 0~3）
#define BOARD_ROWS      5       // 棋盘行数（逻辑坐标 y 范围 0~4）
#define BLOCK_COUNT     10      // 各方块总数（含曹操、五将、四卒）
#define CELL_SIZE       70      // 单格边长（像素）
#define BOARD_ORIGIN_X  120     // 棋盘绘制左上角 x
#define BOARD_ORIGIN_Y  130     // 棋盘绘制左上角 y

/* 方块：id/名称/逻辑位置(x,y)/占地(w×h 格)/填充色 */
typedef struct {
    int id;
    TCHAR label[8];
    int x, y;
    int w, h;
    COLORREF color;
} Block;

/***********************************************************
		void loadLevel(int levelIndex, Block blocks[BLOCK_COUNT])
		参数：levelIndex 关卡索引；blocks 输出方块数组
		功能：将指定关卡初始布局拷贝到 blocks
		返回：无
***********************************************************/
void loadLevel(int levelIndex, Block blocks[BLOCK_COUNT]);

/***********************************************************
		const TCHAR* getLevelName(int levelIndex)
		参数：levelIndex 关卡索引
		功能：返回关卡中文名称
		返回：关卡名字符串；越界返回「未知」
***********************************************************/
const TCHAR* getLevelName(int levelIndex);

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
               int selectedIndex, int hintBlockId, int hintDir, bool showHint);

#endif
