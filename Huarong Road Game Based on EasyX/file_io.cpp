/*
  file_io.cpp - 通关记录与排行榜文件读写实现

  record.txt 行格式：level_id=N,best_steps=S,best_time=T,completed=C
  leaderboard.txt 行格式：level_id=N,steps=S,time=T,name=玩家姓名
  savegame.txt 行格式：level_id=N,active=A,steps=S,time=T,pos=x0,y0,...
  文件均位于 exe 同目录；打开失败时 load* 返回 false 并使用内存默认值
 */
#include "file_io.h"
#include "level.h"
#include "game.h"
#include "hint.h"
#include "ui.h"
#include <Windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <tchar.h>

/***********************************************************
		void getExeDirPath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取当前 exe 所在目录（含末尾反斜杠）
		返回：无
***********************************************************/
void getExeDirPath(TCHAR* path, int pathLen)
{
    GetModuleFileName(NULL, path, pathLen);
    TCHAR* slash = _tcsrchr(path, _T('\\'));
    if (slash != NULL) {
        *(slash + 1) = _T('\0');
    }
}

/***********************************************************
		void getRecordFilePath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取 record.txt 完整路径（exe 同目录）
		返回：无
***********************************************************/
void getRecordFilePath(TCHAR* path, int pathLen)
{
    getExeDirPath(path, pathLen);
    _tcscat_s(path, pathLen, _T("record.txt"));
}

/***********************************************************
		void getLeaderboardFilePath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取 leaderboard.txt 完整路径
		返回：无
***********************************************************/
void getLeaderboardFilePath(TCHAR* path, int pathLen)
{
    getExeDirPath(path, pathLen);
    _tcscat_s(path, pathLen, _T("leaderboard.txt"));
}

/***********************************************************
		void getSaveGameFilePath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取 savegame.txt 完整路径（exe 同目录）
		返回：无
***********************************************************/
void getSaveGameFilePath(TCHAR* path, int pathLen)
{
    getExeDirPath(path, pathLen);
    _tcscat_s(path, pathLen, _T("savegame.txt"));
}

/* 单关存档槽：是否有效、步数、已用秒数、各方块左上角坐标 */
typedef struct {
    int active;
    int steps;
    int timeSec;
    int blockX[BLOCK_COUNT];
    int blockY[BLOCK_COUNT];
} SaveSlot;

/***********************************************************
		static void initDefaultSaveSlots(SaveSlot slots[LEVEL_COUNT])
		参数：slots 输出三关存档槽数组
		功能：将全部关卡槽初始化为未激活空档
		返回：无
***********************************************************/
static void initDefaultSaveSlots(SaveSlot slots[LEVEL_COUNT])
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        slots[i].active = 0;
        slots[i].steps = 0;
        slots[i].timeSec = 0;
        for (int j = 0; j < BLOCK_COUNT; j++) {
            slots[i].blockX[j] = 0;
            slots[i].blockY[j] = 0;
        }
    }
}

/***********************************************************
		static bool parseSaveLine(const char* line, SaveSlot* slot, int* levelId)
		参数：line 文本行；slot 输出存档槽；levelId 输出关卡 id
		功能：解析 level_id=...,active=...,steps=...,time=...,pos=x0,y0,... 行
		返回：解析成功返回 true，否则 false
***********************************************************/
static bool parseSaveLine(const char* line, SaveSlot* slot, int* levelId)
{
    int id = -1;
    int active = 0;
    int steps = 0;
    int timeSec = 0;

    if (sscanf_s(line, "level_id=%d,active=%d,steps=%d,time=%d",
        &id, &active, &steps, &timeSec) < 4) {
        return false;
    }
    if (id < 0 || id >= LEVEL_COUNT || levelId == NULL || slot == NULL) {
        return false;
    }

    const char* posPart = strstr(line, "pos=");
    if (posPart == NULL) {
        return false;
    }
    posPart += 4;

    // pos= 后逗号分隔，须恰好解析出 BLOCK_COUNT*2 个整数（各方块 x,y）
    int values[BLOCK_COUNT * 2];
    int count = 0;
    const char* p = posPart;
    while (count < BLOCK_COUNT * 2 && *p != '\0' && *p != '\n' && *p != '\r') {
        char* end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) {
            break;
        }
        values[count++] = (int)v;
        p = end;
        if (*p == ',') {
            p++;
        }
    }
    if (count != BLOCK_COUNT * 2) {
        return false;
    }

    slot->active = active;
    slot->steps = steps;
    slot->timeSec = timeSec;
    for (int i = 0; i < BLOCK_COUNT; i++) {
        slot->blockX[i] = values[i * 2];
        slot->blockY[i] = values[i * 2 + 1];
    }
    *levelId = id;
    return true;
}

