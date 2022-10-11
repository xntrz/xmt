#include "Fs.hpp"


struct FSysContainer_t
{
    FileSystem_t* FsList;
    FileSystem_t* FsDefault;
    int32 FsNum;
};


static FSysContainer_t FSysContainer;


bool FSysOpen(void)
{
    FSysContainer.FsList = nullptr;
    FSysContainer.FsDefault = nullptr;
    FSysContainer.FsNum = 0;

	return true;
};


void FSysClose(void)
{
    ASSERT(FSysContainer.FsNum == 0);
};


bool FSysRegist(FileSystem_t* Fs, const char* Path, const char* Name)
{
    ASSERT(Fs);
    ASSERT(Path);
    ASSERT(Name);

    if (FSysSearchByName(Name))
        return false;

    Fs->Name = new char[std::strlen(Name) + 1];
    Fs->Path = new char[std::strlen(Path) + 1];

    std::strncpy(Fs->Name, Name, std::strlen(Name) + 1);
    std::strncpy(Fs->Path, Path, std::strlen(Path) + 1);    

    Fs->Next = FSysContainer.FsList;
    FSysContainer.FsList = Fs;

    ++FSysContainer.FsNum;

    return true;
};


void FSysRemove(FileSystem_t* Fs)
{
    ASSERT(FSysSearchByName(Fs->Name));

    ASSERT(FSysContainer.FsNum > 0);
    --FSysContainer.FsNum;

    FileSystem_t* FsNow = FSysContainer.FsList;
    FileSystem_t* FsPrev = nullptr;
    while (FsNow)
    {
        if (FsNow == Fs)
        {
			if (FsPrev)
				FsPrev->Next = FsNow->Next;
			else
				FSysContainer.FsList = FsNow->Next;

            break;
        };

        FsPrev = FsNow;
        FsNow = FsNow->Next;
    };

    if (Fs->Path)
    {        
        delete[] Fs->Path;
        Fs->Path = nullptr;        
    };

    if (Fs->Name)
    {
        delete[] Fs->Name;
        Fs->Name = nullptr;
    };
};


FileSystem_t* FSysSearchByName(const char* Name)
{
    ASSERT(Name);

    FileSystem_t* Fs = FSysContainer.FsList;
    while (Fs)
    {
        if (!std::strcmp(Fs->Name, Name))
            return Fs;

        Fs = Fs->Next;
    };

    return nullptr;
};


FileSystem_t* FSysSearchByPath(const char* Path)
{
	ASSERT(Path);

	int32 PathLen = std::strlen(Path);
	if (PathLen <= 0)
        return FSysContainer.FsDefault;

	char FsPath[MAX_PATH];
	FsPath[0] = '\0';

	for (int32 i = 0; i < PathLen; ++i)
	{
		if (Path[i] == ':')
		{
			std::strncpy(FsPath, Path, i + 1);
			FsPath[i + 1] = '\0';
			break;
		};
	};

    FileSystem_t* Fs = FSysContainer.FsList;
    while (Fs)
    {
        if (!std::strcmp(Fs->Path, FsPath))
            return Fs;        

        Fs = Fs->Next;
    };

    return FSysContainer.FsDefault;
};


void FSysSetDefault(FileSystem_t* Fs)
{
    FSysContainer.FsDefault = Fs;
};


FileSystem_t* FSysGetDefault(void)
{
    return FSysContainer.FsDefault;
};