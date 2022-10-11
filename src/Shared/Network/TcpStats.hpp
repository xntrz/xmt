#pragma once

void TcpStatsInitialize(void);
void TcpStatsTerminate(void);
struct TcpLayer_t* TcpStatsCreate(void* Param);
void TcpStatsDestroy(struct TcpLayer_t* TcpLayer);
void TcpStatsClear(void);
void TcpStatsGrab(
    uint64* LastSecBytesRecvAccum,
    uint64* LastSecBytesSentAccum,
    uint64* LastSecBytesRecv,
    uint64* LastSecBytesSent,
    uint64* TotalBytesRecv,
    uint64* TotalBytesSent
);