#pragma once


class CEvent
{
public:
    inline CEvent()
    : m_mutex()
    , m_cv()
    , m_flag(false) {
        ;
    };

    inline void reset() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_flag = false;
    };

    inline void signal_once() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_flag = true;
        m_cv.notify_one();
    };

    inline void signal_all() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_flag = true;
        m_cv.notify_all();
    };

    inline void wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock);
        m_flag = false;
    };

    template<class predicate>
    inline void wait(predicate pred) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, pred);
        m_flag = false;
    };

    template<class rep, class period>
    inline bool wait_for(std::chrono::duration<rep, period> duration) {
        return wait_for(duration, [ this ](void) {
            return m_flag.load();
        });
    };

    template<class rep, class period, class predicate>
    inline bool wait_for(std::chrono::duration<rep, period> duration, predicate pred) {
        std::unique_lock<std::mutex> lock(m_mutex);
        bool ret = m_cv.wait_for(lock, duration, pred);
        if (ret)
            m_flag = false;
        return ret;
    };

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_flag;
};