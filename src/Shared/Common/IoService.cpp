#include "IoService.hpp"


CIoService::CIoService(
    std::size_t         threadsCnt /*= std::thread::hardware_concurrency()*/, 
    const std::string&  threadsTag /*= "IoSvc"*/
)
: m_ioctx(nullptr)
, m_iowork(nullptr)
, m_threads()
, m_threadsAlive(0u)
, m_evtStop()
, m_evtCleanup()
, m_cbExit(nullptr)
{
    m_ioctx = new asio::io_context;
    m_iowork = new asio::io_context::work(*m_ioctx);

    const std::size_t threadsCntMin = 1;
    m_threads.resize(std::max(threadsCnt, threadsCntMin));
    
    std::for_each(m_threads.begin(), m_threads.end(), [ = ](std::thread& thread) {
        std::ptrdiff_t threadIndex = (&thread - &m_threads[0]);
        thread = std::move(thread::spawn(threadsTag + "_" + std::to_string(threadIndex), [this]() {
            ++m_threadsAlive;

            asio::error_code errcode;
            m_ioctx->run(errcode);
            if (errcode)
            {
#ifdef _DEBUG            
                OUTPUTLN(
                    "thread: %s (id: %" PRIu32 ") -- %s",
                    thread::current_name(),
                    thread::current_id(),
                    errcode.message().c_str()
                );
#else
                DbgFatal(
                    "io service fatal error: %s\n"
                    "thread name: %s\n"
                    "thread id: %" PRIu32,
                    errcode.message().c_str(),
                    thread::current_name(),
                    thread::current_id()
                );
#endif                
            };

            if (m_cbExit)
                m_cbExit();

            if (--m_threadsAlive == 0)
                m_evtStop.signal_all();

            m_evtCleanup.wait();
        }));
    });
};


CIoService::~CIoService()
{
    ASSERT(m_threadsAlive == 0u);
    ASSERT(m_ioctx->stopped() == true);

    if (m_iowork)
    {
        delete m_iowork;
        m_iowork = nullptr;
    };

    if (m_ioctx)
    {
        delete m_ioctx;
        m_ioctx = nullptr;
    };

    m_evtCleanup.signal_all();

    std::for_each(m_threads.begin(), m_threads.end(), [this](std::thread& thread) {
        if (thread.joinable())
            thread.join();
    });

    m_threads.clear();
};