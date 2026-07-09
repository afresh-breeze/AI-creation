/*
  audio.cpp - 音效与背景音乐播放实现

  短音效经 MCI 四路别名池播放，避免连续触发时 close 打断上一声；
  BGM 优先 MCI repeat，失败则回退 PlaySound 异步循环。
 */
#include "audio.h"
#include "file_io.h"
#include <Windows.h>
#include <Mmsystem.h>
#include <tchar.h>

#pragma comment(lib, "winmm.lib")

static TCHAR s_movePath[MAX_PATH];
static TCHAR s_bgmPath[MAX_PATH];
static TCHAR s_buttonPath[MAX_PATH];
static TCHAR s_winPath[MAX_PATH];
static int s_bgmMciOpen = 0;      // MCI 通道 BGM 是否已打开
static int s_bgmPlaySound = 0;    // PlaySound 通道 BGM 是否在播
static int s_sfxPoolIdx = 0;      // 音效池轮转下标（0..SFX_POOL_SIZE-1）

#define SFX_POOL_SIZE 4           // MCI 短音效并发别名数量

/***********************************************************
		static void buildSfxPath(TCHAR* buf, int bufLen, const TCHAR* filename)
		参数：buf 输出缓冲；bufLen 缓冲长度；filename 音效文件名
		功能：拼接 exe 同目录下 sounds\filename 完整路径
		返回：无
***********************************************************/
static void buildSfxPath(TCHAR* buf, int bufLen, const TCHAR* filename)
{
    getExeDirPath(buf, bufLen);
    _tcscat_s(buf, bufLen, _T("sounds\\"));
    _tcscat_s(buf, bufLen, filename);
}

/***********************************************************
		static bool sfxFileExists(const TCHAR* path)
		参数：path 文件路径
		功能：判断音效文件是否存在
		返回：存在返回 true，否则 false
***********************************************************/
static bool sfxFileExists(const TCHAR* path)
{
    return GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
}

/***********************************************************
		static void playWav(const TCHAR* path)
		参数：path WAV 文件路径
		功能：经 MCI 播放一次性短音效
		返回：无
***********************************************************/
static void playWav(const TCHAR* path)
{
    TCHAR alias[16];
    TCHAR cmd[512];

    if (!sfxFileExists(path)) {
        return;
    }

    // 轮转 sfx0..sfx3 别名，避免连续播放时 close 同一别名打断上一声
    _stprintf_s(alias, _countof(alias), _T("sfx%d"), s_sfxPoolIdx);
    s_sfxPoolIdx = (s_sfxPoolIdx + 1) % SFX_POOL_SIZE;

    _stprintf_s(cmd, _countof(cmd), _T("close %s"), alias);
    mciSendString(cmd, NULL, 0, NULL);

    // 优先 type waveaudio 打开，失败再退化为无 type（兼容不同 WAV）
    _tcscpy_s(cmd, _countof(cmd), _T("open \""));
    _tcscat_s(cmd, _countof(cmd), path);
    _tcscat_s(cmd, _countof(cmd), _T("\" type waveaudio alias "));
    _tcscat_s(cmd, _countof(cmd), alias);
    if (mciSendString(cmd, NULL, 0, NULL) != 0) {
        _tcscpy_s(cmd, _countof(cmd), _T("open \""));
        _tcscat_s(cmd, _countof(cmd), path);
        _tcscat_s(cmd, _countof(cmd), _T("\" alias "));
        _tcscat_s(cmd, _countof(cmd), alias);
        if (mciSendString(cmd, NULL, 0, NULL) != 0) {
            return;
        }
    }

    _stprintf_s(cmd, _countof(cmd), _T("play %s from 0"), alias);
    mciSendString(cmd, NULL, 0, NULL);
}

/***********************************************************
		static void stopSfxPool(void)
		功能：关闭全部 MCI 短音效别名并复位池下标
		返回：无
***********************************************************/
static void stopSfxPool(void)
{
    TCHAR cmd[64];
    for (int i = 0; i < SFX_POOL_SIZE; i++) {
        _stprintf_s(cmd, _countof(cmd), _T("close sfx%d"), i);
        mciSendString(cmd, NULL, 0, NULL);
    }
    s_sfxPoolIdx = 0;
}

