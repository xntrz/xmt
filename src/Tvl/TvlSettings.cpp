#include "TvlSettings.hpp"

#include "Utils/Misc/RegSet.hpp"
#include "Shared/Network/NetProxy.hpp"


/*extern*/ TvlSettings_t TvlSettings
{    
    /* TargetId[256]            */  "",
    /* Viewers                  */  500,
	/* RunMode					*/	TVLRUNMODE_KEEPALIVE,
#ifdef _DEBUG	
	/* Test                     */  true,
#else
	/* Test                     */  true,
#endif	
};


static void TvlSettingsLoad(void)
{
	regset::load(TvlSettings.TargetId, "tvl_targetid");
	regset::load(TvlSettings.Viewers, "tvl_viewers");
	regset::load(TvlSettings.RunMode, "tvl_runmode", { "rst", "keepalive" }, { TVLRUNMODE_RST, TVLRUNMODE_KEEPALIVE});
};


static void TvlSettingsSave(void)
{
	regset::save(TvlSettings.TargetId, "tvl_targetid");
	regset::save(TvlSettings.Viewers, "tvl_viewers");
	regset::save(TvlSettings.RunMode, "tvl_runmode", { "rst", "keepalive" });
};


void TvlSettingsInitialize(void)
{
	RegResetRegist(TvlSettingsLoad);
	TvlSettingsLoad();
};


void TvlSettingsTerminate(void)
{
	TvlSettingsSave();
};