/***********************************************************
		static bool validateSaveSlot(int level, const SaveSlot* slot)
		参数：level 关卡索引；slot 待校验存档槽
		功能：校验槽已激活且各方块坐标不越界（用关卡模板 w/h）
		返回：合法返回 true，否则 false
***********************************************************/
static bool validateSaveSlot(int level, const SaveSlot* slot)
{
    Block templateBlocks[BLOCK_COUNT];

    // inactive 或空指针直接拒绝，防止脏存档污染局面
    if (slot == NULL || !slot->active) {
        return false;
    }
    if (level < 0 || level >= LEVEL_COUNT) {
        return false;
    }

    loadLevel(level, templateBlocks);
    for (int i = 0; i < BLOCK_COUNT; i++) {
        int x = slot->blockX[i];
        int y = slot->blockY[i];
        int w = templateBlocks[i].w;
        int h = templateBlocks[i].h;
        if (x < 0 || y < 0 || x + w > BOARD_COLS || y + h > BOARD_ROWS) {
            return false;
        }
    }
    return true;
}

/***********************************************************
		static void loadSaveGameSlots(SaveSlot slots[LEVEL_COUNT])
		参数：slots 输出三关存档槽
		功能：从 savegame.txt 加载；文件不存在则保持默认空槽
		返回：无
***********************************************************/
static void loadSaveGameSlots(SaveSlot slots[LEVEL_COUNT])
{
    initDefaultSaveSlots(slots);

    TCHAR path[MAX_PATH];
    getSaveGameFilePath(path, MAX_PATH);

    FILE* fp = NULL;
    if (_tfopen_s(&fp, path, _T("r")) != 0 || fp == NULL) {
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        SaveSlot slot;
        int levelId = -1;
        if (parseSaveLine(line, &slot, &levelId)) {
            slots[levelId] = slot;
        }
    }

    fclose(fp);
}

