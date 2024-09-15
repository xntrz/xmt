#pragma once

#include "TcpLayer.hpp"

#include "../NetSsl.hpp"


class CTcpSsl final : public CTcpLayer
{
public:
    enum cmdtype {
        cmdtype_sethostname = cmdtype_extend,
    };

    struct cmd_sethostname {
        const std::string& hostname;

        inline cmd_sethostname(const std::string& _hostname)
            : hostname(_hostname) {};
    };

private:
    static const std::size_t buffsize_read   = 4096u;
    static const std::size_t buffsize_write  = 4096u;
    static const std::size_t tls_rec_size    = 16384u;

    enum flag : uint8
    {
        flag_handshake_complete = (1 << 0),
        flag_close_sent         = (1 << 1),
        flag_rq_dc              = (1 << 2),
    };

public:
    CTcpSsl(CTcpSession& session, int32 label);
    virtual ~CTcpSsl();
    virtual bool EventProc(int32 id, void* param) override;
    virtual bool CmdProc(int32 id, void* param) override;

    bool HandshakeInitiate();
    bool HandshakeProcess();
    void HandshakeProc();
    bool CtxCreate();
    void CtxDestroy();
    bool Read(void* buffer = nullptr, std::size_t size = 0);
    bool Write(const void* data = nullptr, std::size_t size = 0, bool doc = false);
    
    /*inline*/ bool FlagTest(uint32 flag) const;
    /*inline*/ bool FlagTestAny(uint32 flag) const;
    /*inline*/ void FlagSet(uint32 flag, bool state);

private:
    BIO* m_pBioR;
    BIO* m_pBioW;
    SSL* m_pCtx;
    uint64 m_netaddr;
    uint32 m_elapsed;
    std::string m_hostname;
    std::atomic<uint8> m_flags;
};

inline bool CTcpSsl::FlagTest(uint32 flag) const {
    return ((m_flags.load() & flag) == flag);
};

inline bool CTcpSsl::FlagTestAny(uint32 flag) const {
    return ((m_flags.load() & flag) != 0);
};

inline void CTcpSsl::FlagSet(uint32 flag, bool state){
    (state ? m_flags.fetch_or(flag) : m_flags.fetch_and(~flag));
};