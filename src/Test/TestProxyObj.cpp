#include "TestProxyObj.hpp"

#include "Utils/Proxy/ProxyObj.hpp"
#include "Utils/Proxy/ProxySeq.hpp"


namespace TESTCTX
{
    enum STATE
    {
        STATE_1 = 0,
        STATE_2,
    };

    enum SVCFLAG
    {
        /* jump to next state */
        SVCFLAG_STRIDE  = (1 << 0),
        
        /* set object to stop state */
        SVCFLAG_STOP    = (1 << 1),
    };
};


class CTestObjState1 : public CProxyObj::CState
{
public:
    virtual void Attach() override {};
    virtual void Detach() override {};
    virtual void Observing() override { Subject().ServiceRequest(TESTCTX::SVCFLAG_STRIDE); };
};


class CTestObjState2 : public CProxyObj::CState
{
public:
    virtual void Attach() override {};
    virtual void Detach() override {};
    virtual void Observing() override { Subject().ServiceRequest(TESTCTX::SVCFLAG_STOP); };
};


class CTestObj : public CProxyObj
{
public:
    CTestObj() : m_state1(), m_state2(), m_seq() {};
    virtual ~CTestObj() {};
    virtual void Start() override;
    virtual void Stop() override {};
    virtual void Service(uint32 svcFlags) override;

private:
    CTestObjState1 m_state1;
    CTestObjState2 m_state2;
    CProxyObjSeq m_seq;
};


void CTestObj::Start()
{
    /* init seq man */
    m_seq = CProxyObjSeq(
        TESTCTX::STATE_1,   // begin
        TESTCTX::STATE_2,   // end
        TESTCTX::STATE_1    // start
    );

    /* regist states */
    StateRegist(TESTCTX::STATE_1, &m_state1);
    StateRegist(TESTCTX::STATE_2, &m_state2);
    StateJump(TESTCTX::STATE_1);    
};


void CTestObj::Service(uint32 svcFlags)
{
    /* handle stride request - change to the next state */
    if (svcFlags & TESTCTX::SVCFLAG_STRIDE)
    {
        m_seq.StrideNext();
        StateJump(m_seq.Current());
    };

    /* handle stop request - mark obj as stopped */
    if (svcFlags & TESTCTX::SVCFLAG_STOP)
    {
        StateJump(STATE_STOP);
    };
};


class CTestObjSystem : public CProxyObjSystem<CTestObj>
{
private:
    enum SYSTEMSTATE
    {
        SYSTEMSTATE_CHECK_FIRST = 0,
        SYSTEMSTATE_CHECK_SECOND,
        SYSTEMSTATE_RUN,
    };
    
public:
    CTestObjSystem(std::size_t workpoolSize, std::chrono::milliseconds updateInterval);
    virtual ~CTestObjSystem();
    virtual void OnRun() override;

private:
    SYSTEMSTATE m_systemstate;
    std::size_t m_testCounter;
};


CTestObjSystem::CTestObjSystem(std::size_t workpoolSize, std::chrono::milliseconds updateInterval)
: CProxyObjSystem(workpoolSize, updateInterval)
, m_systemstate(SYSTEMSTATE_CHECK_FIRST)
, m_testCounter(0u)
{
    ;
};


CTestObjSystem::~CTestObjSystem()
{
    /**
     *  0 - begin
     *  1 - check first
     *  2 - check second
     *  3 - start object
     *  4 - object state 1 observing
     *  5 - object state 2 observing and stop
     */
    ASSERT(m_testCounter == 5);
};


void CTestObjSystem::OnRun()
{
    switch (m_systemstate)
    {
    case SYSTEMSTATE_CHECK_FIRST:
        /* since we override main run we got first tick here so counter is 0 */
        ASSERT(m_testCounter == 0);
        ++m_testCounter;
        m_systemstate = SYSTEMSTATE_CHECK_SECOND;
        break;
        
    case SYSTEMSTATE_CHECK_SECOND:
        /* second tick we here counter should be 1 */
        ASSERT(m_testCounter == 1);
        ++m_testCounter;
		m_systemstate = SYSTEMSTATE_RUN;
        break;

    case SYSTEMSTATE_RUN:
        /* third and subsequent ticks we here */
        CProxyObjSystem::OnRun();
        if (!IsStopped())
            ++m_testCounter;
        break;

    default:
        break;
    };
};


void TestProxyObj()
{
    std::chrono::milliseconds t = std::chrono::milliseconds(16);

    /* start system */
    CTestObjSystem sys(1, t);

    while (!sys.IsStopped())
        ;
};