/***********************************************************
		static bool writeSaveGameSlots(const SaveSlot slots[LEVEL_COUNT])
		参数：slots 三关存档槽
		功能：将全部关卡槽写入 savegame.txt（覆盖写）
		返回：成功 true，打开文件失败 false
***********************************************************/
static bool writeSaveGameSlots(const SaveSlot slots[LEVEL_COUNT])
{
    TCHAR path[MAX_PATH];
    getSaveGameFilePath(path, MAX_PATH);

    FILE* fp = NULL;
    if (_tfopen_s(&fp, path, _T("w")) != 0 || fp == NULL) {
        return false;
    }

    for (int i = 0; i < LEVEL_COUNT; i++) {
        fprintf(fp, "level_id=%d,active=%d,steps=%d,time=%d,pos=",
            i, slots[i].active, slots[i].steps, slots[i].timeSec);
        for (int j = 0; j < BLOCK_COUNT; j++) {
            fprintf(fp, "%d,%d", slots[i].blockX[j], slots[i].blockY[j]);
            if (j + 1 < BLOCK_COUNT) {
                fprintf(fp, ",");
            }
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return true;
}

/***********************************************************
		void saveGameProgress(int level, int steps, int timeSec,
		                      const Block blocks[BLOCK_COUNT])
		参数：level 关卡；steps 步数；timeSec 已用秒数；blocks 当前方块布局
		功能：退出关卡时读-改-写 savegame.txt，保存该关进度
		返回：无
***********************************************************/
void saveGameProgress(int level, int steps, int timeSec,
                      const Block blocks[BLOCK_COUNT])
{
    SaveSlot slots[LEVEL_COUNT];

    if (level < 0 || level >= LEVEL_COUNT || blocks == NULL) {
        return;
    }

    // 读全文件再写回，保留其他关卡槽不变
    loadSaveGameSlots(slots);
    slots[level].active = 1;
    slots[level].steps = steps;
    slots[level].timeSec = timeSec;
    for (int i = 0; i < BLOCK_COUNT; i++) {
        slots[level].blockX[i] = blocks[i].x;
        slots[level].blockY[i] = blocks[i].y;
    }
    writeSaveGameSlots(slots);
}

/***********************************************************
		void clearGameSave(int level)
		参数：level 关卡索引
		功能：通关或重玩时清除该关存档槽
		返回：无
***********************************************************/
void clearGameSave(int level)
{
    SaveSlot slots[LEVEL_COUNT];

    if (level < 0 || level >= LEVEL_COUNT) {
        return;
    }

    loadSaveGameSlots(slots);
    slots[level].active = 0;
    slots[level].steps = 0;
    slots[level].timeSec = 0;
    writeSaveGameSlots(slots);
}

/***********************************************************
		bool tryRestoreGameProgress(int level, GameContext* ctx)
		参数：level 关卡索引；ctx 游戏上下文
		功能：进关时尝试恢复存档；成功则回填 blocks、步数与计时起点
		返回：恢复成功返回 true，无有效存档返回 false
***********************************************************/
bool tryRestoreGameProgress(int level, GameContext* ctx)
{
    SaveSlot slots[LEVEL_COUNT];

    if (ctx == NULL || level < 0 || level >= LEVEL_COUNT) {
        return false;
    }

    loadSaveGameSlots(slots);
    if (!validateSaveSlot(level, &slots[level])) {
        return false;
    }

    // 先载入关卡模板（标签/颜色），再覆盖坐标
    loadLevel(level, ctx->blocks);
    for (int i = 0; i < BLOCK_COUNT; i++) {
        ctx->blocks[i].x = slots[level].blockX[i];
        ctx->blocks[i].y = slots[level].blockY[i];
    }

    ctx->currentLevel = level;
    ctx->steps = slots[level].steps;
    ctx->winTimeSec = -1;
    ctx->pendingWin = 0;
    ctx->winShowUntil = 0;
    // 反推 levelStartTick，使 UI 计时从存档已用时长继续
    ctx->levelStartTick = GetTickCount() - (DWORD)slots[level].timeSec * 1000;
    undoStackInit(&ctx->undo);
    clearHint(ctx);
    resetPlayingState(ctx);
    return true;
}

/***********************************************************
		void formatTime(int sec, TCHAR* buf, int bufLen)
		参数：sec 秒数；buf 输出缓冲；bufLen 缓冲长度
		功能：格式化为 MM:SS 字符串
		返回：无
***********************************************************/
void formatTime(int sec, TCHAR* buf, int bufLen)
{
    if (sec < 0) {
        _tcscpy_s(buf, bufLen, _T("--:--"));
        return;
    }
    _stprintf_s(buf, bufLen, _T("%02d:%02d"), sec / 60, sec % 60);
}

/***********************************************************
		static void initDefaultRecords(LevelRecord records[LEVEL_COUNT])
		参数：records 输出记录数组
		功能：将各关记录初始化为未通关默认值
		返回：无
***********************************************************/
static void initDefaultRecords(LevelRecord records[LEVEL_COUNT])
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        records[i].completed = 0;
        records[i].bestSteps = 0;
        records[i].bestTimeSec = 0;
    }
}

