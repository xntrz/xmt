#pragma once

struct TcpAct_t;

void TcpTmoutInitialize(void);
void TcpTmoutTerminate(void);
void TcpTmoutRegist(TcpAct_t* Act);
void TcpTmoutRemove(TcpAct_t* Act);