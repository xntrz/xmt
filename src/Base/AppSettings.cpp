#include "AppSettings.hpp"

#include "Shared/Common/Registry.hpp"


/*extern*/ AppSettings_t AppSettings
{
	/* PathSslCert[256]			*/	"",
	/* PathSslKey[256] 			*/	"",
	/* ModCurSel[256]  			*/	"",
	/* WindowW         			*/	0,
	/* WindowH         			*/	0,
	/* FontH           			*/	16,
	/* MultithreadProxySystem	*/	false
};


static void AppSettingsLoad(void)
{
	HOBJ hVar = RegVarFind("app_pathsslcert");
	if (hVar)
		RegVarReadString(hVar, AppSettings.PathSslCert, COUNT_OF(AppSettings.PathSslCert));

	hVar = RegVarFind("app_pathsslkey");
	if (hVar)
		RegVarReadString(hVar, AppSettings.PathSslKey, COUNT_OF(AppSettings.PathSslKey));

	hVar = RegVarFind("app_cursel");
	if (hVar)
		RegVarReadString(hVar, AppSettings.ModCurSel, COUNT_OF(AppSettings.ModCurSel));

	hVar = RegVarFind("app_win_w");
	if (hVar)
		AppSettings.WindowW = RegVarReadInt32(hVar);

	hVar = RegVarFind("app_win_h");
	if (hVar)
		AppSettings.WindowH = RegVarReadInt32(hVar);

	hVar = RegVarFind("app_font_h");
	if (hVar)
		AppSettings.FontH = RegVarReadInt32(hVar);

	hVar = RegVarFind("app_prxsys_threads");
	if (hVar)
		AppSettings.MultithreadProxySystem = RegArgToBool(RegVarReadString(hVar));
};


static void AppSettingsSave(void)
{
	HOBJ hVar = RegVarFind("app_cursel");
	if (hVar)
		RegVarSetValue(hVar, AppSettings.ModCurSel);

	hVar = RegVarFind("app_win_w");
	if (hVar)
		RegVarSetValue(hVar, std::to_string(AppSettings.WindowW).c_str());

	hVar = RegVarFind("app_win_h");
	if (hVar)
		RegVarSetValue(hVar, std::to_string(AppSettings.WindowH).c_str());

	hVar = RegVarFind("app_font_h");
	if (hVar)
		RegVarSetValue(hVar, std::to_string(AppSettings.FontH).c_str());

	hVar = RegVarFind("app_prxsys_threads");
	if (hVar)
	{
		char Tmp[128];
		Tmp[0] = '\0';
		RegBoolToArg(AppSettings.MultithreadProxySystem, Tmp, sizeof(Tmp));
		RegVarSetValue(hVar, Tmp);
	};
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