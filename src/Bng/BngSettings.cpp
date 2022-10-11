#include "BngSettings.hpp"

#include "Utils/Misc/RegSet.hpp"

#include "Shared/Network/NetProxy.hpp"


/*extern*/ BngSettings_t BngSettings
{
    /* Workpool                 */  100,
    /* TargetId[256]            */  "",
    /* Viewers                  */  500,
    /* ViewersMax               */  (256 * 256) / 2,
    /* Host[256]                */  "bongacams.com",
    /* ProxyUseMode             */  false,
    /* ProxyType                */  NetProxy_Http,
    /* ProxyRetryMax            */  3,
};


static void BngSettingsLoad(void)
{
    regset::load(BngSettings.Workpool, "bng_workpool");
    regset::load(BngSettings.TargetId, "bng_targetid");
    regset::load(BngSettings.Viewers, "bng_viewers");
    regset::load(BngSettings.ProxyUseMode, "bng_prxmode");
    regset::load(BngSettings.ProxyType, "bng_prxtype", { "http", "socks4", "socks4a", "socks5" }, { NetProxy_Http, NetProxy_Socks4, NetProxy_Socks4a, NetProxy_Socks5 });
    regset::load(BngSettings.ProxyRetryMax, "bng_prxretry");
};


static void BngSettingsSave(void)
{
    regset::save(BngSettings.TargetId, "bng_targetid");
    regset::save(BngSettings.Viewers, "bng_viewers");
    regset::save(BngSettings.ProxyUseMode, "bng_prxmode");
    regset::save(BngSettings.ProxyType, "bng_prxtype", { "http", "socks4", "socks4a", "socks5" });
};


void BngSettingsInitialize(void)
{
    RegResetRegist(BngSettingsLoad);
    BngSettingsLoad();
};


void BngSettingsTerminate(void)
{
    BngSettingsSave();
};