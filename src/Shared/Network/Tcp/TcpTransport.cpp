#include "TcpTransport.hpp"
#include "TcpSession.hpp"

#include "../NetAddr.hpp"


/* minimum timeout value for all IOs with timeout support (currently its: read, resolve, connect) */
static const std::chrono::milliseconds TIMEOUT_MIN(1000);


#ifdef _DEBUG
#define TcpTransportErrOutputLn(format, ...)    \
    do {                                        \
        OUTPUTLN(format, __VA_ARGS__);          \
    } while (0)
#else
#define TcpTransportErrOutputLn(format, ...)    \
    ((void)(0))
#endif


struct CTcpTransport::resolver_ctx : public std::enable_shared_from_this<resolver_ctx>
{
public:    
    resolver_ctx(CTcpTransport& layer, std::list<std::shared_ptr<resolver_ctx>>::iterator& node)
        : m_layer(layer), m_socket(m_layer.IoService().ctx()), m_timer(m_layer.IoService().ctx()), m_ec(), m_it(), m_ioCnt(0), m_node(node) {};

    ~resolver_ctx() = default;

    void resolve_async(const std::string& hostname, std::chrono::milliseconds timeout)
    {
        m_layer.SessionRefInc();

        std::string host(hostname);
        std::transform(host.begin(), host.end(), host.begin(), std::tolower);
        if (host == "localhost")
            host = asio::ip::host_name();

        m_ioCnt += 2u;

        m_timer.expires_from_now(std::max(TIMEOUT_MIN, timeout));
        m_timer.async_wait(std::bind(&resolver_ctx::on_timeout, shared_from_this(), std::placeholders::_1));

        m_socket.async_resolve(host, "", std::bind(&resolver_ctx::on_wait_complete, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    };

    void close() {
        m_socket.cancel();
    };

    void on_timeout(const asio::error_code& ec) {
        if (ec == asio::error::operation_aborted) {
            ;
        }
        else if (ec) {
            if (!m_ec)
                m_ec = ec;
            m_socket.cancel();
        }
        else {
            if (!m_ec)
                m_ec = asio::error::timed_out;
            m_socket.cancel();
        };

        if (!--m_ioCnt)
            on_complete(m_ec, m_it);
    };
    
    void on_wait_complete(const asio::error_code& ec, asio::ip::tcp::resolver::iterator it) {
        m_timer.cancel();
        m_it = it;

        if (ec) {
            if (!m_ec)
                m_ec = ec;
        };

        if (!--m_ioCnt)
            on_complete(m_ec, m_it);
    };
    
    void on_complete(const asio::error_code& ec, asio::ip::tcp::resolver::iterator it) {
        std::vector<uint32> addresses;
        if (!ec) {
            addresses.reserve(16);
            asio::ip::tcp::resolver::iterator itEnd;
            while (it != itEnd) {
                if ((*it).endpoint().address().is_v4())
                    addresses.push_back((*it).endpoint().address().to_v4().to_uint());
                ++it;
            };
        };

        m_layer.SendEvent(evttype_resolve, &evt_resolve(addresses, ec.value()));
        //m_layer.ResolveEnded(m_node);
        m_layer.SessionRefDec();
    };

private:
    CTcpTransport& m_layer;
    asio::ip::tcp::resolver m_socket;
    asio::steady_timer m_timer;
    asio::error_code m_ec;
    asio::ip::tcp::resolver::iterator m_it;
    std::atomic<std::size_t> m_ioCnt;
    std::list<std::shared_ptr<resolver_ctx>>::iterator& m_node;
};


struct CTcpTransport::acceptor_ctx
{
    inline acceptor_ctx(asio::io_context& ioctx, int32 cnt)
    : socket(ioctx)
    , acts() {
        acts.resize(cnt);
		std::for_each(acts.begin(), acts.end(), [&](std::shared_ptr<CTcpActAccept>& ptr) {
			ptr = std::make_shared<CTcpActAccept>(ioctx);
		});
    };

    asio::ip::tcp::acceptor socket;
    std::vector<std::shared_ptr<CTcpActAccept>> acts;
};


CTcpTransport::CTcpTransport(CTcpSession& session, int32 label)
: CTcpLayer(session, label)
, m_socket(IoService().ctx())
, m_generation(0u)
, m_docCnt(0u)
, m_ioCnt(0)
, m_ioWaitCnt(0)
, m_ioWriteCnt(0u)
, m_netaddr()
, m_mutex()
, m_listActWrite()
, m_actRead()
, m_actConnect()
, m_actDc()
, m_flags(0u)
, m_readTimeout(0u)
, m_waitTimer(IoService().ctx())
, m_waitErrcode()
, m_waitBytes(0u)
, m_pAcceptor(nullptr)
, m_listResolverNode()
{
    NetAddrInit(&m_netaddr);
    
    SetFlag(flag_ctrl_nodelay, true);
};


CTcpTransport::~CTcpTransport()
{
    ;
};


bool CTcpTransport::EventProc(int32 id, void* param)
{
    return true;
};


bool CTcpTransport::CmdProc(int32 id, void* param)
{
    bool bResult = false;

    switch (id) {
    /**
     *  COMMMON COMMANDS START
     */
    case cmdtype_connect:
        {
            cmd_connect& cmd = *reinterpret_cast<cmd_connect*>(param);
            bResult = Connect(cmd.ip, cmd.port, std::chrono::milliseconds(cmd.timeout));
        }
        break;

    case cmdtype_cancel_connect:
        {
            cmd_cancel_connect& cmd = *reinterpret_cast<cmd_cancel_connect*>(param);
            bResult = CancelConnect();
        }
        break;

    case cmdtype_listen:
        {
            cmd_listen& cmd = *reinterpret_cast<cmd_listen*>(param);
            bResult = Listen(cmd.ip, cmd.port, cmd.backlog);
        }
        break;

    case cmdtype_write:
        {
            cmd_write& cmd = *reinterpret_cast<cmd_write*>(param);
            cmd.chunks = Write(cmd.data, cmd.size, cmd.doc);
            bResult = (cmd.chunks > 0u);
            if (bResult)
                cmd.written = cmd.size;
        }
        break;

    case cmdtype_dc:
        {
            cmd_dc& cmd = *reinterpret_cast<cmd_dc*>(param);
            bResult = Disconnect();
        }
        break;

    /**
     *  PRIVATE COMMANDS START
     */
    case cmdtype_resolve:
        {
            cmd_resolve& cmd = *reinterpret_cast<cmd_resolve*>(param);
            bResult = Resolve(cmd.host, std::chrono::milliseconds(cmd.timeout));
        }
        break;

    case cmdtype_cancel_resolve:
        {
            cmd_cancel_resolve& cmd = *reinterpret_cast<cmd_cancel_resolve*>(param);
            bResult = CancelResolve(cmd.opid);
        }
        break;

    case cmdtype_complete_accept:
        {
            cmd_complete_accept& cmd = *reinterpret_cast<cmd_complete_accept*>(param);
            CompleteConnect(cmd.sock, cmd.flags);
            bResult = true;
        }
        break;

    case cmdtype_check_open:
        {
            cmd_check_open& cmd = *reinterpret_cast<cmd_check_open*>(param);
            bResult = IsOpen();
        }
        break;

    case cmdtype_close:
        {
            cmd_close& cmd = *reinterpret_cast<cmd_close*>(param);
            Close();
            bResult = true;
        }
        break;

    case cmdtype_set_read_timeout:
        {
            cmd_set_param& cmd = *reinterpret_cast<cmd_set_param*>(param);
            m_readTimeout = std::chrono::milliseconds(cmd.integer);
            bResult = true;
        }
        break;

    case cmdtype_set_force_close:
        {
            cmd_set_param& cmd = *reinterpret_cast<cmd_set_param*>(param);
            SetFlag(flag_ctrl_forceclose, cmd.boolean);
            bResult = true;
        }
        break;

    default:
        ASSERT(false);
        break;
    };

    return bResult;
};


void CTcpTransport::IoCompletionProc(const asio::error_code& errcode, std::size_t bytes, CTcpAct& act)
{
    switch (act.get_type()) {
    case CTcpAct::type_accept:
        CompleteAccept(static_cast<CTcpActAccept&>(act), errcode);
        break;
    case CTcpAct::type_connect:
        CompleteConnect(static_cast<CTcpActConnect&>(act), errcode);
        break;
    case CTcpAct::type_read:
        CompleteRead(static_cast<CTcpActRead&>(act), bytes, errcode);
        break;
    case CTcpAct::type_write:
        CompleteWrite(static_cast<CTcpActWrite&>(act), bytes, errcode);
        break;
    case CTcpAct::type_disconnect:
        ASSERT(false, "dc cant be async");
        break;
    default:
        ASSERT(false, "unknown act type %d", act.get_type());
        break;
    };

    SessionRefInc();
    if (IoDec()) {
        if (TestFlag(flag_st_connected)) {
            Close();
            CompleteDisconnect(m_actDc, asio::error_code());
        };
    };
    SessionRefDec();
};


void CTcpTransport::WaitIoCompletionProc(const asio::error_code& errcode, std::size_t bytes, CTcpAct& act)
{
    m_waitTimer.cancel();
    m_waitBytes = bytes;
    
    if (errcode) {
        if (!m_waitErrcode)
            m_waitErrcode = errcode;
    };

    if (IoWaitDec())
        IoCompletionProc(m_waitErrcode, m_waitBytes, act);
};


void CTcpTransport::TimeoutProc(const asio::error_code& errcode, CTcpAct& act)
{
    if (errcode == asio::error::operation_aborted) {
        ;
    }
    else if (errcode) {
        if (!m_waitErrcode)
            m_waitErrcode = errcode;

        asio::error_code err;
        m_socket.cancel(err);
    }
    else {
        if (!m_waitErrcode)
            m_waitErrcode = asio::error::timed_out;

        asio::error_code err;
        m_socket.cancel(err);
    };

    if (IoWaitDec())
        IoCompletionProc(m_waitErrcode, m_waitBytes, act);
};


bool CTcpTransport::Connect(uint32 ip, uint16 port, std::chrono::milliseconds timeout)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!IsOpen()) {
        if (OpenSockForConnect() && SetSockOptBeforeConnect()) {
            StartConnect(ip, port, timeout);
            return true;
        }
        else {
            Close();
        };
    };
    
