#include "TvlSettings.hpp"

#include "Utils/Misc/RegSet.hpp"
#include "Shared/Network/NetProxy.hpp"


/*extern*/ TvlSettings_t TvlSettings
{    
	/* BotType					*/	TvlBotType_Stream,
    /* TargetId[256]            */  "",
    /* Viewers                  */  500,
    /* ViewersMax               */  (256 * 256) / 2,
	/* RunMode					*/	TvlRunMode_Keepalive,
#ifdef _DEBUG	
	/* Test                     */  true,
#else
	/* Test                     */  true,
#endif	
};


static void TvlSettingsLoad(void)
{
	regset::load(TvlSettings.BotType, "tvl_bottype", { "stream", "clip", "vod" }, { TvlBotType_Stream, TvlBotType_Clip, TvlBotType_Vod });
	regset::load(TvlSettings.TargetId, "tvl_targetid");
	regset::load(TvlSettings.Viewers, "tvl_viewers");
	regset::load(TvlSettings.RunMode, "tvl_runmode", { "keepalive", "rst", "tmouttest" }, { TvlRunMode_Keepalive, TvlRunMode_Rst, TvlRunMode_TimeoutTest });
};


static void TvlSettingsSave(void)
{
	regset::save(TvlSettings.BotType, "tvl_bottype", { "stream", "clip", "vod" });
	regset::save(TvlSettings.TargetId, "tvl_targetid");
	regset::save(TvlSettings.Viewers, "tvl_viewers");
	regset::save(TvlSettings.RunMode, "tvl_runmode", { "keepalive", "rst", "tmouttest" });
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