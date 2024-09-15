#include "ProxyList.hpp"

#include "Shared/File/File.hpp"
#include "Shared/Network/NetAddr.hpp"


bool proxylist::open(const std::string& path)
{
    bool bResult = false;
    
    HOBJ hPhyFile = FileOpen(path.c_str(), "rb");
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


bool proxylist::save(const std::string& path)
{
    bool bResult = false;

    HOBJ hPhyFile = FileOpen(path.c_str(), "wb");
    if (hPhyFile)
    {
        int32 nWritePos = 0;
        char* pBuff = new char[(64 + 1) * m_netaddrs.size()];
        if (pBuff)
        {
            for (int32 i = 0; i < int32(m_netaddrs.size()); ++i)
            {
                char NetAddrStr[64];
                NetAddrStr[0] = '\0';
                
                NetAddrToString(&m_netaddrs[i], NetAddrStr, sizeof(NetAddrStr), ':');

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