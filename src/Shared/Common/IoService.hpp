#pragma once

#include "Event.hpp"
#include "Thread.hpp"


#define ASIO_STANDALONE 
#define ASIO_NO_EXCEPTIONS
#define ASIO_NO_TYPEID
#define ASIO_HAS_WIN_IOCP_EX
#define ASIO_WIN_IOCP_EX_ENTRIES (16)


#pragma push_macro("new")
#pragma push_macro("delete")
#undef new
#undef delete
#include "asio.hpp"
#pragma pop_macro("delete")
#pragma pop_macro("new")


namespace asio
{
    namespace detail
    {
        template <typename Exception>
        inline void throw_exception(const Exception& e)
        {
            DbgFatal("asio exception");
        };
    };
};


class CIoService final
{
public:
    using thread_exit_cb = std::function<void()>;
    
public:
    CIoService(std::size_t threadsCnt = std::thread::hardware_concurrency(), const std::string& threadsTag = "IoSvc");
    ~CIoService();

    template<class rep, class period>
    inline void stop_and_wait(std::chrono::duration<rep, period> duration)
    {
        m_ioctx->stop();
        
        bool ret = m_evtStop.wait_for(duration, [this]() {
            return (m_threadsAlive == 0u);
        });

        ASSERT(ret, "wait for stop failed - threads remain: %d", m_threadsAlive.load());
    };

    //template<class F, class... Args>
    //inline std::future<typename std::result_of<F(Args...)>::type> post(F&& f, Args&&... args)
    //{
    //    using ret_type = std::result_of_t<F(Args&&...)>;
    //    std::shared_ptr<std::promise<ret_type>> promise = std::make_shared<std::promise<ret_type>>();
    //    std::future<ret_type> future = promise->get_future();
    //    m_ioctx->post([ promise = std::move(promise),
    //                    f = std::forward<F>(f),
    //                    args = std::make_tuple(std::forward<Args>(args)...) ] {
    //        promise->set_value(f(std::forward<Args>(args) ...));
    //    });    
    //    return future;
    //};

    inline asio::io_context& ctx(){ return *m_ioctx; };
    inline void on_thread_exit(thread_exit_cb cb) { m_cbExit = cb; };

public:
    asio::io_context* m_ioctx;
    asio::io_context::work* m_iowork;
    std::vector<std::thread> m_threads;
    std::atomic<std::size_t> m_threadsAlive;
    CEvent m_evtStop;
    CEvent m_evtCleanup;
    thread_exit_cb m_cbExit;
};