#include "TcpSsl.hpp"
#include "TcpLayer.hpp"

#include "../NetSsl.hpp"
#include "../NetAddr.hpp"


CTcpSsl::CTcpSsl(CTcpSession& session, int32 label)
: CTcpLayer(session, label)
, m_pBioR(nullptr)
, m_pBioW(nullptr)
, m_pCtx(nullptr)
, m_netaddr()
, m_elapsed(0u)
, m_hostname()
, m_flags(0u)
{
    NetAddrInit(&m_netaddr);
};


CTcpSsl::~CTcpSsl()
{
    CtxDestroy();
};


bool CTcpSsl::EventProc(int32 id, void* param)
{
    bool bResult = false;

    switch (id)
    {
    case evttype_connect:
        {    
            evt_connect& evt = *reinterpret_cast<evt_connect*>(param);

            /* create ssl context for remote only connections */
            if (evt.remote)
                CtxCreate();

            /* cache event params for notify when handshake complete */
            NetAddrInit(&m_netaddr, evt.ip, evt.port);
            m_elapsed = evt.elapsed;

            /* initiate handshake depending on side */
            bResult = HandshakeInitiate();
        }
        break;

    case evttype_read:
        {
            evt_read& evt = *reinterpret_cast<evt_read*>(param);
            bResult = Read(evt.buffer, evt.bytes);
        }
        break;
        
    case evttype_dc:
        {
            if (FlagTest(flag_handshake_complete)) {
                /* Dispatch disconnect event as usual */
                bResult = SendEvent(id, param);
            }
            else {
                /* We got dc but handshake is not completed - morph disconnect event to connect fail event */
                bResult = SendEvent(evttype_connect_fail, &evt_connect_fail(NetAddrIp(&m_netaddr), NetAddrPort(&m_netaddr), 0, Label()));
            };
        }
        break;

	default:
		bResult = SendEvent(id, param);
		break;
    };

    return bResult;
};


bool CTcpSsl::CmdProc(int32 id, void* param)
{
    bool bResult = false;

    switch (id) {
    case cmdtype_connect:
        {
            /* init per connect data */
            CtxDestroy();
            CtxCreate();
            m_flags = 0;
            m_elapsed = 0;

            /* start connect */
            bResult = SendCmd(id, param);
        }
        break;

    case cmdtype_write:
        {
            if (!FlagTest(flag_handshake_complete))
                break;

            /*  Intercept write cmd and slice it to buffsize_write chunks */
            cmd_write& cmd = *reinterpret_cast<cmd_write*>(param);

            std::size_t size = cmd.size;
            const void* data = cmd.data;

            std::size_t chunks = 0;
            std::size_t written = 0;
            bool doc = false;

            while (size) {
                std::size_t bytes = std::min(size, buffsize_write);

                /* apply flag only to a last chunk */
                if (!(size - bytes))
                    doc = cmd.doc;

                if (Write(&(reinterpret_cast<const char*>(data))[written], bytes, doc)) {
                    bResult = true;
                    written += bytes;
                    size -= bytes;
                }
                else {
                    bResult = false;
                    break;
                };
            };

            if (bResult) {
                cmd.chunks = chunks;
                cmd.written = written;
            };
        }
        break;

    case cmdtype_dc:
        {
            bResult = SendCmd(id, param);
            if (bResult)
                FlagSet(flag_rq_dc, true);
        }
        break;

    case cmdtype_sethostname:
        {
            cmd_sethostname& cmd = *reinterpret_cast<cmd_sethostname*>(param);
            m_hostname = cmd.hostname;
            bResult = true;
        }
        break;

    default:
        bResult = SendCmd(id, param);
        break;
    };

    return bResult;
};


bool CTcpSsl::HandshakeInitiate()
{
    if (IsRemote())
        SSL_set_accept_state(m_pCtx);
    else
        SSL_set_connect_state(m_pCtx);

    return Read();
};


bool CTcpSsl::HandshakeProcess()
{
    return Write();
};


