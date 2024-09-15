#pragma once

#include "TcpTypedefs.hpp"


class CTcpResolverCache
{
private:
    struct resolve_rec
    {
    public:        
        inline resolve_rec(const std::vector<uint32>& addresses, std::chrono::milliseconds timeout)
        : m_addresses(addresses)
        , m_endTime(std::chrono::steady_clock::now() + timeout) {
            ;
        };

        inline bool is_invalid() const {
            return (std::chrono::steady_clock::now() > m_endTime);
        };

        inline const std::vector<uint32>& addresses() const {
            return m_addresses;
        };

    private:
        std::vector<uint32> m_addresses;
        std::chrono::steady_clock::time_point m_endTime;
    };
    
public:
    inline CTcpResolverCache()
    : m_resolverCache()
    , m_resolverMutex() {
        m_resolverCache.reserve(32);
    };

    inline bool lookup(const std::string& host, std::vector<uint32>& addresses) {
        std::lock_guard<std::mutex> lock(m_resolverMutex);
        auto it = m_resolverCache.find(host);
        if (it != m_resolverCache.end()) {
            resolve_rec& rec = (*it).second;
            if (rec.is_invalid()) {
                m_resolverCache.erase(it);
            }
            else {
                addresses = rec.addresses();
                return true;
            };
        };
        return false;
    };

    inline bool write(const std::string& host, const std::vector<uint32>& addresses, std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(m_resolverMutex);
        auto it = m_resolverCache.find(host);
        if (it == m_resolverCache.end()) {
            m_resolverCache.insert({ host, resolve_rec(addresses, timeout) });
            return true;
        };
        return false;
    };

private:
    std::unordered_map<std::string, resolve_rec> m_resolverCache;
    std::mutex m_resolverMutex;
};