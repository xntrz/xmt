#include "BngObj.hpp"
#include "BngObjCtx.hpp"
#include "BngResult.hpp"
#include "BngSettings.hpp"
#include "BngUtil.hpp"

#include "Utils/Misc/Timer.hpp"
#include "Utils/Websocket/Websocket.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Proxy/ProxyObj.hpp"


namespace BNGOBJ
{
    const uint32 SYSTEM_PERIOD_INTERVAL = (1000u / 60u);

    enum SVCFLAG
    {
        SVCFLAG_RUN = BIT(0),
    };
};


class CBngObj final : public CProxyObj
{
public:
    CBngObj(void);
    virtual void Start(void) override;
    virtual void Stop(void) override;
    virtual void Service(uint32 ServiceFlags) override;
    void AllocCtx(void);

private:
    CList<CBngObjCtx, CBngObjCtxTag>::iterator m_CtxCurrent;
    CList<CBngObjCtx, CBngObjCtxTag> m_CtxList;
    CHttpReq m_SharedReq;
};


class CBngObjSystem final : public CProxyObjSystem<CBngObj>
{
private:
    enum STATE
    {
        STATE_CHECK_HOST = 0,
        STATE_CHECK_TARGET,
        STATE_PRE_RUN,
        STATE_RUN,
        STATE_EOL,
    };

public:
    CBngObjSystem(int32 nWorkpoolSize, uint32 UpdateTime, int32 nCtxWorkpoolSize);
    virtual ~CBngObjSystem(void);
    virtual void OnRun(void) override;
    virtual bool IsEol(void) const override;
    void BranchState(bool bStateResult = true);
    void CtrlWorkpoolSize(void);

private:
    STATE m_eState;
    int32 m_nCtxWorkpoolSize;
    CTimer m_WalkTimer;
};


static CBngObjSystem* BngObjSystem = nullptr;
static CBngObjCtx* BngObjCtxPool = nullptr;
static CList<CBngObjCtx, CBngObjCtxTag> BngObjCtxFreeList = {};


CBngObj::CBngObj(void)
: CProxyObj()
, m_CtxList()
, m_CtxCurrent(nullptr, nullptr)
{
    ;
};


void CBngObj::Start(void)
{
    ServiceRequest(BNGOBJ::SVCFLAG_RUN);

    if (m_CtxCurrent)
        (*m_CtxCurrent).Start();
};


void CBngObj::Stop(void)
{
    ;
};


void CBngObj::Service(uint32 ServiceFlags)
{
    if (m_CtxCurrent)
    {
        CBngObjCtx& BngCtx = *m_CtxCurrent;

        BngCtx.Run();

        if (BngCtx.IsRequestedEol())
        {
            StateJump(STATE_LABEL_EOL);
        }
        else
        {
            if (BngCtx.IsRequestedYield())
            {
                BngCtx.Stop();

                if (!++m_CtxCurrent)
                    m_CtxCurrent = m_CtxList.begin();

                (*m_CtxCurrent).Start();
            };

            ServiceRequest(BNGOBJ::SVCFLAG_RUN);
        };
    };
};


void CBngObj::AllocCtx(void)
{
    ASSERT(!BngObjCtxFreeList.empty());

    CBngObjCtx* BngCtx = BngObjCtxFreeList.front();
    BngObjCtxFreeList.erase(BngCtx);

    m_CtxList.push_back(BngCtx);

    if (!m_CtxCurrent)
        m_CtxCurrent = m_CtxList.begin();

    BngCtx->SetSharedReq(&m_SharedReq);
};


CBngObjSystem::CBngObjSystem(int32 nWorkpoolSize, uint32 UpdateTime, int32 nCtxWorkpoolSize)
: CProxyObjSystem(nWorkpoolSize, UpdateTime)
, m_eState(STATE_CHECK_HOST)
, m_nCtxWorkpoolSize(nCtxWorkpoolSize)
{
    ;
};


CBngObjSystem::~CBngObjSystem(void)
{
    ;
};


