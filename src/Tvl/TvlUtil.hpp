#pragma once


void TvluInitialize(void);
void TvluPreTerminate(void);
void TvluPostTerminate(void);
bool TvluIsChannelExist(const char* ChannelName);
bool TvluIsChannelLive(const char* ChannelName);
int32 TvluGetChannelViewersCount(const char* ChannelName);
int32 TvluGetChannelId(const char* ChannelName);
std::string TvluGeneratePVID(void);
std::string TvluGenerateTID(void);
std::string TvluGenerateDeviceId(void);
std::string TvluGenerateQID(void);
bool TvluIsChannelProtected(const char* ChannelName);
bool TvluIsClipExist(const char* ClipId);
int32 TvluGetClipViewsCount(const char* ClipId);
bool TvluIsVodExist(const char* VodId);
int32 TvluGetVodViewsCount(const char* VoidId);