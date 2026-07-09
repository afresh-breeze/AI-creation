/*
  ui.cpp - 界面绘制与点击检测

  负责：字体设置、按钮绘制、各页面 layout、鼠标碰撞检测。
  约定：每个页面的 drawXxx 与 handleXxxClick 共用 buildXxxButtons，
  避免只改绘制坐标导致点击区域错位。
 */
#include <Windows.h>
#include "ui.h"
#include "file_io.h"
#include "audio.h"
#include <stdio.h>

static int s_mouseX = -1;
static int s_mouseY = -1;

/***********************************************************
		void uiSetMousePos(int mx, int my)
		参数：mx/my 鼠标客户区坐标
		功能：更新当前鼠标位置，供按钮悬停高亮判定
		返回：无
***********************************************************/
void uiSetMousePos(int mx, int my)
{
    s_mouseX = mx;
    s_mouseY = my;
}

/***********************************************************
		void setUIFont(int size, bool bold)
		参数：size 字号；bold 是否加粗
		功能：设置中文 TrueType 字体
		返回：无
***********************************************************/
void setUIFont(int size, bool bold)
{
    LOGFONT font = { 0 };
    _tcscpy_s(font.lfFaceName, FONT_NAME);
    font.lfHeight = -size;
    font.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    font.lfQuality = CLEARTYPE_QUALITY;
    font.lfCharSet = DEFAULT_CHARSET;
    settextstyle(&font);
}

/***********************************************************
		bool isButtonClicked(const Button* btn, int mx, int my)
		参数：btn 按钮；mx/my 鼠标坐标
		功能：判断鼠标是否落在按钮矩形内
		返回：命中返回 true，否则 false
***********************************************************/
static bool isButtonClicked(const Button* btn, int mx, int my)
{
    return mx >= btn->x && mx <= btn->x + btn->w &&
           my >= btn->y && my <= btn->y + btn->h;
}

/***********************************************************
		void drawButton(const Button* btn)
		参数：btn 按钮描述
		功能：绘制单个圆角按钮（填充 + 边框 + 居中文字）
		返回：无
***********************************************************/
static void drawButton(const Button* btn)
{
    bool hover = (s_mouseX >= 0 && s_mouseY >= 0 &&
                  isButtonClicked(btn, s_mouseX, s_mouseY));
    setfillcolor(hover ? COLOR_BTN_HOVER : COLOR_BTN);
    setlinecolor(COLOR_BTN_TEXT);
    setlinestyle(PS_SOLID, 2);
    fillroundrect(btn->x, btn->y, btn->x + btn->w, btn->y + btn->h, 10, 10);
    roundrect(btn->x, btn->y, btn->x + btn->w, btn->y + btn->h, 10, 10);

    setUIFont(FONT_BTN_SIZE, false);
    settextcolor(COLOR_BTN_TEXT);
    setbkmode(TRANSPARENT);

    int textW = textwidth(btn->label);
    int textH = textheight(btn->label);
    int tx = btn->x + (btn->w - textW) / 2;
    int ty = btn->y + (btn->h - textH) / 2;
    outtextxy(tx, ty, btn->label);
}

/***********************************************************
		ButtonId checkButtons(const Button* buttons, int count, int mx, int my)
		参数：buttons 按钮数组；count 数量；mx/my 鼠标坐标
		功能：遍历按钮数组，返回第一个被点中的 id
		返回：ButtonId 或 BTN_NONE
***********************************************************/
static ButtonId checkButtons(const Button* buttons, int count, int mx, int my)
{
    for (int i = 0; i < count; i++) {
        if (isButtonClicked(&buttons[i], mx, my)) {
            return buttons[i].id;
        }
    }
    return BTN_NONE;
}

/***********************************************************
		void drawButtons(const Button* buttons, int count)
		参数：buttons 按钮数组；count 数量
		功能：绘制一组按钮
		返回：无
***********************************************************/
static void drawButtons(const Button* buttons, int count)
{
    for (int i = 0; i < count; i++) {
        drawButton(&buttons[i]);
    }
}

