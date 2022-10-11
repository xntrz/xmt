#pragma once


void BnguInitialize(void);
void BnguPreTerminate(void);
void BnguPostTerminate(void);
bool BnguIsChannelExist(const char* ChannelName);
bool BnguIsChannelLive(const char* ChannelName);
int32 BnguGetChannelViewerCount(const char* ChannelName);
std::string BnguGetHostForMe(void);