/***********************************************************
		static void stopBgmMci(void)
		功能：停止并关闭 MCI 通道背景音乐
		返回：无
***********************************************************/
static void stopBgmMci(void)
{
    if (s_bgmMciOpen) {
        mciSendString(_T("stop bgm"), NULL, 0, NULL);
        mciSendString(_T("close bgm"), NULL, 0, NULL);
        s_bgmMciOpen = 0;
    }
}

/***********************************************************
		static void stopBgmPlaySound(void)
		功能：停止 PlaySound 通道背景音乐
		返回：无
***********************************************************/
static void stopBgmPlaySound(void)
{
    if (s_bgmPlaySound) {
        PlaySound(NULL, NULL, SND_PURGE);
        s_bgmPlaySound = 0;
    }
}

/***********************************************************
		static bool tryMciBgm(void)
		功能：尝试用 MCI 循环播放 BGM
		返回：成功返回 true，否则 false
***********************************************************/
static bool tryMciBgm(void)
{
    TCHAR cmd[512];

    _tcscpy_s(cmd, _countof(cmd), _T("open \""));
    _tcscat_s(cmd, _countof(cmd), s_bgmPath);
    _tcscat_s(cmd, _countof(cmd), _T("\" type waveaudio alias bgm"));
    if (mciSendString(cmd, NULL, 0, NULL) != 0) {
        _tcscpy_s(cmd, _countof(cmd), _T("open \""));
        _tcscat_s(cmd, _countof(cmd), s_bgmPath);
        _tcscat_s(cmd, _countof(cmd), _T("\" alias bgm"));
        if (mciSendString(cmd, NULL, 0, NULL) != 0) {
            return false;
        }
    }

    if (mciSendString(_T("play bgm repeat"), NULL, 0, NULL) != 0) {
        mciSendString(_T("close bgm"), NULL, 0, NULL);
        return false;
    }

    s_bgmMciOpen = 1;
    return true;
}

/***********************************************************
		static bool tryPlaySoundBgm(void)
		功能：MCI 失败时回退 PlaySound 异步循环播放 BGM
		返回：成功返回 true，否则 false
***********************************************************/
static bool tryPlaySoundBgm(void)
{
    if (!PlaySound(s_bgmPath, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP)) {
        return false;
    }
    s_bgmPlaySound = 1;
    return true;
}

/***********************************************************
		void audioInit(void)
		功能：初始化各音效文件路径与 BGM 状态标志
		返回：无
***********************************************************/
void audioInit(void)
{
    buildSfxPath(s_movePath, MAX_PATH, _T("move.wav"));
    buildSfxPath(s_bgmPath, MAX_PATH, _T("background.wav"));
    buildSfxPath(s_buttonPath, MAX_PATH, _T("bottom.wav"));
    buildSfxPath(s_winPath, MAX_PATH, _T("win.wav"));
    s_bgmMciOpen = 0;
    s_bgmPlaySound = 0;
}

/***********************************************************
		void audioShutdown(void)
		功能：退出前释放全部 MCI 与 PlaySound 资源
		返回：无
***********************************************************/
void audioShutdown(void)
{
    stopSfxPool();
    stopBgmMci();
    stopBgmPlaySound();
}

/***********************************************************
		void audioPlayMove(void)
		功能：播放方块移动音效
		返回：无
***********************************************************/
void audioPlayMove(void)
{
    playWav(s_movePath);
}

/***********************************************************
		void audioPlayButton(void)
		功能：播放按钮点击音效
		返回：无
***********************************************************/
void audioPlayButton(void)
{
    playWav(s_buttonPath);
}

/***********************************************************
		void audioPlayWin(void)
		功能：播放通关音效
		返回：无
***********************************************************/
void audioPlayWin(void)
{
    playWav(s_winPath);
}

/***********************************************************
		void audioUpdateBgm(bool enabled)
		参数：enabled 是否开启背景音乐
		功能：按开关启停 BGM；开启时先停旧通道，优先 MCI 再 PlaySound
		返回：无
***********************************************************/
void audioUpdateBgm(bool enabled)
{
    if (!enabled) {
        stopBgmMci();
        stopBgmPlaySound();
        return;
    }

    if (!sfxFileExists(s_bgmPath)) {
        return;
    }

    // 切换前先停两路 BGM，避免叠音
    stopBgmMci();
    stopBgmPlaySound();

    if (tryMciBgm()) {
        return;
    }
    tryPlaySoundBgm();
}
