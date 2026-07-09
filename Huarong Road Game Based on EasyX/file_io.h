/*
  file_io.h - 通关记录、排行榜与存档文件读写

  对应文件：record.txt / leaderboard.txt / savegame.txt（均位于 exe 同目录）
 */
#ifndef FILE_IO_H
#define FILE_IO_H

#include "config.h"
#include "level.h"

/* 前向声明：完整定义在 ui.h，此处仅用于存档 API 参数 */
typedef struct GameContext GameContext;

/* 单关通关记录：是否已通关、历史最佳步数与用时（秒，0 表示暂无） */
typedef struct {
    int completed;
    int bestSteps;
    int bestTimeSec;
} LevelRecord;

/* 排行榜单条成绩：步数、用时（秒）、玩家姓名 */
typedef struct {
    int steps;
    int timeSec;
    TCHAR name[PLAYER_NAME_MAX];
} LeaderboardEntry;

/* 单关排行榜：最多 LEADERBOARD_MAX 条，count 为当前有效条数 */
typedef struct {
    LeaderboardEntry entries[LEADERBOARD_MAX];
    int count;
} LevelLeaderboard;

/***********************************************************
		void getExeDirPath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取当前 exe 所在目录（含末尾反斜杠）
		返回：无
***********************************************************/
void getExeDirPath(TCHAR* path, int pathLen);

/***********************************************************
		void getRecordFilePath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取 record.txt 完整路径（exe 同目录）
		返回：无
***********************************************************/
void getRecordFilePath(TCHAR* path, int pathLen);

/***********************************************************
		void getLeaderboardFilePath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取 leaderboard.txt 完整路径
		返回：无
***********************************************************/
void getLeaderboardFilePath(TCHAR* path, int pathLen);

/***********************************************************
		bool loadRecords(LevelRecord records[LEVEL_COUNT])
		参数：records 输出每关记录
		功能：从 record.txt 加载通关记录
		返回：成功 true；文件不存在 false（使用默认值）
***********************************************************/
bool loadRecords(LevelRecord records[LEVEL_COUNT]);

/***********************************************************
		bool saveRecords(const LevelRecord records[LEVEL_COUNT])
		参数：records 待保存记录
		功能：写入 record.txt
		返回：成功 true，失败 false
***********************************************************/
bool saveRecords(const LevelRecord records[LEVEL_COUNT]);

/***********************************************************
		void updateRecordOnWin(int level, int steps, int timeSec,
		                       LevelRecord records[LEVEL_COUNT])
		参数：level 关卡；steps 步数；timeSec 用时；records 记录数组
		功能：通关后更新本关最佳步数与用时
		返回：无
***********************************************************/
void updateRecordOnWin(int level, int steps, int timeSec,
                       LevelRecord records[LEVEL_COUNT]);

/***********************************************************
		bool loadLeaderboard(LevelLeaderboard boards[LEVEL_COUNT])
		参数：boards 输出每关排行榜
		功能：从 leaderboard.txt 加载并排序
		返回：成功 true；文件不存在 false
***********************************************************/
bool loadLeaderboard(LevelLeaderboard boards[LEVEL_COUNT]);

/***********************************************************
		bool saveLeaderboard(const LevelLeaderboard boards[LEVEL_COUNT])
		参数：boards 排行榜数据
		功能：写入 leaderboard.txt
		返回：成功 true，失败 false
***********************************************************/
bool saveLeaderboard(const LevelLeaderboard boards[LEVEL_COUNT]);

/***********************************************************
		void addLeaderboardEntry(int level, int steps, int timeSec,
		                         const TCHAR* playerName,
		                         LevelLeaderboard boards[LEVEL_COUNT])
		参数：level 关卡；steps 步数；timeSec 用时；playerName 玩家姓名；boards 排行榜数组
		功能：插入新成绩（姓名为空则记为「匿名」），排序并保留前 10 名
		返回：无
***********************************************************/
void addLeaderboardEntry(int level, int steps, int timeSec,
                         const TCHAR* playerName,
                         LevelLeaderboard boards[LEVEL_COUNT]);

/***********************************************************
		const LeaderboardEntry* getLeaderboardTop(int level,
		    const LevelLeaderboard boards[LEVEL_COUNT], int* outCount)
		参数：level 关卡；boards 排行榜；outCount 输出条数
		功能：获取指定关卡已排序的排行榜条目
		返回：条目数组指针；无记录返回 NULL
***********************************************************/
const LeaderboardEntry* getLeaderboardTop(int level,
    const LevelLeaderboard boards[LEVEL_COUNT], int* outCount);

/***********************************************************
		void seedLeaderboardFromRecords(LevelLeaderboard boards[LEVEL_COUNT],
		                                const LevelRecord records[LEVEL_COUNT])
		参数：boards 排行榜；records 通关记录
		功能：排行榜为空时从旧 record 迁移一条初始数据
		返回：无
***********************************************************/
void seedLeaderboardFromRecords(LevelLeaderboard boards[LEVEL_COUNT],
                                const LevelRecord records[LEVEL_COUNT]);

/***********************************************************
		void formatTime(int sec, TCHAR* buf, int bufLen)
		参数：sec 秒数；buf 输出缓冲；bufLen 缓冲长度
		功能：格式化为 MM:SS 字符串
		返回：无
***********************************************************/
void formatTime(int sec, TCHAR* buf, int bufLen);

/***********************************************************
		void getSaveGameFilePath(TCHAR* path, int pathLen)
		参数：path 输出缓冲区；pathLen 缓冲区长度
		功能：获取 savegame.txt 完整路径（exe 同目录）
		返回：无
***********************************************************/
void getSaveGameFilePath(TCHAR* path, int pathLen);

/***********************************************************
		void saveGameProgress(int level, int steps, int timeSec,
		                      const Block blocks[BLOCK_COUNT])
		参数：level 关卡；steps 步数；timeSec 已用秒数；blocks 当前方块布局
		功能：退出关卡时读-改-写 savegame.txt，保存该关进度
		返回：无
***********************************************************/
void saveGameProgress(int level, int steps, int timeSec,
                      const Block blocks[BLOCK_COUNT]);

/***********************************************************
		void clearGameSave(int level)
		参数：level 关卡索引
		功能：通关或重玩时清除该关存档槽
		返回：无
***********************************************************/
void clearGameSave(int level);

/***********************************************************
		bool tryRestoreGameProgress(int level, GameContext* ctx)
		参数：level 关卡索引；ctx 游戏上下文
		功能：进关时尝试恢复存档；成功则回填 blocks、步数与计时起点
		返回：恢复成功返回 true，无有效存档返回 false
***********************************************************/
bool tryRestoreGameProgress(int level, GameContext* ctx);

#endif
