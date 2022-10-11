#include "UserAgent.hpp"
#include "Random.hpp"
#include "Configuration.hpp"

#include "Shared/File/File.hpp"
#include "Shared/File/FsRc.hpp"


static const int32 USERAGENT_MAXLEN = 512;
static const int32 USERAGENT_MAXNUM = (1024 * 8);
static const char* USERAGENT_DEFAULT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36";


struct UserAgentDb_t
{
	char Database[USERAGENT_MAXNUM][USERAGENT_MAXLEN];
	std::atomic<int32> Count;
	std::atomic<int32> Prev;
};


static UserAgentDb_t UserAgentDb;


static void UserAgentAdd(const char* UserAgent)
{
	if (UserAgentDb.Count < COUNT_OF(UserAgentDb.Database))
		std::strcpy(UserAgentDb.Database[UserAgentDb.Count++], UserAgent);
};


static void UserAgentReadFromMemory(char* pBuffer, uint32 uBufferSize)
{
	std::memset(&UserAgentDb, 0x00, sizeof(UserAgentDb));

	char UserAgentBuffer[USERAGENT_MAXLEN];
	uint32 Readed = 0;
	uint32 ReadPos = 0;

	static_assert(USERAGENT_MAXLEN == 512, "update sscanf buf size");

	while (std::sscanf(&pBuffer[ReadPos], "%512[^\n]%n", UserAgentBuffer, &Readed) > 0)
	{
		char* Ptr = std::strrchr(UserAgentBuffer, '\r');
		if (!Ptr)
			Ptr = std::strrchr(UserAgentBuffer, '\n');
		
		if (!Ptr)
			break;
		
		while (Ptr && *Ptr)
			*Ptr = *(Ptr + 1);

		UserAgentAdd(UserAgentBuffer);

		if (UserAgentDb.Count >= COUNT_OF(UserAgentDb.Database))
			break;

		UserAgentBuffer[0] = '\0';
		ReadPos += (Readed + 1);
		if (ReadPos >= uBufferSize)
			break;
	};

	if (UserAgentDb.Count <= 0)
		UserAgentAdd(USERAGENT_DEFAULT);
};


static void UserAgentReadFile(HOBJ hFile)
{
	uint32 FSize = uint32(FileSize(hFile));
	if (!FSize)
		return;

	char* FBuff = new char[FSize + 1];
	ASSERT(FBuff);
	if (FBuff)
	{
		uint32 Readed = FileRead(hFile, FBuff, FSize);
		ASSERT(Readed == FSize);
		if (Readed == FSize)
		{
			FBuff[FSize] = '\0';
			UserAgentReadFromMemory(FBuff, FSize);
		};

		delete[] FBuff;
		FBuff = nullptr;
	};
};


void UserAgentInitialize(void)
{
	UserAgentDb.Count = 0;
	UserAgentDb.Prev = 0;
};


void UserAgentTerminate(void)
{
	;
};


void UserAgentRead(const char* Path, int32 RcId)
{
	if (RcId != -1)
	{
		HOBJ hFile = FileOpen(RcFsBuildPath(CfgGetAppInstance(), RcId), "rb");
		if (hFile)
		{
			UserAgentReadFile(hFile);
			FileClose(hFile);
		};
	};

	HOBJ hFile = FileOpen(Path, "rb");
	if (hFile)
	{
		UserAgentReadFile(hFile);
		FileClose(hFile);
	};
};


const char* UserAgentGenereate(void)
{
	int32 Index = RndInt32(0, UserAgentDb.Count - 1);
	return UserAgentDb.Database[Index];
};