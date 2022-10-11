#pragma once

struct AppSettings_t
{
    char ModCurSel[256];
};

extern AppSettings_t AppSettings;

void AppSettingsInitialize(void);
void AppSettingsTerminate(void);