    return false;
};


bool CTcpTransport::CancelConnect()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (IsOpen() && TestFlag(flag_st_connecting)) {
        asio::error_code errcode;
        m_socket.cancel(errcode);
        return true;
    };    
    return false;
};


bool CTcpTransport::Listen(uint32 ip, uint16 port, int32 backlog)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!IsOpen()) {
        m_pAcceptor = std::make_unique<acceptor_ctx>(IoService().ctx(), backlog);
        if (OpenSockForListen(ip, port, backlog)) {
            std::for_each(m_pAcceptor->acts.begin(), m_pAcceptor->acts.end(), [this](std::shared_ptr<CTcpActAccept>& ptr) {
				StartAccept(*ptr);
			});
            return true;
        }
        else {
            Close();
        };
    };
    
    return false;
};


std::size_t CTcpTransport::Write(const void* data, std::size_t size, bool doc)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (IsOpen() ? CreateSendNode(data, size, doc) : 0u);
};


bool CTcpTransport::Disconnect()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (IsOpen()) {
        StartDisconnect();
        return true;
    };
    return false;
};


bool CTcpTransport::Resolve(const std::string& hostname, std::chrono::milliseconds timeout)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    /* prealloc node */
    m_listResolverNode.push_back(std::shared_ptr<resolver_ctx>());

    /* create & start node */
	auto it = m_listResolverNode.end();
	std::shared_ptr<resolver_ctx> node = std::make_shared<resolver_ctx>(*this, std::ref(it));
    node->resolve_async(hostname, timeout);

    /* save node for completion */
    *std::prev(m_listResolverNode.end()) = node;

    return true;
};


