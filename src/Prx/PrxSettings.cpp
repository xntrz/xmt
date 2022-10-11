#include "PrxSettings.hpp"

#include "Utils/Misc/RegSet.hpp"

#include "Shared/Network/NetProxy.hpp"
#include "Shared/Common/UserAgent.hpp"


/*extern*/ PrxSettings_t PrxSettings
{
    /* Type             */  NetProxy_Http,
    /* Retry            */  3,
    /* Workpool         */  100,
    /* Timeout          */  10000,
    /* TargetHost[256]  */  "google.com",
    /* TargetPort       */  443,
    /* FlagRequest      */  true,
    /* UserAgent[256]   */  ""
};


static void PrxSettingsLoad(void)
{
    regset::load(PrxSettings.Type, "prx_type", { "http", "socks4", "socks4a", "socks5" }, { NetProxy_Http, NetProxy_Socks4, NetProxy_Socks4a, NetProxy_Socks5 });
    regset::load(PrxSettings.Retry, "prx_retry");
    regset::load(PrxSettings.Workpool, "prx_workpool");
    regset::load(PrxSettings.Timeout, "prx_timeout");
    regset::load(PrxSettings.TargetHost, "prx_target_addr");
    regset::load(PrxSettings.TargetPort, "prx_target_port");
    regset::load(PrxSettings.FlagRequest, "prx_flag_request");

    const char* UserAgent = UserAgentGenereate();
    ASSERT(std::strlen(UserAgent) < COUNT_OF(PrxSettings.UserAgent));
    std::strcpy(PrxSettings.UserAgent, UserAgent);
};


static void PrxSettingsSave(void)
{
    regset::save(PrxSettings.Type, "prx_type", { "http", "socks4", "socks4a", "socks5" });
};


void PrxSettingsInitialize(void)
{
    RegResetRegist(PrxSettingsLoad);
    PrxSettingsLoad();
};


void PrxSettingsTerminate(void)
{
    PrxSettingsSave();
};