/***********************************************************
		void layoutButtonsVertical(Button* btns, int count, int startY, int step)
		参数：btns 按钮数组；count 数量；startY 首按钮顶部 y；step 相邻间距
		功能：垂直排列按钮（仅写 x、y）
		返回：无
***********************************************************/
static void layoutButtonsVertical(Button* btns, int count, int startY, int step)
{
    int y = startY;
    for (int i = 0; i < count; i++) {
        btns[i].x = BTN_X;
        btns[i].y = y;
        if (btns[i].w <= 0) btns[i].w = BTN_WIDTH;
        if (btns[i].h <= 0) btns[i].h = BTN_HEIGHT;
        y += step;
    }
}

/***********************************************************
		void layoutButtonsHorizontal(Button* btns, int count, int y, int btnWidth, int gap)
		参数：btns 按钮数组；count 数量；y 顶部 y；btnWidth 宽度；gap 间距
		功能：水平居中排列按钮
		返回：无
***********************************************************/
static void layoutButtonsHorizontal(Button* btns, int count, int y, int btnWidth, int gap)
{
    int totalW = count * btnWidth + (count - 1) * gap;
    int startX = (WIN_WIDTH - totalW) / 2;
    for (int i = 0; i < count; i++) {
        btns[i].x = startX + i * (btnWidth + gap);
        btns[i].y = y;
        btns[i].w = btnWidth;
        btns[i].h = BTN_HEIGHT;
    }
}

/***********************************************************
		void drawCenteredTitle(const TCHAR* title, int y, int size)
		参数：title 标题文字；y 顶部 y；size 字号
		功能：在窗口水平方向居中绘制标题
		返回：无
***********************************************************/
static void drawCenteredTitle(const TCHAR* title, int y, int size)
{
    setUIFont(size, true);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);
    int tw = textwidth(title);
    outtextxy((WIN_WIDTH - tw) / 2, y, title);
}

#define LB_TABLE_LEFT   180   // 排行榜表格区域左边界（像素）
#define LB_TABLE_RIGHT  620   // 排行榜表格区域右边界；三等分列中心据此计算

/***********************************************************
		void drawTextCenteredAt(int centerX, int y, const TCHAR* text)
		参数：centerX 水平中心；y 顶部 y；text 文本
		功能：以 centerX 为水平中心绘制文本（表头与数据列对齐用）
		返回：无
***********************************************************/
static void drawTextCenteredAt(int centerX, int y, const TCHAR* text)
{
    int tw = textwidth(text);
    outtextxy(centerX - tw / 2, y, text);
}

/***********************************************************
		void buildMenuButtons(Button* buttons, int* count)
		参数：buttons 输出按钮数组；count 输出数量
		功能：构建主菜单按钮（draw 与 click 共用）
		返回：无
***********************************************************/
static void buildMenuButtons(Button* buttons, int* count)
{
    buttons[0] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("开始游戏"), BTN_START };
    buttons[1] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("游戏规则"), BTN_RULES };
    buttons[2] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("排行榜"),   BTN_LEADERBOARD };
    buttons[3] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("退出游戏"), BTN_EXIT };
    layoutButtonsVertical(buttons, 4, 210, BTN_HEIGHT + BTN_GAP);
    *count = 4;
}

/***********************************************************
		void buildLeaderboardButtons(Button* buttons, int* count)
		参数：buttons 输出按钮数组；count 输出数量
		功能：构建排行榜页按钮（< > 切换关卡，底部返回主菜单）
		返回：无
***********************************************************/
static void buildLeaderboardButtons(Button* buttons, int* count)
{
    buttons[0] = { 30, 50, 40, 36, _T("<"), BTN_LB_PREV };
    buttons[1] = { 90, 50, 40, 36, _T(">"), BTN_LB_NEXT };
    buttons[2] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("返回"), BTN_LB_BACK };
    layoutButtonsVertical(&buttons[2], 1, 500, BTN_HEIGHT + BTN_GAP);
    buttons[2].x = BTN_X;
    *count = 3;
}

