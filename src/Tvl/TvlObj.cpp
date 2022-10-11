#include "TvlObj.hpp"
#include "TvlObjCtx.hpp"
#include "TvlResult.hpp"
#include "TvlSettings.hpp"
#include "TvlUtil.hpp"

#include "Shared/Common/Time.hpp"

#include "Utils/Http/HttpReq.hpp"
#include "Utils/Misc/AsyncSvc.hpp"
#include "Utils/Proxy/ProxyObj.hpp"
#include "Utils/Misc/Timer.hpp"


namespace TVLOBJ
{
    const uint32 SYSTEM_PERIOD_INTERVAL = (1000u / 60u);
    const uint32 VW_UPD_INTERVAL = 5000;
    
    enum SVCFLAG
    {
        SVCFLAG_RUN = BIT(0),
    };
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
    CTvlObjSystem(int32 nWorkpoolSize, uint32 UpdateTime, int32 nCtxWorkpoolSize);
    virtual void OnRun(void) override;
    virtual bool IsEol(void) const override;
    void CtrlWorkpoolSize(void);
    void BranchState(bool bStateResult = true);

private:
    STATE m_eState;
    int32 m_nCtxWorkpoolSize;
    CTimer m_WalkTimer;
};


class CTvlVwUpdSvc final : public CAsyncSvc
{
public:
    CTvlVwUpdSvc(void);
    virtual void OnRun(void) override;

private:
    int32 m_VwCntPrev;
};


static CTvlObjSystem* TvlObjSystem = nullptr;
static CTvlVwUpdSvc* TvlVwUpdSvc = nullptr;


CTvlObjSystem::CTvlObjSystem(int32 nWorkpoolSize, uint32 UpdateTime, int32 nCtxWorkpoolSize)
: CProxyObjSystem(nWorkpoolSize, UpdateTime)
, m_eState(STATE_CHECK_TARGET)
, m_nCtxWorkpoolSize(nCtxWorkpoolSize)
, m_WalkTimer()
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
            
            switch (TvlSettings.BotType)
            {
            case TvlBotType_Stream:
                {
                    bResult = TvluIsChannelExist(TvlSettings.TargetId);
                    if (bResult)
                    {
                        if (!TvlSettings.Test)
                        {
                            bResult = TvluIsChannelLive(TvlSettings.TargetId);
                            if (!bResult)
                                CTvlResult::Instance().SetError(CTvlResult::ERRTYPE_STREAM_NOT_LIVE);
                        };
                    }
                    else
                    {
                        CTvlResult::Instance().SetError(CTvlResult::ERRTYPE_STREAM_NOT_EXIST);
                    };
                }
                break;

            case TvlBotType_Clip:
                {
                    bResult = TvluIsClipExist(TvlSettings.TargetId);
                    if (!bResult)
                        CTvlResult::Instance().SetError(CTvlResult::ERRTYPE_CLIP_NOT_EXIST);
                }
                break;

            case TvlBotType_Vod:
                {
                    bResult = TvluIsVodExist(TvlSettings.TargetId);
                    if (!bResult)
                        CTvlResult::Instance().SetError(CTvlResult::ERRTYPE_VOD_NOT_EXIST);
                }
                break;

            default:
                ASSERT(false);
                break;
            };

            BranchState(bResult);
        }
        break;

    case STATE_CHECK_PROTECT:
        {
            if (TvlSettings.BotType == TvlBotType_Stream)
            {
                bool bResult = TvluIsChannelProtected(TvlSettings.TargetId);
                if (bResult)
                    CTvlResult::Instance().SetError(CTvlResult::ERRTYPE_STREAM_PROTECTED);
                
                BranchState(!bResult);
            }
            else
            {
                BranchState();
            };
        }
        break;

    case STATE_CHECK_CHANNELID:
        {
            if (TvlSettings.BotType == TvlBotType_Stream)
            {
                int32 ChannelId = TvluGetChannelId(TvlSettings.TargetId);
                CTvlObjCtx::TargetChannelId = (ChannelId != 0 ? ChannelId : -1);
                
                BranchState(ChannelId != 0);
            }
            else
            {
                BranchState();
            };
        }
        break;

    case STATE_PRE_RUN:
        {
            BranchState();
            m_WalkTimer.Reset();
        }
        break;

    case STATE_RUN:
        {
            CProxyObjSystem::OnRun();        
            int32 Elapsed = std::max(int32(m_WalkTimer.Reset()) - int32(TVLOBJ::SYSTEM_PERIOD_INTERVAL), 0);
            CTvlResult::Instance().SetCtxObjWalkTime(uint32(Elapsed));
        }
        break;
    };
};


bool CTvlObjSystem::IsEol(void) const
{
    return ((m_eState == STATE_EOL) || (CProxyObjSystem::GetEolNum() > 0));
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
        eStateNext = STATE_PRE_RUN;
        break;

    case STATE_PRE_RUN:
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


CTvlVwUpdSvc::CTvlVwUpdSvc(void)
: CAsyncSvc("TvlVwUpd", TVLOBJ::VW_UPD_INTERVAL)
, m_VwCntPrev(0)
{
    ;
};


void CTvlVwUpdSvc::OnRun(void)
{
    int32 VwCnt = 0;
    
    switch (TvlSettings.BotType)
    {
    case TvlBotType_Stream:
        VwCnt = TvluGetChannelViewersCount(TvlSettings.TargetId);
        break;

    case TvlBotType_Clip:
        VwCnt = TvluGetClipViewsCount(TvlSettings.TargetId);
        break;

    case TvlBotType_Vod:
        VwCnt = TvluGetVodViewsCount(TvlSettings.TargetId);
        break;

    default:
        ASSERT(false);
        break;
    };

    if (VwCnt == -1)
        VwCnt = m_VwCntPrev;
    
    m_VwCntPrev = VwCnt;
    
    CTvlResult::Instance().SetViewerCountReal(VwCnt);
};


void TvlObjStart(void)
{
    TvluInitialize();

    int32 nWorkpoolSize = TvlSettings.Viewers;

    if (TvlSettings.RunMode == TvlRunMode_TimeoutTest)
        nWorkpoolSize = 1;

    TvlObjSystem = new CTvlObjSystem(nWorkpoolSize, TVLOBJ::SYSTEM_PERIOD_INTERVAL, 0);
    if (TvlObjSystem)
    {
        TvlObjSystem->Start();
        
        TvlVwUpdSvc = new CTvlVwUpdSvc();
        if (TvlVwUpdSvc)
            TvlVwUpdSvc->Start();
    };
};


void TvlObjStop(void)
{
    CHttpReq::suspend();
    TvluPreTerminate();
    
    if (TvlVwUpdSvc)
    {
        TvlVwUpdSvc->Stop();
        delete TvlVwUpdSvc;
        TvlVwUpdSvc = nullptr;
    };
    
    if (TvlObjSystem)
    {
        TvlObjSystem->Stop();
        delete TvlObjSystem;
        TvlObjSystem = nullptr;
    };

    TvluPostTerminate();
    CHttpReq::resume();
};


bool TvlObjIsEol(void)
{
    ASSERT(TvlObjSystem);
    return TvlObjSystem->IsEol();
};