bool CTcpTransport::CancelResolve(std::size_t opid)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (opid == opid_all) {
        std::for_each(m_listResolverNode.begin(), m_listResolverNode.end(), std::mem_fn(&resolver_ctx::close));
        m_listResolverNode.clear();
    }
    else {
        ASSERT(opid >= 0);
        ASSERT(opid < m_listResolverNode.size());
        
        auto it = m_listResolverNode.begin();
        std::advance(it, opid);
        (*it)->close();
        m_listResolverNode.erase(it);
    };
    
    return true;
};


void CTcpTransport::StartAccept(CTcpActAccept& act)
{
    IoInc();

    m_pAcceptor->socket.async_accept(act.socket(), std::bind(&CTcpTransport::IoCompletionProc, this, std::placeholders::_1, 0, std::ref(act)));
};


void CTcpTransport::StartConnect(uint32 ip, uint16 port, std::chrono::milliseconds timeout)
{
    NetAddrInit(&m_netaddr, ip, port);

    SetFlag(flag_st_connecting, true);
    m_actConnect.start();
    
    IoInc();
    IoWaitInc();
    StartTimeout(m_actConnect, std::max(TIMEOUT_MIN, timeout));
    
    m_socket.async_connect(
        asio::ip::tcp::endpoint(asio::ip::address_v4(ip), port),
        std::bind(&CTcpTransport::WaitIoCompletionProc, this, std::placeholders::_1, 0, std::ref(m_actConnect))
    );
};


