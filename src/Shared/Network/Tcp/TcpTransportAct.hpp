#pragma once

#include "TcpTypedefs.hpp"


class CTcpAct
{
public:
    enum type {
        type_connect = 0,
        type_read,
        type_write,
        type_disconnect,
        type_accept,
    };

public:
    inline CTcpAct(type t) :m_type(t) {};
    inline virtual ~CTcpAct() {};
    inline type get_type() const { return m_type; };

private:
    type m_type;
};


class CTcpActConnect final : public CTcpAct
{
public:
    inline CTcpActConnect() : CTcpAct(type_connect) , m_timeStart() , m_timeEnd() {};
    inline void start() { m_timeStart = std::chrono::steady_clock::now(); };
    inline void complete() { m_timeEnd = std::chrono::steady_clock::now(); };
    inline uint32 elapsed_ms() const { return uint32(std::chrono::duration_cast<std::chrono::milliseconds>(m_timeEnd - m_timeStart).count()); };

private:
    std::chrono::steady_clock::time_point m_timeStart;
    std::chrono::steady_clock::time_point m_timeEnd;
};


class CTcpActRead final : public CTcpAct
{
public:
    inline CTcpActRead() : CTcpAct(type_read) {};

#ifndef TCP_ASIO_ZERO_RECV
    inline char* get_buffer() { return m_buffer; };
    inline std::size_t get_buffer_size() { return sizeof(m_buffer); };
    
private:
    char m_buffer[4096];  
#endif        
};


class CTcpActWrite final : public CTcpAct
{
public:
    enum state : char {
        state_await = 0,
        state_process,
        state_complete,
    };

public:
    inline static constexpr std::size_t buffer_size() {
        return sizeof(m_buffer);
    };
    
    inline CTcpActWrite(const char* data, std::size_t size, std::size_t generation, bool flagDOC)
    : CTcpAct(type_write)
    , m_buffer()
    , m_size(size)
    , m_pos(0u)
    , m_gen(generation)
    , m_state(state_await)
    , m_flagDOC(flagDOC) {
        ASSERT(size <= sizeof(m_buffer));
        std::memcpy(m_buffer, data, size);
    };    

    inline void start() {
        m_state = state_process;
    };

    inline void complete(std::size_t bytes) {
        m_pos += bytes;
        ASSERT(m_pos <= m_size);
        if (m_pos == m_size)
            m_state = state_complete;
    };

    inline state get_state() const { return m_state; };
    inline bool is_doc_node(std::size_t gen) const { return (m_flagDOC && (m_gen == gen)); };
    inline std::size_t get_generation() const { return m_gen; };
    inline const char* get_buffer() const { return m_buffer; };
    inline const char* get_buffer_pos_ptr() const { return &m_buffer[m_pos]; };
    inline std::size_t get_buffer_pos_bytes() const { return m_size - m_pos; };

private:
    char m_buffer[4096];
    std::size_t m_size;
    std::size_t m_pos;
    std::size_t m_gen;
    state m_state;
    bool m_flagDOC;
};


class CTcpActDisconnect final : public CTcpAct
{
public:
    inline CTcpActDisconnect() : CTcpAct(type_disconnect) {};
};


class CTcpActAccept final : public CTcpAct
{
public:
    inline CTcpActAccept(asio::io_context& ioctx) : CTcpAct(type_accept), m_socket(ioctx) {};
    inline asio::ip::tcp::socket& socket() { return m_socket; };
    inline virtual ~CTcpActAccept() {};

private:
    asio::ip::tcp::socket m_socket;
};
