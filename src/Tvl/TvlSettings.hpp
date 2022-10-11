#pragma once

enum TvlBotType_t
{
    TvlBotType_Stream = 0,
    TvlBotType_Clip,
    TvlBotType_Vod,
};

enum TvlRunMode_t
{
    TvlRunMode_Keepalive = 0,
    TvlRunMode_Rst,
    TvlRunMode_TimeoutTest,
    
    TvlRunModeNum,
};

struct TvlSettings_t
{
    int32 BotType;
    char TargetId[256];
    int32 Viewers;
    int32 ViewersMax;
    int32 RunMode;
    bool Test;
};

extern TvlSettings_t TvlSettings;


void TvlSettingsInitialize(void);
void TvlSettingsTerminate(void);