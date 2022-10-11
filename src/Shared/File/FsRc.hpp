#pragma once

bool RcFsOpen(void);
void RcFsClose(void);
const char* RcFsBuildPath(HMODULE hModule, int32 RcId);
const char* RcFsBuildPathEx(HMODULE hModule, int32 RcId, const char* RcType);