/***********************************************************
		void buildSelectButtons(Button* buttons, int* count)
		参数：buttons 输出按钮数组；count 输出数量
		功能：构建选关页按钮
		返回：无
***********************************************************/
static void buildSelectButtons(Button* buttons, int* count)
{
    buttons[0] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("横刀立马"), BTN_LEVEL_0 };
    buttons[1] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("齐头并进"), BTN_LEVEL_1 };
    buttons[2] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("兵分三路"), BTN_LEVEL_2 };
    buttons[3] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("返回"),     BTN_BACK };
    layoutButtonsVertical(buttons, 4, 160, BTN_HEIGHT + BTN_GAP);
    *count = 4;
}

/***********************************************************
		void buildRulesButtons(Button* buttons, int* count)
		参数：buttons 输出按钮数组；count 输出数量
		功能：构建规则页按钮
		返回：无
***********************************************************/
static void buildRulesButtons(Button* buttons, int* count)
{
    buttons[0] = { 0, 0, BTN_WIDTH, BTN_HEIGHT, _T("返回"), BTN_BACK };
    layoutButtonsVertical(buttons, 1, 430, BTN_HEIGHT + BTN_GAP);
    *count = 1;
}

/***********************************************************
		void buildPlayingButtons(Button* buttons, int* count)
		参数：buttons 输出按钮数组；count 输出数量
		功能：构建游戏页右侧竖排工具栏（x 靠右对齐，不遮挡棋盘）
		返回：无
***********************************************************/
static void buildPlayingButtons(Button* buttons, int* count)
{
    int x = WIN_WIDTH - BTN_WIDTH - 40;
    int y0 = 160;
    int step = BTN_HEIGHT + BTN_GAP;
    buttons[0] = { x, y0,               BTN_WIDTH, BTN_HEIGHT, _T("一步提示"), BTN_HINT_ONE };
    buttons[1] = { x, y0 + step,        BTN_WIDTH, BTN_HEIGHT, _T("完整提示"), BTN_HINT_FULL };
    buttons[2] = { x, y0 + step * 2,    BTN_WIDTH, BTN_HEIGHT, _T("悔棋"),     BTN_UNDO };
    buttons[3] = { x, y0 + step * 3,    BTN_WIDTH, BTN_HEIGHT, _T("重玩"),     BTN_REPLAY_LEVEL };
    buttons[4] = { x, y0 + step * 4,    BTN_WIDTH, BTN_HEIGHT, _T("返回选关"), BTN_BACK_SELECT };
    *count = 5;
}

/***********************************************************
		void buildWinButtons(Button* buttons, int* count, int currentLevel)
		参数：buttons 输出按钮数组；count 输出数量；currentLevel 当前关卡
		功能：构建通关页底部按钮；最后一关不显示「下一关」
		返回：无
***********************************************************/
static void buildWinButtons(Button* buttons, int* count, int currentLevel)
{
    if (currentLevel < LEVEL_COUNT - 1) {
        buttons[0] = { 0, 0, BTN_WIDTH_WIN, BTN_HEIGHT, _T("下一关"),   BTN_NEXT };
        buttons[1] = { 0, 0, BTN_WIDTH_WIN, BTN_HEIGHT, _T("重玩"),     BTN_REPLAY };
        buttons[2] = { 0, 0, BTN_WIDTH_WIN, BTN_HEIGHT, _T("返回选关"), BTN_BACK_SELECT_WIN };
        layoutButtonsHorizontal(buttons, 3, 380, BTN_WIDTH_WIN, BTN_GAP);
        *count = 3;
    } else {
        buttons[0] = { 0, 0, BTN_WIDTH_WIN, BTN_HEIGHT, _T("重玩"),     BTN_REPLAY };
        buttons[1] = { 0, 0, BTN_WIDTH_WIN, BTN_HEIGHT, _T("返回选关"), BTN_BACK_SELECT_WIN };
        layoutButtonsHorizontal(buttons, 2, 380, BTN_WIDTH_WIN, BTN_GAP);
        *count = 2;
    }
}