/***********************************************************
		static void initDefaultLeaderboards(LevelLeaderboard boards[LEVEL_COUNT])
		参数：boards 输出排行榜数组
		功能：将各关排行榜 count 初始化为 0
		返回：无
***********************************************************/
static void initDefaultLeaderboards(LevelLeaderboard boards[LEVEL_COUNT])
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        boards[i].count = 0;
        for (int j = 0; j < LEADERBOARD_MAX; j++) {
            boards[i].entries[j].name[0] = _T('\0');
        }
    }
}

/***********************************************************
		static void copyPlayerName(TCHAR dest[PLAYER_NAME_MAX],
		                           const TCHAR* playerName)
		参数：dest 输出姓名缓冲；playerName 输入姓名
		功能：复制玩家姓名，空或 NULL 时写入「匿名」
		返回：无
***********************************************************/
static void copyPlayerName(TCHAR dest[PLAYER_NAME_MAX], const TCHAR* playerName)
{
    if (playerName == NULL || playerName[0] == _T('\0')) {
        _tcscpy_s(dest, PLAYER_NAME_MAX, _T("匿名"));
    } else {
        _tcscpy_s(dest, PLAYER_NAME_MAX, playerName);
    }
}

/***********************************************************
		static void trimLineEnding(TCHAR* text)
		参数：text 待处理的字符串
		功能：去掉末尾换行符
		返回：无
***********************************************************/
static void trimLineEnding(TCHAR* text)
{
    if (text == NULL) {
        return;
    }
    size_t len = _tcslen(text);
    while (len > 0 && (text[len - 1] == _T('\n') || text[len - 1] == _T('\r'))) {
        text[len - 1] = _T('\0');
        len--;
    }
}

/***********************************************************
		static int compareLeaderboardEntry(const LeaderboardEntry* a,
		                                   const LeaderboardEntry* b)
		参数：a/b 待比较的两条成绩
		功能：比较排行榜条目优劣（步数少优先，相同则用时短优先）
		返回：a 更优返回负值，相同返回 0，b 更优返回正值
***********************************************************/
static int compareLeaderboardEntry(const LeaderboardEntry* a,
                                   const LeaderboardEntry* b)
{
    if (a->steps != b->steps) {
        return a->steps - b->steps;
    }
    return a->timeSec - b->timeSec;
}

/***********************************************************
		static void sortLeaderboard(LevelLeaderboard* board)
		参数：board 排行榜
		功能：对 entries 按成绩从优到劣冒泡排序
		返回：无
***********************************************************/
static void sortLeaderboard(LevelLeaderboard* board)
{
    for (int i = 0; i < board->count; i++) {
        for (int j = i + 1; j < board->count; j++) {
            if (compareLeaderboardEntry(&board->entries[i],
                                        &board->entries[j]) > 0) {
                LeaderboardEntry tmp = board->entries[i];
                board->entries[i] = board->entries[j];
                board->entries[j] = tmp;
            }
        }
    }
}

/***********************************************************
		bool loadRecords(LevelRecord records[LEVEL_COUNT])
		参数：records 输出每关记录
		功能：从 record.txt 加载通关记录
		返回：成功 true；文件不存在 false（使用默认值）
***********************************************************/
bool loadRecords(LevelRecord records[LEVEL_COUNT])
{
    initDefaultRecords(records);

    TCHAR path[MAX_PATH];
    getRecordFilePath(path, MAX_PATH);

    FILE* fp = NULL;
    if (_tfopen_s(&fp, path, _T("r")) != 0 || fp == NULL) {
        return false;
    }

    char line[128];
    while (fgets(line, sizeof(line), fp) != NULL) {
        int levelId = -1;
        int bestSteps = 0;
        int bestTime = 0;
        int completed = 0;
        if (sscanf_s(line, "level_id=%d,best_steps=%d,best_time=%d,completed=%d",
            &levelId, &bestSteps, &bestTime, &completed) == 4) {
            if (levelId >= 0 && levelId < LEVEL_COUNT) {
                records[levelId].completed = completed;
                records[levelId].bestSteps = bestSteps;
                records[levelId].bestTimeSec = bestTime;
            }
        }
    }

    fclose(fp);
    return true;
}

