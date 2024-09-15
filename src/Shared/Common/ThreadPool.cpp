#include "ThreadPool.hpp"
#include "Thread.hpp"



/*static*/ CThreadPool* CThreadPool::ms_pDefault = nullptr;


/*DLLSHARED static*/ CThreadPool& CThreadPool::default()
{
    ASSERT(ms_pDefault != nullptr);
    return *ms_pDefault;
};


/*DLLSHARED*/ CThreadPool::CThreadPool(std::size_t numThreads)
: m_threads()
, m_queueMutex()
, m_queueWork()
, m_event()
, m_bStop(false)
{
    const std::size_t numThreadsMin = 1;
    m_threads.resize(std::max(numThreads, numThreadsMin));

    std::for_each(m_threads.begin(), m_threads.end(), [ = ](std::thread& thread) {
        std::ptrdiff_t threadIndex = (&thread - &m_threads[0]);
        ASSERT(threadIndex >= 0);
        thread = std::move(thread::spawn("ThreadPoolWorker_" + std::to_string(threadIndex), [ this ]() {
            while (true)
            {
                std::function<void()> task;
                {
                    m_event.wait([this]() {
                        return !m_queueWork.empty() || m_bStop;
                    });
                    
                    if (m_bStop)
                        return;
                    
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    if (m_queueWork.empty())
                        continue;

                    task = m_queueWork.front();
                    m_queueWork.pop_front();
                }
                task();
            };
        }));
    });

    

    if (ms_pDefault == nullptr)
        ms_pDefault = this;
};


/*DLLSHARED*/ CThreadPool::~CThreadPool()
{
    if (ms_pDefault == this)
        ms_pDefault = nullptr;

    m_bStop = true;
    m_event.signal_all();
    std::for_each(m_threads.begin(), m_threads.end(), [ = ](std::thread& thread) {
        if (thread.joinable())
            thread.join();
    });
};