/***********************************************************
		void initUI(void)
		功能：初始化界面默认字体
		返回：无
***********************************************************/
void initUI(void)
{
    // 设置全局默认正文字体
    setUIFont(FONT_BODY_SIZE, false);
}

/***********************************************************
		void clearBackground(void)
		功能：清空窗口并填充背景色
		返回：无
***********************************************************/
void clearBackground(void)
{
    setbkcolor(COLOR_BG);
    cleardevice();
}

/***********************************************************
		void drawMenuPage(void)
		功能：绘制主菜单页面
		返回：无
***********************************************************/
void drawMenuPage(void)
{
    clearBackground();
    drawCenteredTitle(_T("华容道"), 100, FONT_TITLE_SIZE);

    Button buttons[4];
    int count = 0;
    buildMenuButtons(buttons, &count);
    drawButtons(buttons, count);
}

/***********************************************************
		void drawSelectPage(void)
		功能：绘制选关页面
		返回：无
***********************************************************/
void drawSelectPage(void)
{
    clearBackground();
    drawCenteredTitle(_T("选择关卡"), 70, FONT_TITLE_SIZE - 4);

    Button buttons[4];
    int count = 0;
    buildSelectButtons(buttons, &count);
    drawButtons(buttons, count);
}

/***********************************************************
		void drawRulesPage(void)
		功能：绘制游戏规则页面
		返回：无
***********************************************************/
void drawRulesPage(void)
{
    clearBackground();
    drawCenteredTitle(_T("游戏规则"), 70, FONT_TITLE_SIZE - 4);

    setUIFont(FONT_BODY_SIZE, false);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);

    const TCHAR* lines[] = {
        _T("1. 移动各方块，让曹操(2x2)从底部出口逃出。"),
        _T("2. 方块只能上下左右滑动，不能重叠或越界。"),
        _T("3. 出口位于棋盘底部中间两格。"),
        _T("4. 步数越少，成绩越好。")
    };

    int startY = 150;
    for (int i = 0; i < 4; i++) {
        outtextxy(80, startY + i * 44, lines[i]);
    }

    Button buttons[1];
    int count = 0;
    buildRulesButtons(buttons, &count);
    drawButtons(buttons, count);
}

