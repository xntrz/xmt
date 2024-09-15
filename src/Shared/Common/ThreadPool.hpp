#pragma once

#include "Event.hpp"


namespace thread
{
    template<class F, class... Args>
    inline std::future<typename std::result_of<F(Args...)>::type> async(F&& f, Args&&... args)
    {
        return CThreadPool::default().post(std::forward<F&&>(f), std::forward<Args&&>(args)...);
    };
};


/**
 *  Simple thread pool with task queue
 *
 *  This can be made in asio IoService but dont want to include tons of public asio headers
 *  across whole project or modules for just simple async task
 */
class CThreadPool
{
public:
    DLLSHARED static CThreadPool& default();
    
	DLLSHARED CThreadPool(std::size_t numThreads);
	DLLSHARED ~CThreadPool();

    template <bool v>
    struct post_traits;

    /* post traits for non void promises */
    template <>
    struct post_traits<false>
    {
        template<typename Ret, class F, class... Args>
        static /*inline*/ void call(std::shared_ptr<std::promise<Ret>> promise, F fn, std::tuple<Args&&...> args);
    };

    /* post traits for void promises */
    template <>
    struct post_traits<true>
    {
        template<typename Ret, class F, class... Args>
        static /*inline*/ void call(std::shared_ptr<std::promise<Ret>> promise, F fn, std::tuple<Args&&...> args);
    };

    /* post fn */
    template<class F, class... Args>
    /*inline*/ std::future<typename std::result_of<F(Args...)>::type> post(F f, Args&&... args);

private:
    std::vector<std::thread> m_threads;
    std::mutex m_queueMutex;
    std::deque<std::function<void()>> m_queueWork;
    CEvent m_event;
    std::atomic<bool> m_bStop;
    static CThreadPool* ms_pDefault;
};


template<typename Ret, class F, class... Args>
/*static*/ inline void CThreadPool::post_traits<false>::call(std::shared_ptr<std::promise<Ret>> promise, F fn, std::tuple<Args&&...> args)
{
    promise->set_value(fn(std::forward<Args>(args) ...));
};


template<typename Ret, class F, class... Args>
/*static*/ inline void CThreadPool::post_traits<true>::call(std::shared_ptr<std::promise<Ret>> promise, F fn, std::tuple<Args&&...> args)
{
    fn(std::forward<Args>(args) ...);
    promise->set_value();
};


template<class F, class... Args>
inline std::future<typename std::result_of<F(Args...)>::type> CThreadPool::post(F f, Args&&... args)
{
    using ret_type = std::result_of_t<F(Args&&...)>;

    std::shared_ptr<std::promise<ret_type>> promise = std::make_shared<std::promise<ret_type>>();
    std::future<ret_type> future = promise->get_future();

    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueWork.push_back([ promise = std::move(promise), f = std::forward<F>(f), args = std::make_tuple(std::forward<Args&&>(args)...) ] {
            post_traits<std::is_void<ret_type>::value>::call<ret_type, F, Args...>(promise, f, args);
        });
    }

    m_event.signal_once();

    return future;
};