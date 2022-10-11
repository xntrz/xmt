#include "ProxyList.hpp"

#include "Shared/File/File.hpp"
#include "Shared/Network/NetAddr.hpp"


CProxyList::CProxyList(void)
: m_vecNetAddr()
{
    ;
};


CProxyList::~CProxyList(void)
{
    ;
};


bool CProxyList::open(const char* Path)
{
    bool bResult = false;
    
    HOBJ hPhyFile = FileOpen(Path, "rb");
    if (hPhyFile)
    {
        uint32 uSize = uint32(FileSize(hPhyFile));
        char* pBuff = new char[uSize];
        if (pBuff)
        {
            if (FileRead(hPhyFile, pBuff, uSize) == uSize)
            {
                int32 nReadPos = 0;
				int32 Readed = 0;
                char szLine[128];
                while (std::sscanf(&pBuff[nReadPos], "%128s\r\n%n", szLine, &Readed) > 0)
                {
                    char szIp[64] = { 0 };
                    uint16 nPort = 0;
                    if (std::sscanf(szLine, "%[^:]:%hu", szIp, &nPort) == 2)
                    {
                        if (NetAddrIsValidIp(szIp))
                        {
                            uint64 NetAddr = 0;
                            NetAddrInit(&NetAddr, szIp, nPort);
                            insert(NetAddr);
                            
                            bResult = true;
                        };
                    };

					nReadPos += Readed;
					szLine[0] = '\0';
                };
            };

            delete[] pBuff;
            pBuff = nullptr;
        };

        FileClose(hPhyFile);
        hPhyFile = 0;
    };

    return bResult;
};


bool CProxyList::save(const char* Path)
{
    bool bResult = false;

    HOBJ hPhyFile = FileOpen(Path, "wb");
    if (hPhyFile)
    {
        int32 nWritePos = 0;
        char* pBuff = new char[(64 + 1) * m_vecNetAddr.size()];
        if (pBuff)
        {
            for (int32 i = 0; i < int32(m_vecNetAddr.size()); ++i)
            {
                char NetAddrStr[64];
                NetAddrStr[0] = '\0';
                
                NetAddrToString(&m_vecNetAddr[i], NetAddrStr, sizeof(NetAddrStr), ':');

                int32 nNetAddrStrLen = std::strlen(NetAddrStr);
                std::strncpy(&pBuff[nWritePos], NetAddrStr, nNetAddrStrLen);
                nWritePos += nNetAddrStrLen;
                pBuff[nWritePos++] = '\n';                
            };

            if (nWritePos > 0)
            {
                if (FileWrite(hPhyFile, pBuff, nWritePos) == nWritePos)
                    bResult = true;
            };

            delete[] pBuff;
            pBuff = nullptr;
        };

        FileClose(hPhyFile);
        hPhyFile = 0;
    };

    return bResult;
};


void CProxyList::insert(uint64 NetAddr)
{
    if (m_vecNetAddr.empty())
        m_vecNetAddr.reserve(4096);
    
    m_vecNetAddr.push_back(NetAddr);
};


void CProxyList::clear(void)
{
    m_vecNetAddr.clear();
};


uint64 CProxyList::addr_at(int32 idx) const
{
    ASSERT(idx >= 0 && idx < int32(m_vecNetAddr.size()));
    return m_vecNetAddr[idx];
};


int32 CProxyList::count(void) const
{
    return m_vecNetAddr.size();
};


void CProxyList::dbgprint(void)
{
    for (int32 i = 0; i < int32(m_vecNetAddr.size()); ++i)
    {
        uint64 NetAddr = m_vecNetAddr[i];
        OUTPUTLN("%s", NetAddrToStdString(&NetAddr).c_str());
    };
};