/***********************************************************
		void drawLeaderboardPage(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：绘制排行榜页面
		返回：无
***********************************************************/
void drawLeaderboardPage(const GameContext* ctx)
{
    clearBackground();

    // colCenter[c] = 表格内第 c 列水平中心（c=0..3：排名/姓名/步数/用时）
    int tableWidth = LB_TABLE_RIGHT - LB_TABLE_LEFT;
    int tableCenterX = (LB_TABLE_LEFT + LB_TABLE_RIGHT) / 2;
    int colCenter[4];
    for (int c = 0; c < 4; c++) {
        colCenter[c] = LB_TABLE_LEFT + tableWidth * (c * 2 + 1) / 8;
    }

    setUIFont(FONT_TITLE_SIZE - 4, true);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);
    drawTextCenteredAt(tableCenterX, 110, _T("排行榜"));

    int level = ctx->leaderboardLevel;
    if (level < 0 || level >= LEVEL_COUNT) {
        level = 0;
    }

    setUIFont(FONT_BODY_SIZE, false);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);

    TCHAR levelText[64];
    _stprintf_s(levelText, _T("关卡：%s"), getLevelName(level));
    outtextxy(140, 58, levelText);

    setUIFont(FONT_BODY_SIZE, true);
    drawTextCenteredAt(colCenter[0], 170, _T("排名"));
    drawTextCenteredAt(colCenter[1], 170, _T("姓名"));
    drawTextCenteredAt(colCenter[2], 170, _T("步数"));
    drawTextCenteredAt(colCenter[3], 170, _T("用时"));

    int entryCount = 0;
    const LeaderboardEntry* entries = getLeaderboardTop(level, ctx->leaderboards,
                                                        &entryCount);

    setUIFont(FONT_BODY_SIZE, false);
    if (entryCount <= 0) {
        int tw = textwidth(_T("暂无记录"));
        outtextxy((WIN_WIDTH - tw) / 2, 240, _T("暂无记录"));
    } else {
        int rowY = 210;
        for (int i = 0; i < entryCount; i++) {
            TCHAR rankText[16];
            TCHAR stepText[16];
            TCHAR timeBuf[16];
            _stprintf_s(rankText, _T("%d"), i + 1);
            _stprintf_s(stepText, _T("%d"), entries[i].steps);
            formatTime(entries[i].timeSec, timeBuf, 16);
            drawTextCenteredAt(colCenter[0], rowY, rankText);
            drawTextCenteredAt(colCenter[1], rowY, entries[i].name);
            drawTextCenteredAt(colCenter[2], rowY, stepText);
            drawTextCenteredAt(colCenter[3], rowY, timeBuf);
            rowY += 32;
        }
    }

    Button buttons[3];
    int count = 0;
    buildLeaderboardButtons(buttons, &count);
    drawButtons(buttons, count);
}

/***********************************************************
		void drawPlayingPage(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：绘制游戏对局页面
		返回：无
***********************************************************/
void drawPlayingPage(const GameContext* ctx)
{
    clearBackground();

    TCHAR title[64];
    if (ctx->currentLevel >= 0 && ctx->currentLevel < LEVEL_COUNT) {
        _stprintf_s(title, _T("关卡：%s"), getLevelName(ctx->currentLevel));
    } else {
        _tcscpy_s(title, _T("关卡：未知"));
    }

    setUIFont(FONT_TITLE_SIZE - 16, true);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);
    outtextxy(40, 40, title);

    setUIFont(FONT_BODY_SIZE, false);
    TCHAR stepText[32];
    _stprintf_s(stepText, _T("步数：%d"), ctx->steps);
    outtextxy(40, 85, stepText);

    // pendingWin 期间冻结显示通关瞬间用时，避免计时继续跳动
    int elapsedSec;
    if (ctx->pendingWin && ctx->winTimeSec >= 0) {
        elapsedSec = ctx->winTimeSec;
    } else {
        elapsedSec = (int)((GetTickCount() - ctx->levelStartTick) / 1000);
    }
    TCHAR timeText[32];
    TCHAR timeBuf[16];
    formatTime(elapsedSec, timeBuf, 16);
    _stprintf_s(timeText, _T("时间：%s"), timeBuf);
    outtextxy(40 + textwidth(stepText) + 40, 85, timeText);

    //绘制 4x5 棋盘与方块
    bool showHint = (ctx->hintMode != HINT_NONE && ctx->hintBlockId >= 0);
    drawBoard(ctx->blocks, BOARD_ORIGIN_X, BOARD_ORIGIN_Y, ctx->selectedBlockId,
              ctx->hintBlockId, (int)ctx->hintDir, showHint);

    Button buttons[5];
    int count = 0;
    buildPlayingButtons(buttons, &count);
    drawButtons(buttons, count);
}

