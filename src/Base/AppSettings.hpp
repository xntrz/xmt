#pragma once

struct AppSettings_t
{
    char PathSslCert[256];
    char PathSslKey[256];
    char ModCurSel[256];
    int32 WindowW;
    int32 WindowH;
    int32 FontH;
    bool MultithreadProxySystem;
};

extern AppSettings_t AppSettings;

void AppSettingsInitialize(void);
void AppSettingsTerminate(void);