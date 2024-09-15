#pragma once


class CProxyObjSeq
{
public:
    inline CProxyObjSeq()
        : m_init(0u), m_current(m_init), m_begin(0u), m_end(0u) {};

    inline CProxyObjSeq(uint32 begin, uint32 end)
        : m_init(0u), m_current(m_init), m_begin(begin), m_end(end) {};

    inline CProxyObjSeq(uint32 begin, uint32 end, uint32 init)
        : m_init(init), m_current(m_init), m_begin(begin), m_end(end) {};

    inline void StrideNext() {
        m_current = (m_current + 1 > m_end ? m_end : m_current + 1);
    };

    inline void StridePrev() {
        m_current = (m_current > m_begin ? m_current - 1 : m_begin);
    };

    inline void Reset() {
        m_current = m_init;
    };
    
    inline bool IsEnded() const {
        return (m_current == m_end);
    };

    inline uint32 Current() const {
        return m_current;
    };

private:
    uint32 m_init;
    uint32 m_current;
    uint32 m_begin;
    uint32 m_end;
};