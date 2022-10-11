#pragma once


class CProxyList
{
public:
    CProxyList(void);
    ~CProxyList(void);
    bool open(const char* Path);
    bool save(const char* Path);
    void insert(uint64 NetAddr);
    void clear(void);
    uint64 addr_at(int32 idx) const;    
    int32 count(void) const;
    void dbgprint(void);

private:
    std::vector<uint64> m_vecNetAddr;
};