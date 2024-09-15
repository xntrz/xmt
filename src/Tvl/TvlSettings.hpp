#pragma once


enum TVLRUNMODE
{
    TVLRUNMODE_RST = 0,
    TVLRUNMODE_KEEPALIVE,
};


struct TvlSettings_t
{
    char    TargetId[256];
    int32   Viewers;
    int32   RunMode;
    bool    Test;
};

extern TvlSettings_t TvlSettings;


void TvlSettingsInitialize(void);
void TvlSettingsTerminate(void);