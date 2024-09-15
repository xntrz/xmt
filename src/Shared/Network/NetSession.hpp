#pragma once

#include "NetTypes.hpp"

#include "Shared/Common/Spinlock.hpp"


class CNetLayer;


class CNetSession
{
protected:
    struct event_proc_args {
        NETEVENT    event;
        uint32      errcode;
        uint64      netaddr;
        const void* data;
        uint32      size;
        bool        dispatch;

        inline event_proc_args()
            : event(NETEVENT(-1)), errcode(0), netaddr(0), data(nullptr), size(0), dispatch(false) {};

        inline event_proc_args(NETEVENT _event, uint32 _errcode, uint64 _netaddr, const void* _data, uint32 _size, bool _dispatch)
            : event(_event), errcode(_errcode), netaddr(_netaddr), data(_data), size(_size), dispatch(_dispatch) {};
    };

    static const int32 receiver_label_all = -1;
    
public:
    CNetSession(CNetSession& parent);
    CNetSession(NETEVENTPROC eventProc, void* eventProcParam);
    virtual ~CNetSession();
    virtual bool OnPreDispatchUserProc(int32 id, void* param, event_proc_args& args) = 0;
    virtual void OnUserRefEnd() = 0;
    virtual void OnRefEnd() = 0;

    void Close();
    CNetSession* Copy();
    void UserRefInc();
    void UserRefDec();
    void LayerRefInc();
    void LayerRefDec();
    void LayerAttach(int32 label, std::shared_ptr<CNetLayer> layer);
    std::shared_ptr<CNetLayer> LayerDetach(int32 label);
    bool LayerIsAttached(int32 label);
    bool LayerIsAnyAttached(const std::vector<int32>& labels);
    bool SendEvent(std::shared_ptr<CNetLayer> sender, int32 id, void* param = nullptr);
    bool SendCmd(int32 id, void* param = nullptr);
    bool SendCmd(int32 label, int32 id, void* param = nullptr);
    bool SendCmd(std::shared_ptr<CNetLayer> sender, int32 id, void* param = nullptr);
    bool SendCmd(std::shared_ptr<CNetLayer> sender, int32 label, int32 id, void* param = nullptr);
    
    /*inline*/ void SetEventProc(NETEVENTPROC eventProc, void* eventProcParam = nullptr);
    /*inline*/ void GetEventProc(NETEVENTPROC& eventProc, void*& eventProcParam);
    /*inline*/ void SetUserParam(void* usrParam);
    /*inline*/ void* GetUserParam();

private:
    void refInc();
    void refDec();

private:
    std::list<std::shared_ptr<CNetLayer>> m_listLayers;
    std::atomic<int16> m_refTotal;
    std::atomic<int16> m_refUser;
    std::atomic<void*> m_usrParam;
    NETEVENTPROC m_eventProc;
    void* m_eventProcParam;
    CSpinlock m_eventProcMutex;
};

inline void CNetSession::SetEventProc(NETEVENTPROC eventProc, void* eventProcParam /*= nullptr*/){
    std::unique_lock<CSpinlock> lock(m_eventProcMutex);
    m_eventProc = eventProc;
    m_eventProcParam = eventProcParam;
};

inline void CNetSession::GetEventProc(NETEVENTPROC& eventProc, void*& eventProcParam) {
    std::unique_lock<CSpinlock> lock(m_eventProcMutex);
    eventProc = m_eventProc;
    eventProcParam = m_eventProcParam;
};

inline void CNetSession::SetUserParam(void* usrParam) {
    m_usrParam = usrParam;
};

inline void* CNetSession::GetUserParam() {
    return m_usrParam;
};
