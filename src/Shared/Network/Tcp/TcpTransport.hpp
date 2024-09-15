#pragma once

#include "TcpTypedefs.hpp"
#include "TcpLayer.hpp"
#include "TcpTransportAct.hpp"
#include "TcpNetwork.hpp"


class CTcpTransport final : public CTcpLayer
{
public:
    static const std::size_t opid_all = std::numeric_limits<std::size_t>::max();

    enum evttype {
        evttype_resolve = evttype_extend,
    };

    struct evt_resolve {
        const std::vector<uint32>& ips;
        uint32 errcode;

        inline evt_resolve(const std::vector<uint32>& _ips, uint32 _errcode)
            : ips(_ips), errcode(_errcode) {};
    };

    enum cmdtype {
        cmdtype_resolve = cmdtype_extend,
        cmdtype_cancel_resolve,
        cmdtype_complete_accept,
        cmdtype_check_open,
        cmdtype_close,
        cmdtype_set_read_timeout,
        cmdtype_set_force_close,
    };

    struct cmd_resolve {
        std::string host;
        uint32 timeout;
        
        inline cmd_resolve(const std::string& _host, uint32 _timeout)
            : host(_host), timeout(_timeout) {};
    };

    struct cmd_cancel_resolve {
        std::size_t opid;
        
        inline cmd_cancel_resolve(std::size_t _opid = opid_all) : opid(_opid) {};
    };

    struct cmd_complete_accept {
        asio::ip::tcp::socket& sock;
        uint16 flags;
        
        inline cmd_complete_accept(asio::ip::tcp::socket& _sock, uint16 _flags)
            : sock(_sock), flags(_flags) {};
    };

    struct cmd_check_open {
        inline cmd_check_open() {};
    };

    struct cmd_close {
        inline cmd_close() {};
    };

    struct cmd_set_param {
        union {
            bool boolean;
            uint32 integer;
        };
        
        inline cmd_set_param(uint32 _value)
            : integer(_value) {};

        inline cmd_set_param(bool _value)
            : boolean(_value) {};
    };

private:
    struct resolver_ctx;
    struct acceptor_ctx;

    enum flag : uint16
    {        
        /* control flags */
        flag_ctrl_forceclose    = (1 << 0),
        flag_ctrl_nodelay       = (1 << 1),
        flag_ctrl_mask          = flag_ctrl_forceclose
                                | flag_ctrl_nodelay,

        /* state flags */
        flag_st_connecting      = (1 << 8),
        flag_st_connected       = (1 << 9),     
        flag_st_mask            = flag_st_connecting
                                | flag_st_connected,
    };

public:
    CTcpTransport(CTcpSession& session, int32 label);
    virtual ~CTcpTransport();
    virtual bool EventProc(int32 id, void* param) override;
    virtual bool CmdProc(int32 id, void* param) override;

    void IoCompletionProc(const asio::error_code& errcode, std::size_t bytes, CTcpAct& act);
    void WaitIoCompletionProc(const asio::error_code& errcode, std::size_t bytes, CTcpAct& act);
    void TimeoutProc(const asio::error_code& errcode, CTcpAct& act);

    bool Connect(uint32 ip, uint16 port, std::chrono::milliseconds timeout);
    bool CancelConnect();
    bool Listen(uint32 ip, uint16 port, int32 backlog);
    std::size_t Write(const void* data, std::size_t size, bool doc);
    bool Disconnect();
    bool Resolve(const std::string& hostname, std::chrono::milliseconds timeout);
    bool CancelResolve(std::size_t opid);

    void StartAccept(CTcpActAccept& act);
    void StartConnect(uint32 ip, uint16 port, std::chrono::milliseconds timeout);
    void StartRead();
    void StartWrite();
    void StartDisconnect();
    void StartTimeout(CTcpAct& act, std::chrono::milliseconds timeout);