void CTcpTransport::StartRead()
{
    IoInc();
    IoWaitInc();
    StartTimeout(m_actRead, std::max(TIMEOUT_MIN, m_readTimeout));
    
#ifdef TCP_ASIO_ZERO_RECV
    m_socket.async_read_some(
        asio::null_buffers(),
        std::bind(&CTcpTransport::WaitIoCompletionProc, this, std::placeholders::_1, std::placeholders::_2, std::ref(m_actRead))
    );
#else
    m_socket.async_read_some(
        asio::buffer(m_actRead.get_buffer(), m_actRead.get_buffer_size()),
        std::bind(&CTcpTransport::WaitIoCompletionProc, this, std::placeholders::_1, std::placeholders::_2, std::ref(m_actRead), asio::ip::tcp::resolver::iterator())
    );
#endif   
};


void CTcpTransport::StartWrite()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    IoWriteInc();

    auto it = m_listActWrite.begin();
    while (it != m_listActWrite.end()) {
        std::shared_ptr<CTcpActWrite> Act = (*it);

        if (Act->get_generation() < m_generation) {
            it = m_listActWrite.erase(it);
            continue;
        };
        
        switch (Act->get_state()) {
        case CTcpActWrite::state_complete:
            it = m_listActWrite.erase(it);
            break;

        case CTcpActWrite::state_process:
            ++it;
            break;

        case CTcpActWrite::state_await:
            {
                ++it;

                IoInc();
                IoWriteInc();
                
                Act->start();
                m_socket.async_write_some(
                    asio::buffer(Act->get_buffer_pos_ptr(), Act->get_buffer_pos_bytes()),
                    std::bind(&CTcpTransport::IoCompletionProc, this, std::placeholders::_1, std::placeholders::_2, std::ref(*Act))
                );
            }
            break;

        default:
            ASSERT(false);
            break;
        };
    };

    IoWriteDec();
};


void CTcpTransport::StartDisconnect()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (IsOpen())
        Close();
};


void CTcpTransport::StartTimeout(CTcpAct& act, std::chrono::milliseconds timeout)
{
    IoWaitInc();

    m_waitBytes = 0;
    m_waitErrcode.clear();
    
    m_waitTimer.expires_from_now(timeout);
    m_waitTimer.async_wait(std::bind(&CTcpTransport::TimeoutProc, this, std::placeholders::_1, std::ref(act)));
};


void CTcpTransport::CompleteAccept(CTcpActAccept& act, const asio::error_code& errcode)
{
    if (!errcode) {
        ASSERT(act.socket().is_open() == true);
        CTcpSession* session = OpenRemote(&cmd_complete_accept(act.socket(), m_flags));
        if (session) {
            DispatchEvent(evttype_accept, &evt_accept(session));
            session->LayerRefDec();
        };
        ASSERT(act.socket().is_open() == false);
    };

    if (m_pAcceptor->socket.is_open())
        StartAccept(act);
};


void CTcpTransport::CompleteConnect(asio::ip::tcp::socket& socket, uint16 flags)
{
    ASSERT(IsRemote() == true);
    
    SetFlag(flag_st_connected, true);
    
    /* inherit only ctrl flags from acceptor */
    SetFlag(flags & flag_ctrl_mask, true);

    /* convert asio peer netaddr to net subsystem netaddr */
    AsioEndpointToNetAddr(socket.remote_endpoint(), &m_netaddr);

    /* taking ownership of native socket */
    m_socket = std::move(socket);

    if (SetSockOptBeforeConnect() && SetSockOptAfterConnect()) {
        if (DispatchEvent(evttype_connect, &evt_connect(NetAddrIp(&m_netaddr), NetAddrPort(&m_netaddr), 0, true)))
            StartRead();
    };
};