void CBngObjSystem::OnRun(void)
{
    switch (m_eState)
    {
    case STATE_CHECK_HOST:
        {
            bool bResult = false;
            
            if (!BngSettings.ProxyUseMode)
            {
				bResult = (!BnguGetHostForMe().empty());
                if (!bResult)
                    CBngResult::Instance().SetError(CBngResult::ERRTYPE_HOST_UNREACH);
            }
            else
            {
                bResult = true;
            };

            BranchState(bResult);
        }
        break;
        
    case STATE_CHECK_TARGET:
        {
            bool bResult = false;

            bResult = BnguIsChannelExist(BngSettings.TargetId);
            if (bResult)
            {
                bResult = BnguIsChannelLive(BngSettings.TargetId);
                if (!bResult)
                    CBngResult::Instance().SetError(CBngResult::ERRTYPE_STREAM_NOT_LIVE);
            }
            else
            {
                CBngResult::Instance().SetError(CBngResult::ERRTYPE_STREAM_NOT_EXIST);
            };

            BranchState(bResult);
        }
        break;
        
    case STATE_PRE_RUN:
        {
            int32 nWorkpoolSize = GetWorkpoolSize();
            int32 nCtxWorkpoolSize = m_nCtxWorkpoolSize;

            for (int32 i = 0, j = 0; i < nCtxWorkpoolSize; ++i, ++j)
            {
                j %= nWorkpoolSize;

                CBngObj& BngObj = GetObj(j);
                BngObj.AllocCtx();
            };
            
            BranchState();
            m_WalkTimer.Reset();
        }
        break;

    case STATE_RUN:
        {            
            CProxyObjSystem::OnRun();
            
            int32 Elapsed = std::max(int32(m_WalkTimer.Reset()) - int32(BNGOBJ::SYSTEM_PERIOD_INTERVAL), 0);
            CBngResult::Instance().SetCtxObjWalkTime(uint32(Elapsed));
        }
        break;
    };
};


bool CBngObjSystem::IsEol(void) const
{
    return ((m_eState == STATE_EOL) || (CProxyObjSystem::GetEolNum() > 0));
};


void CBngObjSystem::BranchState(bool bStateResult)
{
    STATE eStateNext = m_eState;

    switch (m_eState)
    {
    case STATE_CHECK_HOST:
        eStateNext = STATE_CHECK_TARGET;
        break;
        
    case STATE_CHECK_TARGET:
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


void BngObjStart(void)
{
    BnguInitialize();

	if (BngSettings.ProxyUseMode)
	{
		int32 nProxyCount = CBngResult::Instance().ImportList().count();
		nProxyCount = Max(nProxyCount, 1);

		CBngObjCtx::m_pProxyQueue = new CQueue<uint64>(nProxyCount);
		ASSERT(nProxyCount > 0);
		for (int32 i = 0; i < nProxyCount; ++i)
			CBngObjCtx::m_pProxyQueue->Push(CBngResult::Instance().ImportList().addr_at(i));
	};

    //
    //  workpool <  ctxpool --> ok, some work will use multiple ctx
    //  workpool >  ctxpool --> clamp workpool to ctxpool, then work will use all ctx pool
    //  workpool == ctxpool --> ok, work will use all ctx pool
    //
    int32 nWorkpoolSize = BngSettings.Workpool;
    int32 nCtxWorkpoolSize = BngSettings.Viewers;

    if (nWorkpoolSize > nCtxWorkpoolSize)
        nWorkpoolSize = nCtxWorkpoolSize;

    if (nCtxWorkpoolSize > 0)
    {
        BngObjCtxPool = new CBngObjCtx[nCtxWorkpoolSize];
        ASSERT(BngObjCtxPool);
        if (BngObjCtxPool)
        {
            for (int32 i = 0; i < nCtxWorkpoolSize; ++i)
                BngObjCtxFreeList.push_back(&BngObjCtxPool[i]);
        };
    };

    BngObjSystem = new CBngObjSystem(nWorkpoolSize, BNGOBJ::SYSTEM_PERIOD_INTERVAL, nCtxWorkpoolSize);
    ASSERT(BngObjSystem);
    if (BngObjSystem)
        BngObjSystem->Start();
};


void BngObjStop(void)
{
    CHttpReq::suspend();
    BnguPreTerminate();

    //
    //  Terminate bot system
    //
    if (BngObjSystem)
    {
        BngObjSystem->Stop();
        delete BngObjSystem;
        BngObjSystem = nullptr;
    };

    if (BngObjCtxPool)
    {
        delete[] BngObjCtxPool;
        BngObjCtxPool = nullptr;
    };

    BngObjCtxFreeList.clear();

    if (CBngObjCtx::m_pProxyQueue)
    {
        delete CBngObjCtx::m_pProxyQueue;
        CBngObjCtx::m_pProxyQueue = nullptr;
    };

    //
    //  Terminate bot global settings
    //
    BnguPostTerminate();
    CHttpReq::resume();
};


bool BngObjIsEol(void)
{
    ASSERT(BngObjSystem);
    return BngObjSystem->IsEol();
};