/***********************************************************
		void drawWinPage(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：绘制通关结算页面
		返回：无
***********************************************************/
void drawWinPage(const GameContext* ctx)
{
    clearBackground();

    setUIFont(FONT_TITLE_SIZE + 2, true);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);
    const TCHAR* winTitle = _T("恭喜通关！");
    int titleW = textwidth(winTitle);
    outtextxy((WIN_WIDTH - titleW) / 2 + 8, 90, winTitle);  // +8 补偿字体视觉重心偏左

    setUIFont(FONT_BODY_SIZE + 2, false);
    settextcolor(COLOR_TITLE);
    setbkmode(TRANSPARENT);

    TCHAR info[64];
    if (ctx->currentLevel >= 0 && ctx->currentLevel < LEVEL_COUNT) {
        _stprintf_s(info, _T("关卡：%s"), getLevelName(ctx->currentLevel));
    } else {
        _tcscpy_s(info, _T("关卡：未知"));
    }
    int tw = textwidth(info);
    outtextxy((WIN_WIDTH - tw) / 2, 170, info);

    TCHAR timeBuf[16];
    formatTime(ctx->winTimeSec, timeBuf, 16);

    TCHAR thisRun[80];
    _stprintf_s(thisRun, _T("本次通关：步数 %d    时间 %s"), ctx->steps, timeBuf);
    tw = textwidth(thisRun);
    outtextxy((WIN_WIDTH - tw) / 2, 220, thisRun);

    const LevelRecord* rec = &ctx->levelRecords[ctx->currentLevel];
    TCHAR bestRun[80];
    if (rec->bestSteps > 0 && rec->bestTimeSec > 0) {
        TCHAR bestTimeBuf[16];
        formatTime(rec->bestTimeSec, bestTimeBuf, 16);
        _stprintf_s(bestRun, _T("本关最佳：步数 %d    时间 %s"), rec->bestSteps, bestTimeBuf);
    } else {
        _tcscpy_s(bestRun, _T("本关最佳：暂无"));
    }
    tw = textwidth(bestRun);
    outtextxy((WIN_WIDTH - tw) / 2, 265, bestRun);

    Button buttons[3];
    int count = 0;
    buildWinButtons(buttons, &count, ctx->currentLevel);
    drawButtons(buttons, count);
}

/***********************************************************
		ButtonId handleMenuClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测主菜单按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleMenuClick(int mx, int my)
{
    Button buttons[4];
    int count = 0;
    buildMenuButtons(buttons, &count);
    return checkButtons(buttons, count, mx, my);
}

/***********************************************************
		ButtonId handleSelectClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测选关页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleSelectClick(int mx, int my)
{
    Button buttons[4];
    int count = 0;
    buildSelectButtons(buttons, &count);
    return checkButtons(buttons, count, mx, my);
}

/***********************************************************
		ButtonId handleRulesClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测规则页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleRulesClick(int mx, int my)
{
    Button buttons[1];
    int count = 0;
    buildRulesButtons(buttons, &count);
    return checkButtons(buttons, count, mx, my);
}

/***********************************************************
		ButtonId handleLeaderboardClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测排行榜页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleLeaderboardClick(int mx, int my)
{
    Button buttons[3];
    int count = 0;
    buildLeaderboardButtons(buttons, &count);
    return checkButtons(buttons, count, mx, my);
}

/***********************************************************
		ButtonId handlePlayingClick(int mx, int my)
		参数：mx/my 鼠标坐标
		功能：检测游戏页工具栏按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handlePlayingClick(int mx, int my)
{
    Button buttons[5];
    int count = 0;
    buildPlayingButtons(buttons, &count);
    return checkButtons(buttons, count, mx, my);
}

/***********************************************************
		ButtonId handleWinClick(const GameContext* ctx, int mx, int my)
		参数：ctx 游戏上下文；mx/my 鼠标坐标
		功能：检测通关页按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleWinClick(const GameContext* ctx, int mx, int my)
{
    Button buttons[3];
    int count = 0;
    buildWinButtons(buttons, &count, ctx->currentLevel);
    return checkButtons(buttons, count, mx, my);
}

static TCHAR s_sfxToggleLabels[4][16];

/***********************************************************
		static void buildSoundToggleButtons(Button* buttons, int* count,
		                                    const GameContext* ctx)
		参数：buttons 输出按钮数组；count 输出数量；ctx 游戏上下文
		功能：构建右上角 2×2 音效开关按钮
		返回：无
***********************************************************/
static void buildSoundToggleButtons(Button* buttons, int* count,
                                    const GameContext* ctx)
{
    int x1 = WIN_WIDTH - SFX_TOGGLE_MARGIN - SFX_TOGGLE_W;
    int x0 = x1 - SFX_TOGGLE_GAP - SFX_TOGGLE_W;
    int y0 = SFX_TOGGLE_MARGIN;
    int y1 = y0 + SFX_TOGGLE_H + SFX_TOGGLE_GAP;

    _stprintf_s(s_sfxToggleLabels[0], _T("BGM:%s"),
                ctx->sfxBgmOn ? _T("开") : _T("关"));
    _stprintf_s(s_sfxToggleLabels[1], _T("移动:%s"),
                ctx->sfxMoveOn ? _T("开") : _T("关"));
    _stprintf_s(s_sfxToggleLabels[2], _T("按键:%s"),
                ctx->sfxButtonOn ? _T("开") : _T("关"));
    _stprintf_s(s_sfxToggleLabels[3], _T("通关:%s"),
                ctx->sfxWinOn ? _T("开") : _T("关"));

    buttons[0] = { x0, y0, SFX_TOGGLE_W, SFX_TOGGLE_H,
                   s_sfxToggleLabels[0], BTN_SFX_BGM };
    buttons[1] = { x1, y0, SFX_TOGGLE_W, SFX_TOGGLE_H,
                   s_sfxToggleLabels[1], BTN_SFX_MOVE };
    buttons[2] = { x0, y1, SFX_TOGGLE_W, SFX_TOGGLE_H,
                   s_sfxToggleLabels[2], BTN_SFX_BUTTON };
    buttons[3] = { x1, y1, SFX_TOGGLE_W, SFX_TOGGLE_H,
                   s_sfxToggleLabels[3], BTN_SFX_WIN };
    *count = 4;
}