void CTcpTransport::CompleteConnect(CTcpActConnect& act, const asio::error_code& errcode)
{
    ASSERT(IsRemote() == false);

    m_actConnect.complete();
    
    ASSERT(TestFlag(flag_st_connecting));
    SetFlag(flag_st_connecting, false);
    SetFlag(flag_st_connected, true);

    if (!errcode && SetSockOptAfterConnect()) {
        if (DispatchEvent(evttype_connect, &evt_connect(NetAddrIp(&m_netaddr), NetAddrPort(&m_netaddr), m_actConnect.elapsed_ms(), false)))
            StartRead();
    }
    else {
        Close();
        CleanupPerConnect();
        DispatchEvent(evttype_connect_fail, &evt_connect_fail(NetAddrIp(&m_netaddr), NetAddrPort(&m_netaddr), errcode.value(), Label()));
    };
};


void CTcpTransport::CompleteRead(CTcpActRead& act, std::size_t bytes, const asio::error_code& errcode)
{
    if (errcode)
        return;

#ifdef TCP_ASIO_ZERO_RECV
    ASSERT(bytes == 0, "Not a zero recv = %u", bytes);

    /**
     *  Possible errors with nonblocking receive:
     *      asio::error::eof            -- remote peer gracefully close connection
     *      asio::error::bad_descriptor -- closing socket while dispatching event then trying recv again
     *      asio::error::would_block    -- no more data to read
     */

    asio::error_code err;
    char buff[4096];
    std::size_t size = 0;
    
    do {
        while ((size < sizeof(buff)) && !err) {
            size += m_socket.receive(asio::buffer(buff + size, sizeof(buff) - size), 0, err);
        };

        if (size) {
            if (!DispatchEvent(evttype_read, &evt_read(buff, size))) {
                bytes = 0;
                break;
            }
            else {
                bytes += size;
                size = 0;
            };
        };
    } while (!err);

    if (err == asio::error::eof) {
        bytes = 0;
    };
#else
	if (bytes) {
	    if (!DispatchEvent(evttype_read, &evt_read(m_actRead.get_buffer(), bytes)))
	        bytes = 0;
	};
#endif

    if (bytes)
        StartRead();
};


void CTcpTransport::CompleteWrite(CTcpActWrite& act, std::size_t bytes, const asio::error_code& errcode)
{
    bool bStartWrite = IoWriteDec();
    if (errcode)
        return;

    DispatchEvent(evttype_write, &evt_write(act.get_buffer(), bytes));

    act.complete(bytes);
    if (act.get_state() == CTcpActWrite::state_complete) {
        if (act.is_doc_node(m_generation)) {
            ASSERT(m_docCnt > 0);
            if (!--m_docCnt)
                return;
        };
    };

    /* avoid multiple working threads idling at mutex locking if write list already locked by someone else */
    if (bStartWrite)
        StartWrite();
};


void CTcpTransport::CompleteDisconnect(CTcpActDisconnect& act, const asio::error_code& errcode)
{
    /**
     *  only one worker thread will reach here when io refs end but mutex still required
     *  for avoid race condition with StartWrite() - when sending old data in new connection
     */
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    CleanupPerConnect();
    ASSERT(m_socket.is_open() == false);
    DispatchEvent(evttype_dc, &evt_dc(IsRemote()));
};


std::size_t CTcpTransport::CreateSendNode(const void* data, std::size_t size, bool doc)
{
    /* result of how much data was slices into act write chunks */
    std::size_t writesCnt = 0;
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if ((data == nullptr) || (size == 0) || (m_docCnt > 0))
        return writesCnt;

    std::size_t docCnt = 0;
    std::size_t written = 0;
    bool bStartWrite = (m_listActWrite.empty() == true);

    do {
        ASSERT(written < size);
        std::size_t remain = (size - written);
        std::size_t bytes = std::min(CTcpActWrite::buffer_size(), remain);
        
		const char* data_ptr = &static_cast<const char*>(data)[written];
		std::size_t data_size = bytes;

		std::shared_ptr<CTcpActWrite> act = std::make_shared<CTcpActWrite>(data_ptr, data_size, m_generation, doc);

        written += bytes;
        ++docCnt;
        ++writesCnt;

        m_listActWrite.push_back(act);
    } while (written != size);

    if (doc && (m_docCnt == 0))
        m_docCnt = docCnt;

    if (bStartWrite)
        StartWrite();

    return writesCnt;
};


