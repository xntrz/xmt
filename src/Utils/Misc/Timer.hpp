#pragma once


class CTimer
{
public:
    CTimer(void);
    ~CTimer(void);
    void Period(void);
    uint32 Reset(void);
    void SetAlarm(uint32 Ms);
    bool IsAlarm(void) const;

private:
    uint32 m_uStart;
    uint32 m_uNow;
    uint32 m_uAlarm;
};