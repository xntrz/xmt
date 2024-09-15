#pragma once

#include "../NetLayer.hpp"

#include "TcpSession.hpp"


class CIoService;


class CTcpLayer : public CNetLayer
{
public:
    enum evttype {
        evttype_accept = 0,
        evttype_connect,
        evttype_connect_fail,
        evttype_read,
        evttype_write,
        evttype_dc,

        evttype_extend = 100,
    };

    struct evt_accept {
        CTcpSession* session;

        inline evt_accept(CTcpSession* _session)
            : session(_session) {};
    };

    struct evt_connect {
        uint32 ip;
        uint16 port;
        uint32 elapsed;
        bool remote;

        inline evt_connect(uint32 _ip, uint16 _port, uint32 _elapsed = 0, bool _remote = false)
            : ip(_ip), port(_port), elapsed(_elapsed), remote(_remote) {};
    };

    struct evt_connect_fail {
        uint32 ip;
        uint16 port;
        uint32 errcode;
        int32 label;    /* specifies layer label where connection failure happens */

        inline evt_connect_fail(uint32 _ip, uint16 _port, uint32 _errcode, int32 _label)
            : ip(_ip), port(_port), errcode(_errcode), label(_label) {};
    };

    struct evt_write {
        const void* data;
        std::size_t bytes;

        inline evt_write(const void* _data, uint32 _bytes)
            : data(_data), bytes(_bytes) {};
    };

    struct evt_read {
        void* buffer;
        std::size_t bytes;

        inline evt_read(void* _buffer, std::size_t _bytes)
            : buffer(_buffer), bytes(_bytes) {};
    };

    struct evt_dc {
        bool remote;

        inline evt_dc(bool _remote)
            : remote(_remote) {};
    };

    enum cmdtype {
        cmdtype_connect = 0,
        cmdtype_cancel_connect,
        cmdtype_listen,
        cmdtype_write,
        cmdtype_dc,

        cmdtype_extend = 100,
    };

    struct cmd_connect {
        uint32 ip;
        uint16 port;
        uint32 timeout;

        inline cmd_connect(uint32 _ip, uint16 _port, uint32 _timeout)
            : ip(_ip), port(_port), timeout(_timeout) {};
    };

    struct cmd_cancel_connect {    
        inline cmd_cancel_connect() {};
    };

    struct cmd_listen {
        uint32 ip;
        uint16 port;
        int32 backlog;

        inline cmd_listen(uint32 _ip, uint16 _port, int32 _backlog)
            : ip(_ip), port(_port), backlog(_backlog) {};
    };

    struct cmd_write {
        const void* data;
        std::size_t size;
        std::size_t written;
        std::size_t chunks;     /* counter of how much it was sliced for transport */
        bool doc;

        inline cmd_write(const void* _data, std::size_t _size, bool _doc = false)
            : data(_data), size(_size), written(0u), chunks(0u), doc(_doc) {};
    };

    struct cmd_dc {
        inline cmd_dc() {};
    };

public:
    /*inline*/ CTcpLayer(CTcpSession& session, int32 label);
    /*inline*/ virtual ~CTcpLayer();
    /*inline*/ void SessionRefInc();
    /*inline*/ void SessionRefDec();
    /*inline*/ bool IsRemote() const;
    /*inline*/ CTcpSession* OpenRemote(void* cmd);
    /*inline*/ CTcpSession& TcpSession();
    /*inline*/ CTcpSession& TcpSession() const;    
    /*inline*/ CIoService& IoService();
    
private:
    CIoService& m_iosvc;
};

inline CTcpLayer::CTcpLayer(CTcpSession& session, int32 label)
: CNetLayer(session, label)
, m_iosvc(session.GetIoService()) {
    ;
};

inline CTcpLayer::~CTcpLayer(){
    ;
};

inline void CTcpLayer::SessionRefInc() {
    TcpSession().LayerRefInc();
};

inline void CTcpLayer::SessionRefDec() {
    TcpSession().LayerRefDec();
};

inline bool CTcpLayer::IsRemote() const {
    return TcpSession().IsRemote();
};

inline CTcpSession* CTcpLayer::OpenRemote(void* cmd) {
    return new CTcpSession(m_iosvc, TcpSession(), cmd);
};

inline CTcpSession& CTcpLayer::TcpSession(){
    return static_cast<CTcpSession&>(Session());
};

inline CTcpSession& CTcpLayer::TcpSession() const {
    return static_cast<CTcpSession&>(Session());
};

inline CIoService& CTcpLayer::IoService() {
    return m_iosvc;
};