bool CTcpTransport::IsOpen()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (m_pAcceptor ? m_pAcceptor->socket.is_open() : m_socket.is_open());
};


void CTcpTransport::Close()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    std::for_each(m_listResolverNode.begin(), m_listResolverNode.end(), std::mem_fn(&resolver_ctx::close));

    if (IsOpen())
    {
        asio::error_code ec;
        if (m_pAcceptor)
        {
            m_pAcceptor->socket.cancel(ec);
            m_pAcceptor->socket.close(ec);
        }
        else
        {
            m_socket.shutdown(asio::socket_base::shutdown_both, ec);
            m_socket.close(ec);
        };
    };
};


bool CTcpTransport::SetSockOptBeforeConnect()
{
    ASSERT(IsOpen());

    asio::error_code errcode;
    
    /* set force close depending on flag */
    bool bLinger = (TestFlag(flag_ctrl_forceclose) ? true : false);
    m_socket.set_option(asio::socket_base::linger(bLinger, 0), errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s set force close failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    /* enable nonblocking mode for socket */
    m_socket.non_blocking(true, errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s set nonblocking mode failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    return true;
};


bool CTcpTransport::SetSockOptAfterConnect()
{
    asio::error_code errcode;
    int buffsize = 0;
#ifndef TCP_ASIO_ZERO_RECV
    buffsize = 4096 * 8;
#endif

    /* setup send buffer size */
    m_socket.set_option(asio::ip::tcp::socket::send_buffer_size(buffsize), errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s set send buffer size failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    /* setup recv buffer size */
    m_socket.set_option(asio::ip::tcp::socket::receive_buffer_size(buffsize), errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s set recv buffer size failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    /* setup nagle opt */
    bool bNoDelay = (TestFlag(flag_ctrl_nodelay) ? true : false);
    m_socket.set_option(asio::ip::tcp::no_delay(bNoDelay), errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s set nodelay failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    return true;
};


bool CTcpTransport::OpenSockForConnect()
{
    asio::error_code errcode;
    m_socket.open(asio::ip::tcp::v4(), errcode);
    
#ifdef _DEBUG
    if (errcode)
        TcpTransportErrOutputLn("%s failed: %s", __FUNCTION__, errcode.message().c_str());
#endif
    
    return (errcode.value() == 0);
};


bool CTcpTransport::OpenSockForListen(uint32 ip, uint16 port, int32 backlogHint)
{
    ASSERT(m_pAcceptor);

    asio::error_code errcode;

    m_pAcceptor->socket.open(asio::ip::tcp::v4(), errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s open failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    m_pAcceptor->socket.bind(asio::ip::tcp::endpoint(asio::ip::address_v4(ip), port), errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s bind failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    m_pAcceptor->socket.listen(backlogHint, errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s listen failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    m_pAcceptor->socket.non_blocking(true, errcode);
    if (errcode) {
        TcpTransportErrOutputLn("%s set non blocking failed: %s", __FUNCTION__, errcode.message().c_str());
        return false;
    };

    return true;
};


void CTcpTransport::CleanupPerConnect()
{
    SetFlag(flag_st_mask, false);
    m_docCnt = 0;
    ++m_generation;
};


void CTcpTransport::AsioEndpointToNetAddr(asio::ip::tcp::endpoint& endpoint, uint64* netaddr) const
{
    ASSERT(endpoint.address().is_v4() == true);

    NetAddrInit(netaddr);
    NetAddrIp(netaddr, endpoint.address().to_v4().to_uint());
    NetAddrPort(netaddr, endpoint.port());
};


void CTcpTransport::ResolveEnded(std::list<std::shared_ptr<resolver_ctx>>::iterator& node)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    m_listResolverNode.erase(node);
};