/***********************************************************
		bool saveRecords(const LevelRecord records[LEVEL_COUNT])
		参数：records 待保存记录
		功能：写入 record.txt
		返回：成功 true，失败 false
***********************************************************/
bool saveRecords(const LevelRecord records[LEVEL_COUNT])
{
    TCHAR path[MAX_PATH];
    getRecordFilePath(path, MAX_PATH);

    FILE* fp = NULL;
    if (_tfopen_s(&fp, path, _T("w")) != 0 || fp == NULL) {
        return false;
    }

    for (int i = 0; i < LEVEL_COUNT; i++) {
        fprintf(fp, "level_id=%d,best_steps=%d,best_time=%d,completed=%d\n",
            i,
            records[i].bestSteps,
            records[i].bestTimeSec,
            records[i].completed);
    }

    fclose(fp);
    return true;
}

/***********************************************************
		void updateRecordOnWin(int level, int steps, int timeSec,
		                       LevelRecord records[LEVEL_COUNT])
		参数：level 关卡；steps 步数；timeSec 用时；records 记录数组
		功能：通关后更新本关最佳步数与用时
		返回：无
***********************************************************/
void updateRecordOnWin(int level, int steps, int timeSec,
                       LevelRecord records[LEVEL_COUNT])
{
    if (level < 0 || level >= LEVEL_COUNT) {
        return;
    }

    LevelRecord* rec = &records[level];
    rec->completed = 1;

    if (rec->bestSteps == 0 || steps < rec->bestSteps) {
        rec->bestSteps = steps;
    }
    if (rec->bestTimeSec == 0 || timeSec < rec->bestTimeSec) {
        rec->bestTimeSec = timeSec;
    }
}

/***********************************************************
		bool loadLeaderboard(LevelLeaderboard boards[LEVEL_COUNT])
		参数：boards 输出每关排行榜
		功能：从 leaderboard.txt 加载并排序
		返回：成功 true；文件不存在 false
***********************************************************/
bool loadLeaderboard(LevelLeaderboard boards[LEVEL_COUNT])
{
    initDefaultLeaderboards(boards);

    TCHAR path[MAX_PATH];
    getLeaderboardFilePath(path, MAX_PATH);

    FILE* fp = NULL;
    if (_tfopen_s(&fp, path, _T("r")) != 0 || fp == NULL) {
        return false;
    }
    _setmode(_fileno(fp), _O_U8TEXT);

    TCHAR line[256];
    while (_fgetts(line, 256, fp) != NULL) {
        int levelId = -1;
        int steps = 0;
        int timeSec = 0;
        if (_stscanf_s(line, _T("level_id=%d,steps=%d,time=%d"),
            &levelId, &steps, &timeSec) < 3) {
            continue;
        }
        if (levelId < 0 || levelId >= LEVEL_COUNT) {
            continue;
        }

        LevelLeaderboard* board = &boards[levelId];
        if (board->count >= LEADERBOARD_MAX) {
            continue;
        }

        LeaderboardEntry* entry = &board->entries[board->count];
        entry->steps = steps;
        entry->timeSec = timeSec;

        TCHAR* namePart = _tcsstr(line, _T(",name="));
        if (namePart != NULL) {
            _tcscpy_s(entry->name, PLAYER_NAME_MAX, namePart + 6);
            trimLineEnding(entry->name);
        } else {
            copyPlayerName(entry->name, NULL);
        }
        board->count++;
    }

    fclose(fp);

    for (int i = 0; i < LEVEL_COUNT; i++) {
        sortLeaderboard(&boards[i]);
        if (boards[i].count > LEADERBOARD_MAX) {
            boards[i].count = LEADERBOARD_MAX;
        }
    }

    return true;
}

