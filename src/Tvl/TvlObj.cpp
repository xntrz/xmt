#include "TvlObj.hpp"
#include "TvlObjCtx.hpp"
#include "TvlResult.hpp"
#include "TvlSettings.hpp"
#include "TvlUtil.hpp"

#include "Shared/Common/Time.hpp"

#include "Utils/Proxy/ProxyObj.hpp"
#include "Utils/Misc/Timer.hpp"


namespace TVLOBJ
{
	const std::chrono::milliseconds SYSTEM_UPDATE_INTERVAL(20);
	const std::chrono::milliseconds VIEWER_UPDATE_INTERVAL(3000);
};


class CTvlViewerUpdateSvc final
{
public:
    CTvlViewerUpdateSvc(std::chrono::milliseconds updateInterval);
    ~CTvlViewerUpdateSvc();

private:
    std::thread m_thread;
    CEvent m_eventStop;
	std::chrono::milliseconds m_updateInterval;
	int32 m_viewerCount;
};


class CTvlObjSystem final : public CProxyObjSystem<CTvlObjCtx>
{
private:
    enum STATE
    {
        STATE_CHECK_TARGET = 0,
        STATE_CHECK_PROTECT,
        STATE_CHECK_CHANNELID,
        STATE_PRE_RUN,
        STATE_RUN,
        STATE_EOL,
    };

public:
    CTvlObjSystem(uint32 workpoolSize, std::chrono::milliseconds updateInterval);
    virtual void OnRun(void) override;
    virtual bool IsStopped(void) const override;
    void BranchState(bool bStateResult = true);

private:
    CTvlViewerUpdateSvc m_viewerUpdateSvc;
    STATE m_eState;
};


CTvlViewerUpdateSvc::CTvlViewerUpdateSvc(std::chrono::milliseconds updateInterval)
: m_thread()
, m_eventStop()
, m_updateInterval(updateInterval)
, m_viewerCount(0)
{
    m_thread = thread::spawn("TvlViewerUpdateSvc", [ this ]() {
        while (!m_eventStop.wait_for(m_updateInterval))
        {
            static int32(*TvlGetViewerCountFuncs[])(const char*) =
            {
				&TvluGetChannelViewersCount,
				&TvluGetClipViewsCount,
				&TvluGetVodViewsCount,
			};

			int32 viewerCount = TvlGetViewerCountFuncs[0](TvlSettings.TargetId);
			if (viewerCount != -1)
				m_viewerCount = viewerCount;
			
			TvlResult.SetViewerCountReal(m_viewerCount);
        };
    });
};


CTvlViewerUpdateSvc::~CTvlViewerUpdateSvc()
{
    m_eventStop.signal_once();
    m_thread.join();
};


CTvlObjSystem::CTvlObjSystem(uint32 workpoolSize, std::chrono::milliseconds updateInterval)
: CProxyObjSystem(workpoolSize, updateInterval)
, m_viewerUpdateSvc(TVLOBJ::VIEWER_UPDATE_INTERVAL)
, m_eState(STATE_CHECK_TARGET)
{
	;
};


void CTvlObjSystem::OnRun(void)
{
    switch (m_eState)
    {
    case STATE_CHECK_TARGET:
        {
            bool bResult = false;
            
            bResult = TvluIsChannelExist(TvlSettings.TargetId);
            if (bResult)
            {
                if (!TvlSettings.Test)
                {
                    bResult = TvluIsChannelLive(TvlSettings.TargetId);
                    if (!bResult)
                        TvlResult.SetError(CTvlResult::ERRTYPE_STREAM_NOT_LIVE);
                };
            }
            else
            {
                TvlResult.SetError(CTvlResult::ERRTYPE_STREAM_NOT_EXIST);
            };

            BranchState(bResult);
        }
        break;

    case STATE_CHECK_PROTECT:
        {
            bool bResult = TvluIsChannelProtected(TvlSettings.TargetId);
            if (bResult)
                TvlResult.SetError(CTvlResult::ERRTYPE_STREAM_PROTECTED);

            BranchState(!bResult);
        }
        break;

    case STATE_CHECK_CHANNELID:
        {
            int32 ChannelId = TvluGetChannelId(TvlSettings.TargetId);
            CTvlObjCtx::TargetChannelId = (ChannelId != 0 ? ChannelId : -1);

            BranchState(ChannelId != 0);
        }
        break;

    case STATE_RUN:
        {
            CProxyObjSystem::OnRun();        
        }
        break;
    };
};


bool CTvlObjSystem::IsStopped(void) const
{
    return ((m_eState == STATE_EOL) || (CProxyObjSystem::GetStoppedNum() > 0));
};


void CTvlObjSystem::BranchState(bool bStateResult)
{
    STATE eStateNext = m_eState;

    switch (m_eState)
    {
    case STATE_CHECK_TARGET:
        eStateNext = STATE_CHECK_PROTECT;
        break;

    case STATE_CHECK_PROTECT:
        eStateNext = STATE_CHECK_CHANNELID;
        break;

    case STATE_CHECK_CHANNELID:
        eStateNext = STATE_RUN;
        break;

    case STATE_RUN:
        eStateNext = STATE_EOL;
        break;
    };

    if (!bStateResult)
        eStateNext = STATE_EOL;

    m_eState = eStateNext;
};


static CTvlObjSystem* TvlObjSystem = nullptr;


void TvlObjInitialize()
{
    TvluInitialize();
    TvlResult.OnStart();

    CTvlObjCtx::TargetChannelId = -1;

#ifdef PRX_TEST
    uint32 workpoolSize = 1;
#else
    uint32 workpoolSize = uint32(TvlSettings.Viewers);
#endif

    TvlObjSystem = new CTvlObjSystem(workpoolSize, TVLOBJ::SYSTEM_UPDATE_INTERVAL);
};


void TvlObjTerminate()
{
    if (TvlObjSystem)
    {
        delete TvlObjSystem;
        TvlObjSystem = nullptr;
    };

    TvlResult.OnStop();
    TvluTerminate();
};


bool TvlObjIsStopped()
{
    if (TvlObjSystem)
        return TvlObjSystem->IsStopped();
    else
        return true;
};