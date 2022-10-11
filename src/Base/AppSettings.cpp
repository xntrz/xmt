#include "AppSettings.hpp"

#include "Shared/Common/Registry.hpp"


/*extern*/ AppSettings_t AppSettings
{
    /* ModCurSel[256]            */  "",
};


static void AppSettingsLoad(void)
{
    HOBJ hVar = RegVarFind("app_cursel");
    if (hVar)
        RegVarReadString(hVar, AppSettings.ModCurSel, COUNT_OF(AppSettings.ModCurSel));
};


static void AppSettingsSave(void)
{
    HOBJ hVar = RegVarFind("app_cursel");
    if (hVar)
        RegVarSetValue(hVar, AppSettings.ModCurSel);
};


void AppSettingsInitialize(void)
{
    RegResetRegist(AppSettingsLoad);
    AppSettingsLoad();
};


void AppSettingsTerminate(void)
{
    AppSettingsSave();
};