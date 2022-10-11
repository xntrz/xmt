#pragma once

struct PrxSettings_t
{
    int32 Type;
    int32 Retry;
    int32 Workpool;
    uint32 Timeout;
    char TargetHost[256];
    uint16 TargetPort;
    bool FlagRequest;
    char UserAgent[256];
};

extern PrxSettings_t PrxSettings;


void PrxSettingsInitialize(void);
void PrxSettingsTerminate(void);