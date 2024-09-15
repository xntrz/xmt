#pragma once

enum NETEVENT
{
    NETEVENT_CONNECT = 0,
    NETEVENT_CONNECTFAIL,
    NETEVENT_CONNECTFAILPRX,
    NETEVENT_CONNECTFAILSSL,
    NETEVENT_RECV,
    NETEVENT_SEND,
    NETEVENT_DISCONNECT,
    NETEVENT_RESOLVE,
};

using HCONN = void*;
using NETEVENTPROC = std::function<bool(HCONN hConn, NETEVENT Event, uint32 Error, uint64 NetAddr, const void* Data, uint32 DataSize, void* Param)>;