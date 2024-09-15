#include "File.hpp"
#include "Fs.hpp"
#include "FsPhy.hpp"
#include "FsRc.hpp"

#include "Shared/Common/Configuration.hpp"


bool FileInitialize(void)
{
	if (!FSysOpen())
		return false;
	
	if (!PhyFsOpen(CfgGetCurrentDir()))
		return false;
	
	if (!RcFsOpen())
		return false;

	FileSystem_t* Fs = FSysSearchByName("Phy");
	ASSERT(Fs);
	FSysSetDefault(Fs);

	return true;
};


void FileTerminate(void)
{
	FSysSetDefault(nullptr);
	
	RcFsClose();
	PhyFsClose();
	FSysClose();
};


/*DLLSHARED*/ HOBJ FileOpen(const char* Path, const char* Access, void* Param)
{
	FileSystem_t* Fs = FSysSearchByPath(Path);
	if (Fs)
	{
		File_t* File = (File_t*)Fs->Open(Fs, Path, Access, Param);
		if (File)
			File->Fs = Fs;

		return File;
	};

	return 0;
};


/*DLLSHARED*/ void FileClose(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	File->Fs->Close(hFile);
};


/*DLLSHARED*/ uint32 FileRead(HOBJ hFile, char* Buffer, uint32 BufferSize)
{
	File_t* File = (File_t*)hFile;

	return File->Fs->Read(hFile, Buffer, BufferSize);
};


/*DLLSHARED*/ uint32 FileWrite(HOBJ hFile, const char* Buffer, uint32 BufferSize)
{
	File_t* File = (File_t*)hFile;

	return File->Fs->Write(hFile, Buffer, BufferSize);
};


/*DLLSHARED*/ uint64 FileTell(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	return File->Fs->Tell(hFile);
};


/*DLLSHARED*/ void FileSeek(HOBJ hFile, int64 Offset, FileSeek_t Seek)
{
	File_t* File = (File_t*)hFile;

	File->Fs->Seek(hFile, Offset, Seek);
};


/*DLLSHARED*/ void FileSync(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	File->Fs->Sync(hFile);
};


/*DLLSHARED*/ void FileFlush(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	File->Fs->Flush(hFile);
};


/*DLLSHARED*/ bool FileIsEof(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	return File->Fs->IsEof(hFile);
};


/*DLLSHARED*/ uint64 FileSize(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	return File->Fs->Size(hFile);
};


/*DLLSHARED*/ FileStat_t FileStat(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	return File->Fs->Stat(hFile);
};


/*DLLSHARED*/ int32 FileError(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;

	return File->Fs->Error(hFile);
};


/*DLLSHARED*/ bool FileAbort(HOBJ hFile)
{
	File_t* File = (File_t*)hFile;
	
	return File->Fs->Abort(hFile);
};


/*DLLSHARED*/ bool FileExist(const char* Path)
{
	FileSystem_t* Fs = FSysSearchByPath(Path);
	if (Fs)
		return Fs->Exist(Fs, Path);
	else
		return false;
};


/*DLLSHARED*/ bool FileDelete(const char* Path)
{
	FileSystem_t* Fs = FSysSearchByPath(Path);
	if (Fs)
		return Fs->Delete(Fs, Path);
	else
		return false;
};