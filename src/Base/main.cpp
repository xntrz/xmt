#include "AppMem.hpp"
#include "AppModule.hpp"
#include "AppVersion.hpp"
#include "AppRc.hpp"
#include "AppSettings.hpp"
#include "AppUI.hpp"

#include "Shared/Common/Mem.hpp"
#include "Shared/Common/Thread.hpp"
#include "Shared/Common/Time.hpp"
#include "Shared/Common/Random.hpp"
#include "Shared/Common/Registry.hpp"
#include "Shared/Common/Configuration.hpp"
#include "Shared/Common/UserAgent.hpp"
#include "Shared/Network/Net.hpp"
#include "Shared/File/File.hpp"
#include "Shared/UI/nk_text.hpp"
#include "Shared/UI/nk_font.hpp"
#include "Shared/UI/nk_window.hpp"

#include "Shared/Common/IoService.hpp"
#include "Shared/Common/ThreadPool.hpp"


//#define VLDCHECK
#ifdef VLDCHECK
#include "vld.h"
#endif


int32 APIENTRY
_tWinMain(
	HINSTANCE	hInstance,
	HINSTANCE	/*hPrevInstance*/,
	LPTSTR		/*lpCmdLine*/,
	int32		/*iCmdShow*/
)
{
	MemInitialize();
#ifdef _DEBUG
	DbgInitialize();
	DbgRegistModule(APP_NAME, hInstance);
#endif
	thread::initialize();
	thread::regist_current("Main thread");
	AppMemInitialize((1024 * 1024) * 1); // reserve 1 mb heap for each thread
	TimeInitialize();
	RegInitialize();
	RndInitialize();
	CfgInitialize(APP_NAME, hInstance);

	if (FileInitialize())
	{
		nk_text_init();
		nk_font_install(hInstance, IDF_APPFONT);
		nkgdi_window_init(nkgdi_window_theme_dark);

		CfgLoad(hInstance, APP_NAME, IDF_APPINI);
		if (NetInitialize())
		{
			CThreadPool defaultThreadPool(4);
			UserAgentInitialize();
			UserAgentRead("UserAgentDb.txt", IDF_APPUSERDB);

			AppSettingsInitialize();

			do
			{
				AppUI_Loop();
			} while (AppUI_IsSettingsChange());

			AppSettingsTerminate();

			UserAgentTerminate();
			NetTerminate();
		}
		else
		{
			NetTerminate();
		};

		nkgdi_window_shutdown();
		nk_font_uninstall();
		nk_text_term();
		CfgSave();
		FileTerminate();
	}
	else
	{
		FileTerminate();
	};

	CfgTerminate();
	RndTerminate();
	RegTerminate();
	TimeTerminate();
#ifdef VLDCHECK		
	VLDReportLeaks();
#endif		
	AppMemTerminate();
	thread::remove_current();
	AppMemTerminate2();
	thread::terminate();
#ifdef _DEBUG
	DbgRemoveModule(hInstance);
	DbgTerminate();
#endif
	MemTerminate();
	return 0;
};
