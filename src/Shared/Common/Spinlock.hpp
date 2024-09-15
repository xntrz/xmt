#pragma once


class CSpinlock
{
public:
    struct unique_lock : public std::unique_lock<CSpinlock> {};

public:
    inline CSpinlock()
    : m_flag() {
        ;
    };

    inline void lock() {
        while (m_flag.test_and_set(std::memory_order_acquire));
    };

    inline void unlock() {
        m_flag.clear(std::memory_order_release);
    };

private:
    std::atomic_flag m_flag;
};
