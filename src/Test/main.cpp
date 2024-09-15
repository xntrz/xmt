#include "TestRc.hpp"
#include "TestHttp.hpp"
#include "TestWebsocket.hpp"
#include "TestJson.hpp"
#include "TestNet.hpp"
#include "TestAsyncService.hpp"
#include "TestUI.hpp"
#include "TestUserAgent.hpp"

#include "Utils/init.hpp"
#include "Utils/Misc/WebUtils.hpp"

#include "Shared/Common/UserAgent.hpp"
#include "Shared/UI/nk_text.hpp"


extern "C" DLLEXPORT void ModuleEvtProc(MODULE_EVT& evt)
{
	switch (evt.id)
	{
	case MODULE_EVT_STARTUP:
		nk_text_load_mod_rc("test_en", evt.param.startup.hInstance, IDF_TEXT_EN);
		UtilsInitialize();
		TestUserAgent = UserAgentGenereate();
		break;

	case MODULE_EVT_SHUTDOWN:
		TestUserAgent.clear();
		UtilsTerminate();
		break;

	case MODULE_EVT_ATTACH:
	case MODULE_EVT_DETACH:
		break;

	case MODULE_EVT_INFO:
		nk_text_path_locale_push("test");
		evt.param.info.title 		= nk_text_id(1);
		evt.param.info.description 	= nk_text_id(1);
		nk_text_path_pop();
		evt.param.info.icoid 	= -1;
		evt.param.info.ui_proc 	= &TestUI;
		evt.param.info.tag 		= "Test";
		break;

	default:
		break;
	};
};