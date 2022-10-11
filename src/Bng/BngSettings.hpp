#pragma once

struct BngSettings_t
{
    int32 Workpool;
    char TargetId[256];
    int32 Viewers;
    int32 ViewersMax;
    char Host[256];
    bool ProxyUseMode;
    int32 ProxyType;
    int32 ProxyRetryMax;
};

extern BngSettings_t BngSettings;


void BngSettingsInitialize(void);
void BngSettingsTerminate(void);