/***********************************************************
		bool saveLeaderboard(const LevelLeaderboard boards[LEVEL_COUNT])
		参数：boards 排行榜数据
		功能：写入 leaderboard.txt
		返回：成功 true，失败 false
***********************************************************/
bool saveLeaderboard(const LevelLeaderboard boards[LEVEL_COUNT])
{
    TCHAR path[MAX_PATH];
    getLeaderboardFilePath(path, MAX_PATH);

    FILE* fp = NULL;
    if (_tfopen_s(&fp, path, _T("w")) != 0 || fp == NULL) {
        return false;
    }
    _setmode(_fileno(fp), _O_U8TEXT);

    for (int level = 0; level < LEVEL_COUNT; level++) {
        const LevelLeaderboard* board = &boards[level];
        for (int i = 0; i < board->count; i++) {
            _ftprintf(fp, _T("level_id=%d,steps=%d,time=%d,name=%s\n"),
                level,
                board->entries[i].steps,
                board->entries[i].timeSec,
                board->entries[i].name);
        }
    }

    fclose(fp);
    return true;
}

/***********************************************************
		void addLeaderboardEntry(int level, int steps, int timeSec,
		                         LevelLeaderboard boards[LEVEL_COUNT])
		参数：level 关卡；steps 步数；timeSec 用时；boards 排行榜数组
		功能：插入新成绩，排序并保留前 10 名
		返回：无
***********************************************************/
void addLeaderboardEntry(int level, int steps, int timeSec,
                         const TCHAR* playerName,
                         LevelLeaderboard boards[LEVEL_COUNT])
{
    if (level < 0 || level >= LEVEL_COUNT) {
        return;
    }

    LevelLeaderboard* board = &boards[level];
    LeaderboardEntry all[LEADERBOARD_MAX + 1];
    int n = board->count;
    if (n > LEADERBOARD_MAX) {
        n = LEADERBOARD_MAX;
    }

    for (int i = 0; i < n; i++) {
        all[i] = board->entries[i];
    }
    all[n].steps = steps;
    all[n].timeSec = timeSec;
    copyPlayerName(all[n].name, playerName);
    n++;

    LevelLeaderboard temp;
    temp.count = n;
    for (int i = 0; i < n; i++) {
        temp.entries[i] = all[i];
    }
    sortLeaderboard(&temp);

    board->count = n > LEADERBOARD_MAX ? LEADERBOARD_MAX : n;
    for (int i = 0; i < board->count; i++) {
        board->entries[i] = temp.entries[i];
    }
}

/***********************************************************
		const LeaderboardEntry* getLeaderboardTop(int level,
		    const LevelLeaderboard boards[LEVEL_COUNT], int* outCount)
		参数：level 关卡；boards 排行榜；outCount 输出条数
		功能：获取指定关卡已排序的排行榜条目
		返回：条目数组指针；无记录返回 NULL
***********************************************************/
const LeaderboardEntry* getLeaderboardTop(int level,
    const LevelLeaderboard boards[LEVEL_COUNT], int* outCount)
{
    if (level < 0 || level >= LEVEL_COUNT || outCount == NULL) {
        if (outCount) {
            *outCount = 0;
        }
        return NULL;
    }

    *outCount = boards[level].count;
    if (boards[level].count <= 0) {
        return NULL;
    }
    return boards[level].entries;
}

/***********************************************************
		void seedLeaderboardFromRecords(LevelLeaderboard boards[LEVEL_COUNT],
		                                const LevelRecord records[LEVEL_COUNT])
		参数：boards 排行榜；records 通关记录
		功能：排行榜为空时从旧 record 迁移一条初始数据
		返回：无
***********************************************************/
void seedLeaderboardFromRecords(LevelLeaderboard boards[LEVEL_COUNT],
                                const LevelRecord records[LEVEL_COUNT])
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        if (boards[i].count == 0 && records[i].bestSteps > 0) {
            boards[i].entries[0].steps = records[i].bestSteps;
            boards[i].entries[0].timeSec = records[i].bestTimeSec;
            copyPlayerName(boards[i].entries[0].name, NULL);
            boards[i].count = 1;
        }
    }
}