    void CompleteAccept(CTcpActAccept& act, const asio::error_code& errcode);
    void CompleteConnect(asio::ip::tcp::socket& socket, uint16 flags);
    void CompleteConnect(CTcpActConnect& act, const asio::error_code& errcode);
    void CompleteRead(CTcpActRead& act, std::size_t bytes, const asio::error_code& errcode);
    void CompleteWrite(CTcpActWrite& act, std::size_t bytes, const asio::error_code& errcode);
    void CompleteDisconnect(CTcpActDisconnect& act, const asio::error_code& errcode);

    std::size_t CreateSendNode(const void* data, std::size_t size, bool doc);
    bool IsOpen();
    void Close();
    bool SetSockOptBeforeConnect();
    bool SetSockOptAfterConnect();
    bool OpenSockForConnect();
    bool OpenSockForListen(uint32 ip, uint16 port, int32 backlogHint);
    void CleanupPerConnect();
    void AsioEndpointToNetAddr(asio::ip::tcp::endpoint& endpoint, uint64* netaddr) const;
    void ResolveEnded(std::list<std::shared_ptr<resolver_ctx>>::iterator& node);

    /*inline*/ bool DispatchEvent(int32 id, void* param);
    /*inline*/ void SetFlag(uint32 flag, bool state);
    /*inline*/ bool TestFlag(uint32 flag);
    /*inline*/ bool TestFlagAny(uint32 flag);
    /*inline*/ void IoInc();
    /*inline*/ bool IoDec();
    /*inline*/ void IoWaitInc();
    /*inline*/ bool IoWaitDec();
    /*inline*/ void IoWriteInc();
    /*inline*/ bool IoWriteDec();
    
private:
    asio::ip::tcp::socket m_socket;
    std::atomic<std::size_t> m_generation;
    std::atomic<std::size_t> m_docCnt; // Disconnect On Complete counter
    std::atomic<std::size_t> m_ioCnt;
    std::atomic<std::size_t> m_ioWaitCnt;
    std::atomic<std::size_t> m_ioWriteCnt;
    uint64 m_netaddr;
    std::recursive_mutex m_mutex;
    std::list<std::shared_ptr<CTcpActWrite>> m_listActWrite;
    CTcpActRead m_actRead;
    CTcpActConnect m_actConnect;
    CTcpActDisconnect m_actDc;
    std::atomic<uint16> m_flags;
    std::chrono::milliseconds m_readTimeout;
    asio::steady_timer m_waitTimer;
    asio::error_code m_waitErrcode;
    std::size_t m_waitBytes;
    std::unique_ptr<acceptor_ctx> m_pAcceptor;
    std::list<std::shared_ptr<resolver_ctx>> m_listResolverNode;
};

inline bool CTcpTransport::DispatchEvent(int32 id, void* param) {
    return SendEvent(id, param);
};

inline void CTcpTransport::SetFlag(uint32 flag, bool state) {
    (state ? m_flags.fetch_or(flag) : m_flags.fetch_and(~flag));
};

inline bool CTcpTransport::TestFlag(uint32 flag) {
    return ((m_flags.load() & flag) == flag);
};

inline bool CTcpTransport::TestFlagAny(uint32 flag) {
    return ((m_flags.load() & flag) != 0);
};

inline void CTcpTransport::IoInc() {
    if (!m_ioCnt++)
        SessionRefInc();
};

inline bool CTcpTransport::IoDec() {
    ASSERT(m_ioCnt > 0);
    if (!--m_ioCnt) {
        SessionRefDec();    
        return true;
    };
    return false;
};

inline void CTcpTransport::IoWaitInc(){
    ++m_ioWaitCnt;
};

inline bool CTcpTransport::IoWaitDec(){
    ASSERT(m_ioWaitCnt > 0);
    return (--m_ioWaitCnt == 0);
};

inline void CTcpTransport::IoWriteInc(){
    ++m_ioWriteCnt;
};

inline bool CTcpTransport::IoWriteDec(){
    ASSERT(m_ioWriteCnt > 0);
    return (--m_ioWriteCnt == 0);
};