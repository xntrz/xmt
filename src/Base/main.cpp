#include "AppMem.hpp"
#include "AppModule.hpp"
#include "AppDlgMain.hpp"
#include "AppVersion.hpp"
#include "AppRc.hpp"
#include "AppSettings.hpp"

#include "Shared/Common/Mem.hpp"
#include "Shared/Common/Thread.hpp"
#include "Shared/Common/Time.hpp"
#include "Shared/Common/Random.hpp"
#include "Shared/Common/Registry.hpp"
#include "Shared/Common/Dlg.hpp"
#include "Shared/Common/Configuration.hpp"
#include "Shared/Common/DirWatch.hpp"
#include "Shared/Common/UserAgent.hpp"
#include "Shared/Network/Net.hpp"
#include "Shared/Network/NetTest.hpp"
#include "Shared/File/File.hpp"


//#define VLDCHECK

#ifdef VLDCHECK
#include "vld.h"
#endif


int32 APIENTRY
_tWinMain(
	HINSTANCE	hInstance,
	HINSTANCE	hPrevInstance,
	LPTSTR		lpCmdLine,
	int32		iCmdShow
)
{
	if (!CfgCheckWinver())
	{
		DlgShowErrorMsg(NULL, IDS_ERR_WINVER, IDS_ERROR);
		return 0;
	};

	MemInitialize();

#ifdef _DEBUG
	DbgInitialize();
	DbgRegistModule(APP_NAME, hInstance);
#endif

	{
		AppMemInitialize();
		ThreadInitialize();
		TimeInitialize();
		RegInitialize();
		RndInitialize();
		DirWatchInitialize();
		CfgInitialize(APP_NAME, hInstance);

		if (FileInitialize())
		{
			CfgLoad(hInstance, APP_NAME, IDF_APPINI);
			if (DlgInitialize())
			{
				if (NetInitialize())
				{
					UserAgentInitialize();
					UserAgentRead("UserAgentDb.txt", IDF_APPUSERDB);

					AppSettingsInitialize();
#ifdef _DEBUG					
					if (CfgIsTestMode())
					{
						NetTest();
						DlgShowNotifyMsg(NULL, IDS_TESTPASS, IDS_TITLE);
					}
					else
					{
						AppDlgMainCreate();
					};
#else
					AppDlgMainCreate();
#endif					
					AppSettingsTerminate();

#ifdef _DEBUG
					if (!CfgIsArgPresent("nosave"))
						CfgSave();
#else			
					CfgSave();
#endif	
					UserAgentTerminate();
					NetTerminate();
				}
				else
				{
					DlgShowErrorMsg(NULL, IDS_ERR_NET, IDS_ERROR);
					NetTerminate();
				};

				DlgTerminate();
			}
			else
			{
				DlgShowErrorMsg(NULL, IDS_ERR_GX, IDS_ERROR);
				DlgTerminate();
			};
			
			FileTerminate();
		}
		else
		{
			DlgShowErrorMsg(NULL, IDS_ERR_FILE, IDS_ERROR);
			FileTerminate();
		};

		CfgTerminate();
		DirWatchTerminate();
		RndTerminate();
		RegTerminate();
		TimeTerminate();
		ThreadTerminate();
#ifdef VLDCHECK		
		VLDReportLeaks();
#endif		
		AppMemTerminate();
	}

#ifdef _DEBUG
	DbgRemoveModule(hInstance);
	DbgTerminate();
#endif

	MemTerminate();
	return 0;
};
