#pragma once


DLLSHARED void UserAgentInitialize(void);
DLLSHARED void UserAgentTerminate(void);
DLLSHARED void UserAgentRead(const char* Path, int32 RcId = -1);
DLLSHARED const char* UserAgentGenereate(void);