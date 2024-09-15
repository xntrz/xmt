#include "Thread.hpp"
#include "Spinlock.hpp"
#include "List.hpp"


thread_local char ThreadCurrentName[256];
thread_local uint32 ThreadCurrentID = 0;
thread_local uint32 ThreadCurrentAppID = 0;


class CThreadContrainer final
{
private:
    struct NODE : public CListNode<NODE>
    {
        uint32 tid;

        inline NODE()
            : tid(0) {};
    };

public:
    CThreadContrainer();
    ~CThreadContrainer();
    uint32 Regist();
    void Remove(uint32 idx);

private:
    NODE m_aNodes[256];
    CList<NODE> m_listFree;
    CList<NODE> m_listAlloc;
    CSpinlock m_mutex;
};


CThreadContrainer::CThreadContrainer()
: m_aNodes()
, m_listFree()
, m_listAlloc()
{
    for (int32 i = 0; i < COUNT_OF(m_aNodes); ++i)
        m_listFree.push_back(&m_aNodes[i]);
};


CThreadContrainer::~CThreadContrainer()
{
    ASSERT(m_listAlloc.empty() == true);
};


uint32 CThreadContrainer::Regist()
{
    std::unique_lock<CSpinlock> lock(m_mutex);

    if (m_listFree.empty())
        return -1;

    NODE* n = m_listFree.front();
    ASSERT(n->tid == 0);
    n->tid = ThreadCurrentID;
    m_listFree.erase(n);
    m_listAlloc.push_back(n);

    /* return index of node as application thread id */
    uint32 idx = uint32(std::ptrdiff_t(n - &m_aNodes[0]));
    idx += 1;

    return idx;
};


void CThreadContrainer::Remove(uint32 idx)
{
    ASSERT(idx > 0);
    idx -= 1;

    ASSERT(idx >= 0);
    ASSERT(idx < COUNT_OF(m_aNodes));

    std::unique_lock<CSpinlock> lock(m_mutex);

    NODE* n = &m_aNodes[idx];
    ASSERT(n->tid == ThreadCurrentID);
    n->tid = 0;
    m_listAlloc.erase(n);
    m_listFree.push_back(n);
};


static CThreadContrainer s_ThreadContainer;


static inline CThreadContrainer& ThreadContainer()
{
    return s_ThreadContainer;
};


namespace thread
{
    void initialize()
    {
        ;
    };

    void terminate()
    {
        ;
    };

    /*DLLSHARED*/ void regist_current(const char* pszName)
    {
        /* init system thread id */
        ThreadCurrentID = uint32(GetCurrentThreadId());

        /* init thread name */
        ASSERT(std::strlen(pszName) < COUNT_OF(ThreadCurrentName));
        std::strcpy(ThreadCurrentName, pszName);

        /* init native thread name */
#define MS_VC_EXCEPTION 0x406D1388
#pragma pack(push,8)
        typedef struct {
            DWORD    dwType;     // Must be 0x1000.
            LPCSTR   szName;     // Pointer to name (in user addr space).
            DWORD    dwThreadID; // Thread ID (-1=caller thread).
            DWORD    dwFlags;    // Reserved for future use, must be zero.
        } THREADNAME_INFO;
#pragma pack(pop)

        THREADNAME_INFO ThreadNameInfo = {};
        ThreadNameInfo.dwType = 0x1000;
        ThreadNameInfo.szName = ThreadCurrentName;
        ThreadNameInfo.dwThreadID = ThreadCurrentID;
        ThreadNameInfo.dwFlags = 0;

        __try
        {
            RaiseException(
                MS_VC_EXCEPTION,
                0,
                sizeof(ThreadNameInfo) / sizeof(ULONG_PTR),
                PULONG_PTR(&ThreadNameInfo)
            );
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ;
        };

        /* init application thread id */
        ThreadCurrentAppID = ThreadContainer().Regist();
    };

    /*DLLSHARED*/ void remove_current()
    {
        /* term app thread id */
        ThreadContainer().Remove(ThreadCurrentAppID);
        ThreadCurrentAppID = 0;
    };

    /*DLLSHARED*/ const char* current_name()
    {
        return ThreadCurrentName;
    };

    /*DLLSHARED*/ uint32 current_id()
    {
        return ThreadCurrentID;
    };

    /*DLLSHARED*/ uint32 current_app_id()
    {
        return ThreadCurrentAppID;
    };
};