/***********************************************************
		void drawSoundToggleBar(const GameContext* ctx)
		参数：ctx 游戏上下文
		功能：各页右上角绘制 2×2 音效开关按钮
		返回：无
***********************************************************/
void drawSoundToggleBar(const GameContext* ctx)
{
    Button buttons[4];
    int count = 0;
    buildSoundToggleButtons(buttons, &count, ctx);
    drawButtons(buttons, count);
}

/***********************************************************
		ButtonId handleSoundToggleClick(const GameContext* ctx, int mx, int my)
		参数：ctx 游戏上下文；mx/my 鼠标坐标
		功能：检测右上角音效开关按钮点击
		返回：ButtonId 或 BTN_NONE
***********************************************************/
ButtonId handleSoundToggleClick(const GameContext* ctx, int mx, int my)
{
    Button buttons[4];
    int count = 0;
    buildSoundToggleButtons(buttons, &count, ctx);
    return checkButtons(buttons, count, mx, my);
}

/***********************************************************
		void toggleSoundSetting(GameContext* ctx, ButtonId btn)
		参数：ctx 游戏上下文；btn 被点击的音效开关按钮
		功能：切换 BGM/移动/按键/通关开关；BGM 联动 audioUpdateBgm
		返回：无
***********************************************************/
void toggleSoundSetting(GameContext* ctx, ButtonId btn)
{
    switch (btn) {
    case BTN_SFX_BGM:
        ctx->sfxBgmOn = !ctx->sfxBgmOn;
        audioUpdateBgm(ctx->sfxBgmOn != 0);  // BGM 开关即时启停播放
        break;
    case BTN_SFX_MOVE:
        ctx->sfxMoveOn = !ctx->sfxMoveOn;
        break;
    case BTN_SFX_BUTTON:
        ctx->sfxButtonOn = !ctx->sfxButtonOn;
        break;
    case BTN_SFX_WIN:
        ctx->sfxWinOn = !ctx->sfxWinOn;
        break;
    default:
        break;
    }
}
