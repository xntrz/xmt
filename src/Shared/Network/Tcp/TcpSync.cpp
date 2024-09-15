#include "TcpSync.hpp"


CTcpSync::CTcpSync(CTcpSession& session, int32 label)
: CTcpLayer(session, label)
, m_evt()
, m_mutex()
, m_buffSession()
, m_buffRead()
, m_statusConnect(0)
, m_statusDc(0)
, m_bConnectResult(false)
{
    ;
};


CTcpSync::~CTcpSync()
{
    ;
};


bool CTcpSync::EventProc(int32 id, void* param)
{
    switch (id) {
    case evttype_accept:
        {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_buffSession.push_back((*reinterpret_cast<evt_accept*>(param)).session);
            }
            m_evt.signal_once();
        }
        break;

    case evttype_read:
        {
            evt_read& evt = *reinterpret_cast<evt_read*>(param);
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                char* begin = reinterpret_cast<char*>(evt.buffer);
                char* end = reinterpret_cast<char*>(evt.buffer) + evt.bytes;
                m_buffRead.insert(m_buffRead.end(), begin, end);
            }
            m_evt.signal_once();
        }
        break;

    case evttype_connect:
        if (IsRemote()) {
            m_buffRead.reserve(128 * 128);  // 16k
        }
        else {
            ++m_statusConnect;
            m_bConnectResult = true;
            m_evt.signal_once();
        };
        break;

    case evttype_connect_fail:
        ++m_statusConnect;
        m_bConnectResult = false;
        m_evt.signal_once();
        break;

    case evttype_dc:
        ++m_statusDc;
        m_evt.signal_once();
        break;

	case evttype_write:
		break;

    default:
        ASSERT(false);
        break;
    };

    return true;
};


bool CTcpSync::CmdProc(int32 id, void* param)
{
    bool bResult = false;
    
    switch (id) {
    case cmdtype_connect:
        {
            /* remote connections cant connect */
            if (IsRemote())
                break;

            /* session buffer is not needed for connect */
            m_buffRead.reserve(256 * 128);  // 32k
            m_buffSession.shrink_to_fit();

            /* start connect and wait for completion */
            if (SendCmd(id, param)) {
                m_evt.wait([this]() {
                    return (m_statusConnect > 0);
                });
                
                ASSERT(m_statusConnect > 0);
                --m_statusConnect;
                
                bResult = m_bConnectResult;
            };
        }
        break;

    case cmdtype_listen:
        {
            /* remote connections cant listen */
            if (IsRemote())
                break;

            /* read buffer is not needed for listen */
            m_buffRead.shrink_to_fit();
            m_buffSession.reserve(2048);

            /* start listen */
            bResult = SendCmd(id, param);
        }
        break;
		
    case cmdtype_dc:
        {
            if (SendCmd(id, param)) {
                m_evt.wait([this]() {
                    return (m_statusDc > 0);
                });
                
                ASSERT(m_statusDc > 0);
                --m_statusDc;
                
                bResult = true;
            };
        }
        break;

    case cmdtype_accept:
        {
            /* remote connections cant accept */
            if (IsRemote())
                break;

            cmd_accept& cmd = *reinterpret_cast<cmd_accept*>(param);

            /**
             *  we try to lookup session buffer for new connections under mutex lock
             *  if there is empty unlock mutex and await until buffer will contain something new
             */
            do {
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    if (!m_buffSession.empty()) {
                        cmd.session = m_buffSession.front();
                        m_buffSession.erase(m_buffSession.begin());
                        bResult = true;
                        break;
                    };
                }

                /* wait until new session */
                m_evt.wait([this]() {
                    return !m_buffSession.empty();
                });
                
            } while (true);
        }
        break;

    case cmdtype_read:
        {
            cmd_read& cmd = *reinterpret_cast<cmd_read*>(param);

            /**
             *  same as accept copy data if there is some under mutex
             *  otherwise unlock and wait for it
             */
            do {
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    if (!m_buffRead.empty()) {
                        /* grab enough data to fit user buffer */
                        std::size_t size = std::min(cmd.buffsize, m_buffRead.size());
                        std::memcpy(cmd.buff, m_buffRead.data(), size);
                        
                        /* remove processed data from buffer */
                        m_buffRead.erase(m_buffRead.begin(), m_buffRead.begin() + size);
                        
                        /* set how much we got and return success */
                        cmd.readed = size;
                        bResult = true;
                        break;
                    };
                }

                /* wait until recv complete OR dc happens */
                m_evt.wait([this]() {
                    return (!m_buffRead.empty()) || (m_statusDc > 0);
                });
                
                /* handle case if we were disconnected during recv */
                if (m_statusDc > 0) {
                    --m_statusDc;
                    break;
                };
                
            } while (true);
        }
        break;

    default:
		bResult = SendCmd(id, param);
		break;
    };

    return bResult;
};