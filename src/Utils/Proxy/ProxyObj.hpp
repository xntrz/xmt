#pragma once

#include "Utils/Misc/AsyncSvc.hpp"

#include "Shared/Network/NetTypes.hpp"
#include "Shared/Network/NetProxy.hpp"


class IProxyObjState
{
private:
    friend class CProxyObj;
    
public:
    IProxyObjState(void) = default;
    virtual ~IProxyObjState(void) = default;
    virtual void Attach(void* Param) = 0;
    virtual void Detach(void) = 0;
    virtual void Observing(void) = 0;
    inline CProxyObj& Subject(void) { return *m_pSubject; };
        
private:
    CProxyObj* m_pSubject;
};


class CProxyObj
{
public:
    static const int32 STATE_MAX = 8;
    static const int32 STATE_LABEL_EOL = -1;

private:
    template<class T>
    union STATE_PTR
    {
        static_assert(sizeof(std::size_t) == 4, "update me");
        
        struct
        {
            unsigned Address : (BITSOF(std::size_t) - 1);
            unsigned FlagAlloc : 1;
        } Parts;
        T* Value;
        
        inline T* operator->(void)
        {
            return Value;
        };
    };

public:
    CProxyObj(void);
    virtual ~CProxyObj(void);
    virtual void Start(void) = 0;
    virtual void Stop(void) = 0;
    virtual void Service(uint32 ServiceFlags) = 0;
    void Run(void);
    void SetProxyType(NetProxy_t Proxytype);
    NetProxy_t GetProxyType(void);
    void SetProxyAddr(uint64 NetAddr);
    uint64 GetProxyAddr(void);
    void StateRegist(int32 Label, IProxyObjState* pState, bool bFlagAlloc = true);
    void StateJump(int32 Label, void* Param = nullptr);
    void ServiceRequest(uint32 Flag);
    void* GetShared(void);
    void SetShared(void* Param);
    bool IsEol(void) const;
    int32 CurrentStateLabel(void) const;

private:
    void* m_SharedData;
    NetProxy_t m_ProxyType;
    uint64 m_ProxyAddr;
    std::atomic<uint32> m_uServiceFlags;
    std::atomic<int32> m_iStateLabelCurrent;
    std::atomic<int32> m_iStateLabelRequest;
    void* m_StateParamRequest;
    STATE_PTR<IProxyObjState> m_aStatePtr[STATE_MAX];
    bool m_bFlagEol;
};


template<class T>
class CProxyObjSystem : public CAsyncSvc
{
protected:
    enum SYSTEMSTATE
    {
        SYSTEMSTATE_START = 0,
        SYSTEMSTATE_RUN,
        SYSTEMSTATE_STOP,
		SYSTEMSTATE_EOL,
    };

public:
    static_assert(std::is_base_of<CProxyObj, T>::value, "T is not base of IProxyObjState");
    
    inline CProxyObjSystem(int32 nWorkpoolSize, uint32 UpdateTime)
    : CAsyncSvc("PrxObjSys", UpdateTime)
    , m_pWorkpool(nullptr)
    , m_nWorkpoolSize(nWorkpoolSize)
    , m_nWorkpoolRunCnt(nWorkpoolSize)
    , m_Systemstate(SYSTEMSTATE_START)
    , m_nEolCnt(0)
    , m_nStartedCnt(0)
    {
        ;
    };

    virtual ~CProxyObjSystem(void)
    {
        ;
    };

    virtual void OnStart(void) override
    {    
        m_pWorkpool = new T[m_nWorkpoolSize]();
    };

    virtual void OnStop(void) override
    {
        m_Systemstate = SYSTEMSTATE_STOP;
		CProxyObjSystem::OnRun();
		ASSERT(m_Systemstate == SYSTEMSTATE_EOL);

        if (m_pWorkpool)
        {
            delete[] m_pWorkpool;
            m_pWorkpool = nullptr;
        };
    };

    virtual void OnRun(void) override
    {
        switch (m_Systemstate)
        {
        case SYSTEMSTATE_START:
            {
				m_Systemstate = SYSTEMSTATE_RUN;
            }
            break;

        case SYSTEMSTATE_RUN:
            {
                int32 nStartCnt = m_nStartedCnt;
                for (int32 i = nStartCnt; i < m_nWorkpoolRunCnt; ++i)
                {
                    m_pWorkpool[i].Start();
                    ++nStartCnt;
                };
                m_nStartedCnt = nStartCnt;

                int32 nEolCnt = 0;
                for (int32 i = 0; i < m_nWorkpoolRunCnt; ++i)
                {
                    if (!m_pWorkpool[i].IsEol())
                        m_pWorkpool[i].Run();
                    else
                        ++nEolCnt;
                };
                m_nEolCnt = nEolCnt;
            }
            break;

        case SYSTEMSTATE_STOP:
            {
                for (int32 i = 0; i < m_nWorkpoolRunCnt; ++i)
                    m_pWorkpool[i].Stop();

				m_Systemstate = SYSTEMSTATE_EOL;
            }
            break;

        default:
            ASSERT(false);
            break;
        };
    };

    virtual bool IsEol(void) const
    {
        return (m_nEolCnt == m_nWorkpoolSize);
    };

    inline void SetRunNum(int32 Num)
    {
        ASSERT(Num >= 0 && Num <= m_nWorkpoolSize);
        
        m_nWorkpoolRunCnt = (Num == -1 ? m_nWorkpoolSize : Num);
        m_nWorkpoolRunCnt = Clamp(m_nWorkpoolRunCnt, 0, m_nWorkpoolSize);

        for (int32 i = m_nStartedCnt; i > m_nWorkpoolRunCnt; --i)
            m_pWorkpool[i - 1].Stop();
    };

    inline int32 GetRunNum(void) const
    {
        return (m_nWorkpoolSize - m_nEolCnt);
    };

    inline int32 GetEolNum(void) const
    {
        return m_nEolCnt;
    };

    inline SYSTEMSTATE GetSystemState(void) const
    {
        return m_Systemstate;
    };

    inline int32 GetWorkpoolSize(void) const
    {
        return m_nWorkpoolSize;
    };

    inline T& GetObj(int32 nIndex)
    {
        ASSERT(nIndex >= 0 && nIndex < m_nWorkpoolSize);
        return *&m_pWorkpool[nIndex];
    };

private:
    T* m_pWorkpool;
    int32 m_nWorkpoolSize;
    int32 m_nWorkpoolRunCnt;
    SYSTEMSTATE m_Systemstate;
    int32 m_nEolCnt;
    int32 m_nStartedCnt;
};