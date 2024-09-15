#include "TvlRc.hpp"
#include "TvlSettings.hpp"
#include "TvlObj.hpp"
#include "TvlResult.hpp"
#include "TvlUI.hpp"

#include "Shared/UI/nk_text.hpp"
#include "Shared/Common/Configuration.hpp"

#include "Utils/init.hpp"


extern "C" DLLEXPORT void ModuleEvtProc(MODULE_EVT& evt)
{
	switch (evt.id)
	{
	case MODULE_EVT_STARTUP:
		CfgLoad(evt.param.startup.hInstance, "Tvl", IDF_INI);
		TvlSettingsInitialize();
		nk_text_load_mod_rc("tvl_en", evt.param.startup.hInstance, IDF_TEXT_EN);
		UtilsInitialize();
		break;

	case MODULE_EVT_SHUTDOWN:
		UtilsTerminate();
		TvlSettingsTerminate();
		break;

	case MODULE_EVT_ATTACH:
		TvlResult.OnAttach();
		break;

	case MODULE_EVT_DETACH:
		TvlResult.OnDetach();
		break;

	case MODULE_EVT_INFO:
		nk_text_path_locale_push("tvl");
		evt.param.info.title 		= nk_text_id(1);
		evt.param.info.description 	= nk_text_id(1);
		nk_text_path_pop();
		evt.param.info.icoid 	= IDF_ICO;
		evt.param.info.ui_proc 	= &TvlUI;
		evt.param.info.tag 		= "tvl";
		break;

	default:
		break;
	};
};