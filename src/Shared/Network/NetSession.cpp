#include "NetSession.hpp"

#include "NetLayer.hpp"


CNetSession::CNetSession(CNetSession& parent)
: m_listLayers()
, m_refTotal()
, m_refUser(0)
, m_usrParam(0)
, m_eventProc(parent.m_eventProc)
, m_eventProcParam(parent.m_eventProcParam)
, m_eventProcMutex()
{
    ;
};


CNetSession::CNetSession(NETEVENTPROC eventProc, void* eventProcParam)
: m_listLayers()
, m_refTotal(0)
, m_refUser(0)
, m_usrParam(0)
, m_eventProc(eventProc)
, m_eventProcParam(eventProcParam)
, m_eventProcMutex()
{
    ;
};


CNetSession::~CNetSession()
{
    ;
};


void CNetSession::Close()
{
    UserRefDec();
};


CNetSession* CNetSession::Copy()
{
    UserRefInc();
    return this;
};


void CNetSession::UserRefInc()
{
    if (!m_refUser++)
        refInc();
};


void CNetSession::UserRefDec()
{
    ASSERT(m_refUser > 0);
    if (!--m_refUser) {
        SetEventProc(nullptr, nullptr);
        OnUserRefEnd();
        refDec();
    };
};


void CNetSession::LayerRefInc()
{
    refInc();
};


void CNetSession::LayerRefDec()
{
    refDec();
};


void CNetSession::LayerAttach(int32 label, std::shared_ptr<CNetLayer> layer)
{
    auto it = m_listLayers.begin();
    while (it != m_listLayers.end()) 
    {
        if ((*it)->Label() < layer->Label())
            break;

		++it;
    };
    
    layer->node = m_listLayers.insert(it, layer);
};


std::shared_ptr<CNetLayer> CNetSession::LayerDetach(int32 label)
{
    std::shared_ptr<CNetLayer> layer;

    auto it = m_listLayers.begin();
    while (it != m_listLayers.end()) 
    {
        if ((*it)->Label() == label) 
        {
            layer = (*it);
            m_listLayers.erase(it);
            break;
        };

        ++it;
    };

    return layer;
};


bool CNetSession::LayerIsAttached(int32 label)
{
    auto it = m_listLayers.begin();
    while (it != m_listLayers.end()) 
    {
        if ((*it)->Label() == label) 
            return true;

        ++it;
    };

    return false;
};


bool CNetSession::LayerIsAnyAttached(const std::vector<int32>& labels)
{
    auto it = m_listLayers.begin();
    while (it != m_listLayers.end()) 
    {
        for (auto label : labels) 
        {
            if ((*it)->Label() == label)
                return true;
        };

        ++it;
    };

    return false;
};


bool CNetSession::SendEvent(std::shared_ptr<CNetLayer> sender, int32 id, void* param /*= nullptr*/)
{
    if (m_listLayers.empty())
        return false;

    /* dispatch event across all layers */
    std::list<std::shared_ptr<CNetLayer>>::iterator it(sender ? sender->node : m_listLayers.end());
    if (it != m_listLayers.begin()) {
        /* send event to an following layer */
        --it;
    }
    else {
        /* layer is a first in chain */
        it = m_listLayers.end();
    };

    if (it != m_listLayers.end())
        return (*it)->EventProc(id, param);

    /* now convert specific layer event to common net event */
    event_proc_args args;
    bool bResult = OnPreDispatchUserProc(id, param, args);
    if (!args.dispatch)
        return bResult;

    /* now dispatch to user proc */
    NETEVENTPROC evproc;
    void* evparam = nullptr;    
    GetEventProc(evproc, evparam);
    if (evproc)
        return evproc(this, args.event, args.errcode, args.netaddr, args.data, args.size, evparam);

    return false;
};


bool CNetSession::SendCmd(int32 id, void* param /*= nullptr*/)
{
    return SendCmd(nullptr, receiver_label_all, id, param);
};


bool CNetSession::SendCmd(int32 label, int32 id, void* param /*= nullptr*/)
{
    return SendCmd(nullptr, label, id, param);
};


bool CNetSession::SendCmd(std::shared_ptr<CNetLayer> sender, int32 id, void* param /*= nullptr*/)
{
    return SendCmd(sender, receiver_label_all, id, param);
};


bool CNetSession::SendCmd(std::shared_ptr<CNetLayer> sender, int32 label, int32 id, void* param /*= nullptr*/)
{
    if (m_listLayers.empty())
        return false;

    std::list<std::shared_ptr<CNetLayer>>::iterator it(sender ? sender->node : m_listLayers.begin());
	if (sender)
		++it;

    while (it != m_listLayers.end()) {
        if ((label == (*it)->Label()) || (label == receiver_label_all))
            return (*it)->CmdProc(id, param);
        ++it;
    };

    return true;
};


void CNetSession::refInc()
{
    ++m_refTotal;
};


void CNetSession::refDec()
{
    ASSERT(m_refTotal > 0);
    if (!--m_refTotal) {
        ASSERT(m_refUser == 0);
        OnRefEnd();
    };
};