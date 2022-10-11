#include "Timer.hpp"

#include "Shared/Common/Time.hpp"


static inline uint32 ClockStampSub(void)
{
    //return TimeCurrentTickPrecise();
    return TimeCurrentTick();
};


CTimer::CTimer(void)
: m_uStart(0)
, m_uNow(0)
, m_uAlarm(0)
{
    ;
};


CTimer::~CTimer(void)
{
    ;
};


void CTimer::Period(void)
{
    m_uNow = ClockStampSub();
};


uint32 CTimer::Reset(void)
{
    uint32 uNow = ClockStampSub();
    uint32 uElapsed = uNow - m_uStart;
    m_uStart = uNow;
    return uElapsed;
};


void CTimer::SetAlarm(uint32 Ms)
{
    m_uAlarm = Ms;
};


bool CTimer::IsAlarm(void) const
{
    return ((m_uNow - m_uStart) >= m_uAlarm);
};