void CTcpSsl::HandshakeProc()
{
    if (SSL_is_init_finished(m_pCtx)) {
        FlagSet(flag_handshake_complete, true);
        if (SendEvent(evttype_connect, &evt_connect(NetAddrIp(&m_netaddr), NetAddrPort(&m_netaddr), m_elapsed, IsRemote()))) {
            /* one more for remote side only */
            if (IsRemote())
                HandshakeProcess();
        }
        else {
            SendCmd(cmdtype_dc, &cmd_dc());
        };
    }
    else {
        HandshakeProcess();
    };
};


bool CTcpSsl::CtxCreate()
{
    if (!m_pCtx) {
        ASSERT(!m_pCtx);
        ASSERT(!m_pBioR);
        ASSERT(!m_pBioW);
        
        m_pCtx = CNetSsl::Alloc();
        ASSERT(m_pCtx);
        
        m_pBioR = BIO_new(BIO_s_mem());
        ASSERT(m_pBioR);
        
        m_pBioW = BIO_new(BIO_s_mem());
        ASSERT(m_pBioW);

        BIO_set_read_buffer_size(m_pBioR, buffsize_read);
        BIO_set_write_buffer_size(m_pBioW, buffsize_write);

        SSL_set_bio(m_pCtx, m_pBioR, m_pBioW);
    };

    if (!m_hostname.empty()) {
        if (m_pCtx)
            SSL_set_tlsext_host_name(m_pCtx, m_hostname.c_str());
    };

    return true;
};


void CTcpSsl::CtxDestroy()
{
    if (m_pCtx) {
        CNetSsl::Shutdown(m_pCtx);
        CNetSsl::Free(m_pCtx);

        m_pBioR = nullptr;
        m_pBioW = nullptr;
        m_pCtx = nullptr;
    };
};


bool CTcpSsl::Read(void* buffer /*= nullptr*/, std::size_t size /*= 0*/)
{
    int32 result = 0;
    int32 bytes = 0;

    if (size) {
        bytes = BIO_write(m_pBioR, buffer, int(size));
        result = CNetSsl::GetError(m_pCtx, bytes);
        if ((bytes != int32(size)) && (!BIO_should_retry(m_pBioR))) {
            ASSERT(false);
            return false;
        };
    };

    char buff[buffsize_read];
    int32 readed = 0;
    
    do {
        bytes = SSL_read(m_pCtx, buff, sizeof(buff));
        result = CNetSsl::GetError(m_pCtx, bytes);
        if (bytes > 0) {
            readed += bytes;
            if (!SendEvent(evttype_read, &evt_read(buff, bytes)))
                return false;

            /* since we notifying all decrypted data in a loop user can request dc between read events */
            if (FlagTest(flag_rq_dc))
                return false;
        }
        else if (CNetSsl::IsFatalError(result)) {
            return false;
        }
        else if (CNetSsl::IsEof(result)) {
            ASSERT(bytes == 0);
            return true;
        };

        if (!FlagTest(flag_handshake_complete))
            HandshakeProc();
    } while (bytes > 0);

    return true;
};


bool CTcpSsl::Write(const void* data /*= nullptr*/, std::size_t size /*= 0*/, bool doc /*= false*/)
{
    int32 result = 0;
    int32 bytes = 0;

    if (size) {
        bytes = SSL_write(m_pCtx, data, int(size));
        result = CNetSsl::GetError(m_pCtx, bytes);
        if ((bytes != int32(size)) || CNetSsl::IsFatalError(result)) {
            ASSERT(false);
            return false;
        };
    };

    char buff[buffsize_write];
    int32 written = 0;
    
    while (BIO_pending(m_pBioW)) {
        bytes = BIO_read(m_pBioW, buff + written, int(sizeof(buff)) - written);
        result = CNetSsl::GetError(m_pCtx, bytes);
        if (CNetSsl::IsFatalError(result)) {
            ASSERT(false);
            return false;
        }
        else {
            written += bytes;
        };

        if (written) {
            if (!SendCmd(cmdtype_write, &cmd_write(buff, written, doc)))
                return false;
            written = 0;
        };
    };

    return true;
};