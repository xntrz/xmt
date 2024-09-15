#pragma once

#include "TcpLayer.hpp"

#include "Shared/Common/Event.hpp"


/**
 *  Layer provides sync mode for tcp connections
 */
class CTcpSync final : public CTcpLayer
{
public:
    enum cmdtype {
        cmdtype_accept = cmdtype_extend,
        cmdtype_read,
    };

    struct cmd_accept {
        CTcpSession* session;

        inline cmd_accept()
            : session(nullptr) {};
    };

    struct cmd_read {
        void* buff;
        std::size_t buffsize;
        std::size_t readed;
        
        inline cmd_read(void* _buff, std::size_t _buffsize)
            : buff(_buff), buffsize(_buffsize), readed(0u) {};
    };

public:
    CTcpSync(CTcpSession& session, int32 label);
    virtual ~CTcpSync();
    virtual bool EventProc(int32 id, void* param) override;
    virtual bool CmdProc(int32 id, void* param) override;

private:
    CEvent m_evt;
    std::mutex m_mutex;
    std::vector<CTcpSession*> m_buffSession;
    std::vector<char> m_buffRead;
    std::atomic<int16> m_statusConnect;
    std::atomic<int16> m_statusDc;
    bool m_bConnectResult;
};