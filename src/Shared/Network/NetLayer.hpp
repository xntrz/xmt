#pragma once

#include "NetSession.hpp"


class CNetLayer : public std::enable_shared_from_this<CNetLayer>
{
public:
    std::list<std::shared_ptr<CNetLayer>>::iterator node;

public:
    virtual bool EventProc(int32 id, void* param) = 0;
    virtual bool CmdProc(int32 id, void* param) = 0;
    
    /*inline*/ CNetLayer(CNetSession& session, int32 label);    
    /*inline*/ virtual ~CNetLayer();    
    /*inline*/ bool SendEvent(int32 id, void* param);    
    /*inline*/ bool SendCmd(int32 id, void* param);
    /*inline*/ int32 Label() const;
    /*inline*/ CNetSession& Session() const;

private:
    CNetSession& m_session;
    int32 m_label;
};

inline CNetLayer::CNetLayer(CNetSession& session, int32 label)
: m_session(session)
, m_label(label) {
    ;
};

inline CNetLayer::~CNetLayer() {
    ;
};

inline bool CNetLayer::SendEvent(int32 id, void* param) {
    return Session().SendEvent(shared_from_this(), id, param);
};

inline bool CNetLayer::SendCmd(int32 id, void* param) {
    return Session().SendCmd(shared_from_this(), id, param);
};

inline int32 CNetLayer::Label() const {
    return m_label;
};

inline CNetSession& CNetLayer::Session() const {
    return m_session;
};
