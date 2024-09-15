#include "module_obj.hpp"

#include "Shared/Common/Spinlock.hpp"
#include "Shared/Common/Event.hpp"


struct module_obj_container
{
public:
    inline module_obj_container()
    : m_listObj()
    , m_listMutex()
    , m_refCnt(0)
    , m_evtRefEnd() {};

    inline void wait_for_refs_end()
    {
        if (m_refCnt > 0)
        {
            if (!m_evtRefEnd.wait_for(std::chrono::seconds(5)))
            {
                for (auto& it : m_listObj)
                    OUTPUTLN("unremoved obj %s", it.m_tag.c_str());
                ASSERT(false);
            };
        };
    };

    inline void regist(module_obj* obj)
    {
        {
            std::unique_lock<CSpinlock> lock(m_listMutex);
            m_listObj.push_back(obj);
        }

        ++m_refCnt;
    };

    inline void remove(module_obj* obj)
    {
        {
            std::unique_lock<CSpinlock> lock(m_listMutex);
            m_listObj.erase(obj);
        }

        if (!--m_refCnt)
            m_evtRefEnd.signal_all();
    };

private:
    CList<module_obj> m_listObj;
    CSpinlock m_listMutex;
    std::atomic<int32> m_refCnt;
    CEvent m_evtRefEnd;
};


static module_obj_container* s_pContainer = nullptr;


void module_obj_init()
{
    ASSERT(s_pContainer == nullptr);
    s_pContainer = new module_obj_container;
};


void module_obj_term()
{
    ASSERT(s_pContainer != nullptr);
    delete s_pContainer;
    s_pContainer = nullptr;
};


void module_obj_wait_removes()
{
    s_pContainer->wait_for_refs_end();
};


void module_obj_regist(module_obj* obj)
{
    s_pContainer->regist(obj);
};


void module_obj_remove(module_obj* obj)
{
    s_pContainer->remove(obj);
};