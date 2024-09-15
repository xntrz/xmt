#pragma once


struct proxylist
{
    bool open(const std::string& path);
    bool save(const std::string& path);

    inline void insert(uint64 netaddr) {
        if (m_netaddrs.empty())
            m_netaddrs.reserve(2048);
        m_netaddrs.push_back(netaddr);
    };

    inline void clear() {
        m_netaddrs.clear();
    };

    inline uint64 at(std::size_t idx) const {
        return m_netaddrs[idx];
    };

    inline std::size_t count() const {
        return m_netaddrs.size();
    };

    inline bool empty() const {
        return m_netaddrs.empty();
    };

    inline void dbg_print() const {
        std::for_each(m_netaddrs.begin(), m_netaddrs.end(), [](uint64 netaddr) {});
    };

private:
    std::vector<uint64> m_netaddrs;
};